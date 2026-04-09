#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../audio/adpcm.h"

// Manages the audio streaming pipeline over BLE:
// - TX: reads mic, encodes ADPCM, sends via BLE notify
// - RX: receives BLE writes, decodes ADPCM, writes to speaker

class BleAudio {
public:
    void begin();
    void update();  // Call every loop iteration

    // TX control (mic → phone)
    void startTx();     // Called when PTT pressed
    void stopTx();      // Called when PTT released
    bool isTxActive() const { return _txActive; }

    // RX control (phone → speaker)
    void startRx();     // Called when audio stream starts
    void stopRx();      // Called when audio stream stops
    bool isRxActive() const { return _rxActive; }

    // Called by BLE service when audio data arrives from phone
    void onRxData(const uint8_t* data, size_t len);

private:
    void processTx();
    void processRx();

    // TX state
    bool _txActive = false;
    AdpcmState _txState;
    int16_t _txPcmBuffer[ADPCM_FRAME_SAMPLES];
    uint8_t _txAdpcmBuffer[ADPCM_FRAME_TOTAL_BYTES];

    // RX state
    bool _rxActive = false;
    AdpcmState _rxState;
    int16_t _rxPcmBuffer[ADPCM_FRAME_SAMPLES];

    // RX ring buffer for incoming BLE audio frames
    static const int RX_RING_SIZE = 8;
    uint8_t _rxRing[RX_RING_SIZE][ADPCM_FRAME_TOTAL_BYTES];
    volatile int _rxRingHead = 0;
    volatile int _rxRingTail = 0;
    int _rxBuffered = 0;
    bool _rxPlaying = false;
};

extern BleAudio bleAudio;
