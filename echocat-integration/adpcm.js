// ============================================================
// IMA-ADPCM Codec — JavaScript Implementation
// Matches the ESP32 C implementation exactly
// ============================================================

const STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
];

const INDEX_TABLE = [
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
];

function clamp16(val) {
    if (val > 32767) return 32767;
    if (val < -32768) return -32768;
    return val | 0;
}

function clampIndex(idx) {
    if (idx < 0) return 0;
    if (idx > 88) return 88;
    return idx;
}

/**
 * Create a new ADPCM codec state
 */
function createState() {
    return { predicted: 0, stepIndex: 0 };
}

/**
 * Encode PCM Int16Array → ADPCM Uint8Array
 * @param {Int16Array} pcm - Input PCM samples (16-bit signed)
 * @param {object} state - Codec state { predicted, stepIndex }
 * @returns {Uint8Array} Encoded ADPCM frame (4 header + N/2 data bytes)
 */
function encode(pcm, state) {
    const numSamples = pcm.length;
    const dataBytes = Math.ceil(numSamples / 2);
    const output = new Uint8Array(4 + dataBytes);

    // Header: predictor (2 bytes LE) + stepIndex (1 byte) + reserved (1 byte)
    output[0] = state.predicted & 0xFF;
    output[1] = (state.predicted >> 8) & 0xFF;
    output[2] = state.stepIndex;
    output[3] = 0;

    let byteIdx = 0;
    let highNibble = false;

    for (let i = 0; i < numSamples; i++) {
        const step = STEP_TABLE[state.stepIndex];
        let diff = pcm[i] - state.predicted;

        let nibble = 0;
        if (diff < 0) {
            nibble = 8;
            diff = -diff;
        }

        if (diff >= step) { nibble |= 4; diff -= step; }
        const halfStep = step >> 1;
        if (diff >= halfStep) { nibble |= 2; diff -= halfStep; }
        const quarterStep = step >> 2;
        if (diff >= quarterStep) { nibble |= 1; }

        // Decode to update predictor
        const decodeStep = STEP_TABLE[state.stepIndex];
        let delta = (decodeStep >> 3);
        if (nibble & 4) delta += decodeStep;
        if (nibble & 2) delta += (decodeStep >> 1);
        if (nibble & 1) delta += (decodeStep >> 2);
        if (nibble & 8) delta = -delta;

        state.predicted = clamp16(state.predicted + delta);
        state.stepIndex = clampIndex(state.stepIndex + INDEX_TABLE[nibble]);

        if (!highNibble) {
            output[4 + byteIdx] = nibble & 0x0F;
            highNibble = true;
        } else {
            output[4 + byteIdx] |= (nibble << 4);
            byteIdx++;
            highNibble = false;
        }
    }

    return output;
}

/**
 * Decode ADPCM Uint8Array → PCM Int16Array
 * @param {Uint8Array} input - Encoded ADPCM frame (4 header + data)
 * @param {object} state - Codec state { predicted, stepIndex }
 * @returns {Int16Array} Decoded PCM samples (16-bit signed)
 */
function decode(input, state) {
    if (input.length < 4) return new Int16Array(0);

    // Read header
    state.predicted = (input[0] | (input[1] << 8)) << 16 >> 16;  // Sign-extend
    state.stepIndex = clampIndex(input[2]);

    const dataLen = input.length - 4;
    const numSamples = dataLen * 2;
    const pcm = new Int16Array(numSamples);
    let sampleCount = 0;

    for (let byteIdx = 0; byteIdx < dataLen; byteIdx++) {
        for (let nibbleIdx = 0; nibbleIdx < 2; nibbleIdx++) {
            let nibble;
            if (nibbleIdx === 0) {
                nibble = input[4 + byteIdx] & 0x0F;
            } else {
                nibble = (input[4 + byteIdx] >> 4) & 0x0F;
            }

            const step = STEP_TABLE[state.stepIndex];
            let delta = (step >> 3);
            if (nibble & 4) delta += step;
            if (nibble & 2) delta += (step >> 1);
            if (nibble & 1) delta += (step >> 2);
            if (nibble & 8) delta = -delta;

            state.predicted = clamp16(state.predicted + delta);
            state.stepIndex = clampIndex(state.stepIndex + INDEX_TABLE[nibble]);

            pcm[sampleCount++] = state.predicted;
        }
    }

    return pcm.subarray(0, sampleCount);
}

// Export for use in ECHOCAT
if (typeof window !== 'undefined') {
    window.AdpcmCodec = { createState, encode, decode };
}
