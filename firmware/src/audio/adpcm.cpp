#include "adpcm.h"
#include <string.h>

// Standard IMA-ADPCM step size table
const int16_t AdpcmCodec::stepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

// Index adjustment per ADPCM nibble value
const int8_t AdpcmCodec::indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

int16_t AdpcmCodec::clamp16(int32_t val) {
    if (val > 32767) return 32767;
    if (val < -32768) return -32768;
    return (int16_t)val;
}

uint8_t AdpcmCodec::clampIndex(int32_t idx) {
    if (idx < 0) return 0;
    if (idx > 88) return 88;
    return (uint8_t)idx;
}

void AdpcmCodec::resetState(AdpcmState& state) {
    state.predicted = 0;
    state.stepIndex = 0;
    state._reserved = 0;
}

int AdpcmCodec::encode(const int16_t* pcm, int numSamples, uint8_t* output, AdpcmState& state) {
    // Write header: current state (for decoder sync)
    output[0] = state.predicted & 0xFF;
    output[1] = (state.predicted >> 8) & 0xFF;
    output[2] = state.stepIndex;
    output[3] = 0;  // Reserved

    uint8_t* data = output + ADPCM_FRAME_HEADER;
    int byteIdx = 0;
    bool highNibble = false;

    for (int i = 0; i < numSamples; i++) {
        int16_t step = stepTable[state.stepIndex];
        int32_t diff = pcm[i] - state.predicted;

        // Encode difference as 4-bit ADPCM nibble
        uint8_t nibble = 0;
        if (diff < 0) {
            nibble = 8;  // Sign bit
            diff = -diff;
        }

        // Quantize
        if (diff >= step) { nibble |= 4; diff -= step; }
        step >>= 1;
        if (diff >= step) { nibble |= 2; diff -= step; }
        step >>= 1;
        if (diff >= step) { nibble |= 1; }

        // Decode to update predictor (same as decoder does)
        step = stepTable[state.stepIndex];
        int32_t delta = (step >> 3);
        if (nibble & 4) delta += step;
        if (nibble & 2) delta += (step >> 1);
        if (nibble & 1) delta += (step >> 2);
        if (nibble & 8) delta = -delta;

        state.predicted = clamp16(state.predicted + delta);
        state.stepIndex = clampIndex(state.stepIndex + indexTable[nibble]);

        // Pack nibble into byte
        if (!highNibble) {
            data[byteIdx] = nibble & 0x0F;
            highNibble = true;
        } else {
            data[byteIdx] |= (nibble << 4);
            byteIdx++;
            highNibble = false;
        }
    }

    return ADPCM_FRAME_HEADER + byteIdx + (highNibble ? 1 : 0);
}

int AdpcmCodec::decode(const uint8_t* input, int inputLen, int16_t* pcm, AdpcmState& state) {
    if (inputLen < ADPCM_FRAME_HEADER) return 0;

    // Read header
    state.predicted = (int16_t)(input[0] | (input[1] << 8));
    state.stepIndex = clampIndex(input[2]);

    const uint8_t* data = input + ADPCM_FRAME_HEADER;
    int dataLen = inputLen - ADPCM_FRAME_HEADER;
    int sampleCount = 0;

    for (int byteIdx = 0; byteIdx < dataLen; byteIdx++) {
        // Low nibble first, then high nibble
        for (int nibbleIdx = 0; nibbleIdx < 2; nibbleIdx++) {
            uint8_t nibble;
            if (nibbleIdx == 0) {
                nibble = data[byteIdx] & 0x0F;
            } else {
                nibble = (data[byteIdx] >> 4) & 0x0F;
            }

            int16_t step = stepTable[state.stepIndex];
            int32_t delta = (step >> 3);
            if (nibble & 4) delta += step;
            if (nibble & 2) delta += (step >> 1);
            if (nibble & 1) delta += (step >> 2);
            if (nibble & 8) delta = -delta;

            state.predicted = clamp16(state.predicted + delta);
            state.stepIndex = clampIndex(state.stepIndex + indexTable[nibble]);

            pcm[sampleCount++] = state.predicted;
        }
    }

    return sampleCount;
}
