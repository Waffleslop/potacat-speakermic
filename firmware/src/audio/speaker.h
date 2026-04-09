#pragma once

#include <Arduino.h>
#include <driver/i2s_std.h>
#include "../config.h"

class Speaker {
public:
    bool begin();
    void end();

    // Write PCM samples to the speaker via I2S
    // Returns number of samples written
    int writeFrame(const int16_t* buffer, int numSamples);

    // Set volume (0-100)
    void setVolume(uint8_t vol);
    uint8_t getVolume() const { return _volume; }

    bool isRunning() const { return _running; }

private:
    i2s_chan_handle_t _txHandle = nullptr;
    bool _running = false;
    uint8_t _volume = VOLUME_DEFAULT;

    // Apply volume scaling to a buffer (in-place)
    void applyVolume(int16_t* buffer, int numSamples);
};

extern Speaker speaker;
