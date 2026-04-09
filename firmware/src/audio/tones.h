#pragma once

#include <Arduino.h>
#include "../config.h"

class Tones {
public:
    // Play a single tone at given frequency for duration (blocking)
    void playTone(uint16_t freqHz, uint16_t durationMs);

    // Play pre-defined sound effects (blocking)
    void playStartup();
    void playShutdown();
    void playConnected();
    void playDisconnected();
    void playLowBattery();
    void playPttClick();

    // Non-blocking tone support
    void startTone(uint16_t freqHz);
    void stopTone();

private:
    // Generate one frame of sine wave samples and write to speaker
    void generateSine(int16_t* buffer, int numSamples, uint16_t freqHz,
                      float amplitude, uint32_t& phase);
};

extern Tones tones;
