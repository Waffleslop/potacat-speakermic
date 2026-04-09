#pragma once

#include <stdint.h>
#include "../config.h"

// IMA-ADPCM state (4 bytes — transmitted as frame header)
struct AdpcmState {
    int16_t predicted;      // Last predicted sample value
    uint8_t stepIndex;      // Index into step size table (0-88)
    uint8_t _reserved;      // Padding for alignment
};

class AdpcmCodec {
public:
    // Encode 160 PCM samples → 84-byte ADPCM frame (4 header + 80 data)
    // Returns the number of bytes written to output (always ADPCM_FRAME_TOTAL_BYTES)
    static int encode(const int16_t* pcm, int numSamples, uint8_t* output, AdpcmState& state);

    // Decode 84-byte ADPCM frame → 160 PCM samples
    // Returns the number of samples written to output
    static int decode(const uint8_t* input, int inputLen, int16_t* pcm, AdpcmState& state);

    // Reset codec state
    static void resetState(AdpcmState& state);

private:
    static int16_t clamp16(int32_t val);
    static uint8_t clampIndex(int32_t idx);

    // IMA-ADPCM step size table (89 entries)
    static const int16_t stepTable[89];
    // IMA-ADPCM index adjustment table
    static const int8_t indexTable[16];
};
