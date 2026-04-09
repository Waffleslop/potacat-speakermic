#include "tones.h"
#include "speaker.h"
#include <math.h>

Tones tones;

void Tones::generateSine(int16_t* buffer, int numSamples, uint16_t freqHz,
                          float amplitude, uint32_t& phase) {
    float phaseInc = 2.0f * PI * freqHz / AUDIO_SAMPLE_RATE;
    for (int i = 0; i < numSamples; i++) {
        float sample = sinf(phase * phaseInc) * amplitude * 32767.0f;
        buffer[i] = (int16_t)sample;
        phase++;
    }
}

void Tones::playTone(uint16_t freqHz, uint16_t durationMs) {
    if (!speaker.isRunning()) return;

    int16_t buffer[ADPCM_FRAME_SAMPLES];
    uint32_t phase = 0;
    int totalSamples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
    int remaining = totalSamples;

    while (remaining > 0) {
        int count = min(remaining, (int)ADPCM_FRAME_SAMPLES);
        generateSine(buffer, count, freqHz, 0.5f, phase);
        speaker.writeFrame(buffer, count);
        remaining -= count;
    }
}

void Tones::playStartup() {
    // Rising two-tone: 400Hz then 800Hz
    playTone(400, 120);
    playTone(600, 80);
    playTone(800, 150);
}

void Tones::playShutdown() {
    // Falling two-tone: 800Hz then 400Hz
    playTone(800, 120);
    playTone(600, 80);
    playTone(400, 150);
}

void Tones::playConnected() {
    // Short high chirp
    playTone(1000, 60);
    delay(30);
    playTone(1200, 80);
}

void Tones::playDisconnected() {
    // Double low chirp
    playTone(600, 80);
    delay(50);
    playTone(400, 100);
}

void Tones::playLowBattery() {
    // Three short beeps
    for (int i = 0; i < 3; i++) {
        playTone(800, 60);
        delay(80);
    }
}

void Tones::playPttClick() {
    // Very brief tick
    playTone(2000, 10);
}

void Tones::startTone(uint16_t freqHz) {
    // For non-blocking tone, write one frame at a time from loop()
    // This is a simplified version — call from loop for continuous tone
    int16_t buffer[ADPCM_FRAME_SAMPLES];
    uint32_t phase = 0;
    generateSine(buffer, ADPCM_FRAME_SAMPLES, freqHz, 0.5f, phase);
    speaker.writeFrame(buffer, ADPCM_FRAME_SAMPLES);
}

void Tones::stopTone() {
    // Write silence
    int16_t silence[ADPCM_FRAME_SAMPLES] = {0};
    speaker.writeFrame(silence, ADPCM_FRAME_SAMPLES);
}
