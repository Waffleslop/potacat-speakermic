// ============================================================
// POTACAT Speakermic — AudioWorklet Processor
// Captures RX audio from WebRTC, downsamples to 8kHz mono
// for encoding and sending to ESP32 via BLE
// ============================================================

class SpeakermicCaptureProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.buffer = [];
        this.targetSamples = 160;  // ADPCM frame size
        this.inputRate = sampleRate;  // AudioContext sample rate (typically 48000)
        this.outputRate = 8000;
        this.ratio = this.inputRate / this.outputRate;
    }

    process(inputs, outputs, parameters) {
        const input = inputs[0];
        if (!input || !input[0]) return true;

        const channelData = input[0];  // Mono or first channel

        // Downsample: pick every Nth sample (simple decimation)
        // For production, a proper anti-aliasing filter should be applied
        for (let i = 0; i < channelData.length; i += this.ratio) {
            const idx = Math.floor(i);
            if (idx < channelData.length) {
                // Convert float32 (-1..1) to int16 (-32768..32767)
                let sample = channelData[idx];
                sample = Math.max(-1, Math.min(1, sample));
                this.buffer.push(Math.round(sample * 32767));
            }
        }

        // When we have enough samples for one ADPCM frame, send to main thread
        while (this.buffer.length >= this.targetSamples) {
            const frame = new Int16Array(this.buffer.splice(0, this.targetSamples));
            this.port.postMessage({ type: 'pcm-frame', samples: frame }, [frame.buffer]);
        }

        return true;  // Keep processor alive
    }
}

registerProcessor('speakermic-capture', SpeakermicCaptureProcessor);
