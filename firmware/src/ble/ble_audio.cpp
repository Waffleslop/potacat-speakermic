#include "ble_audio.h"
#include "ble_service.h"
#include "../audio/mic.h"
#include "../audio/speaker.h"

BleAudio bleAudio;

void BleAudio::begin() {
    AdpcmCodec::resetState(_txState);
    AdpcmCodec::resetState(_rxState);
    _rxRingHead = 0;
    _rxRingTail = 0;
    _rxBuffered = 0;
    _rxPlaying = false;
}

void BleAudio::update() {
    if (_txActive) processTx();
    if (_rxActive) processRx();
}

// --- TX Path: Mic → ADPCM → BLE Notify ---

void BleAudio::startTx() {
    if (_txActive) return;
    AdpcmCodec::resetState(_txState);
    mic.begin();
    _txActive = true;
    log_i("TX audio started");
}

void BleAudio::stopTx() {
    if (!_txActive) return;
    _txActive = false;
    mic.end();
    log_i("TX audio stopped");
}

void BleAudio::processTx() {
    // Read a frame from the microphone
    int samples = mic.readFrame(_txPcmBuffer, ADPCM_FRAME_SAMPLES);
    if (samples <= 0) return;

    // Encode to ADPCM
    int adpcmLen = AdpcmCodec::encode(_txPcmBuffer, samples, _txAdpcmBuffer, _txState);

    // Send via BLE notification
    bleService.sendAudioFrame(_txAdpcmBuffer, adpcmLen);
}

// --- RX Path: BLE Write → ADPCM → Speaker ---

void BleAudio::startRx() {
    if (_rxActive) return;
    AdpcmCodec::resetState(_rxState);
    _rxRingHead = 0;
    _rxRingTail = 0;
    _rxBuffered = 0;
    _rxPlaying = false;
    speaker.begin();
    _rxActive = true;
    log_i("RX audio started");
}

void BleAudio::stopRx() {
    if (!_rxActive) return;
    _rxActive = false;
    _rxPlaying = false;
    speaker.end();
    log_i("RX audio stopped");
}

void BleAudio::onRxData(const uint8_t* data, size_t len) {
    if (!_rxActive) return;
    if (len > ADPCM_FRAME_TOTAL_BYTES) return;

    // Add to ring buffer
    int nextHead = (_rxRingHead + 1) % RX_RING_SIZE;
    if (nextHead == _rxRingTail) {
        // Buffer full — drop oldest frame
        _rxRingTail = (_rxRingTail + 1) % RX_RING_SIZE;
        _rxBuffered--;
    }

    memcpy(_rxRing[_rxRingHead], data, len);
    _rxRingHead = nextHead;
    _rxBuffered++;
}

void BleAudio::processRx() {
    // Jitter buffer: wait until we have enough frames before starting playback
    int jitterFrames = (SPEAKER_BUFFER_MS / 20);  // 20ms per frame
    if (!_rxPlaying) {
        if (_rxBuffered >= jitterFrames) {
            _rxPlaying = true;
        } else {
            return;
        }
    }

    // Play one frame from the ring buffer
    if (_rxRingHead == _rxRingTail) {
        // Buffer underrun — insert silence
        memset(_rxPcmBuffer, 0, sizeof(_rxPcmBuffer));
        speaker.writeFrame(_rxPcmBuffer, ADPCM_FRAME_SAMPLES);
        return;
    }

    // Decode and play
    int samples = AdpcmCodec::decode(
        _rxRing[_rxRingTail], ADPCM_FRAME_TOTAL_BYTES,
        _rxPcmBuffer, _rxState
    );
    _rxRingTail = (_rxRingTail + 1) % RX_RING_SIZE;
    _rxBuffered--;

    if (samples > 0) {
        speaker.writeFrame(_rxPcmBuffer, samples);
    }
}
