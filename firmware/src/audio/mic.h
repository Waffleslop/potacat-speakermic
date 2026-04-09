#pragma once

#include <Arduino.h>
#include <driver/i2s_pdm.h>
#include "../config.h"

class Mic {
public:
    bool begin();
    void end();

    // Read a frame of PCM samples from the PDM mic
    // Returns number of samples read, or 0 on error
    int readFrame(int16_t* buffer, int maxSamples);

    bool isRunning() const { return _running; }

private:
    i2s_chan_handle_t _rxHandle = nullptr;
    bool _running = false;
};

extern Mic mic;
