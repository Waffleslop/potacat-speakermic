// ============================================================
// POTACAT Speakermic — Web Bluetooth Audio Bridge
// Connects to ESP32 BLE speakermic and bridges audio into
// ECHOCAT's existing WebRTC connection
// ============================================================

const SPEAKERMIC = (() => {
    // BLE UUIDs (must match firmware config.h)
    const SERVICE_UUID      = 'f47ac10b-58cc-4372-a567-0e02b2c3d479';
    const TX_AUDIO_UUID     = 'f47ac10b-58cc-4372-a567-0e02b2c30001';
    const RX_AUDIO_UUID     = 'f47ac10b-58cc-4372-a567-0e02b2c30002';
    const CONTROL_UUID      = 'f47ac10b-58cc-4372-a567-0e02b2c30003';
    const DEVICE_UUID       = 'f47ac10b-58cc-4372-a567-0e02b2c30004';

    // Control commands
    const CMD_PTT       = 0x01;
    const CMD_BATTERY   = 0x02;
    const CMD_STATUS    = 0x03;
    const CMD_VOLUME    = 0x04;
    const CMD_AUDIO     = 0x05;

    // Audio config
    const SAMPLE_RATE = 8000;
    const FRAME_SAMPLES = 160;

    // State
    let device = null;
    let server = null;
    let service = null;
    let txAudioChar = null;
    let rxAudioChar = null;
    let controlChar = null;
    let deviceChar = null;
    let connected = false;

    // Audio state
    let rxDecodeState = null;   // ADPCM state for decoding mic audio from ESP32
    let txEncodeState = null;   // ADPCM state for encoding speaker audio to ESP32
    let audioContext = null;
    let captureWorklet = null;
    let micStreamTrack = null;  // MediaStreamTrack generated from ESP32 mic data
    let originalMicTrack = null;
    let pcSender = null;        // RTCRtpSender for mic track

    // Callbacks
    let onConnect = null;
    let onDisconnect = null;
    let onPtt = null;
    let onBattery = null;

    // --------------------------------------------------------
    // Connection Management
    // --------------------------------------------------------

    async function connect() {
        if (!navigator.bluetooth) {
            throw new Error('Web Bluetooth not supported. Use Chrome (Android) or Bluefy (iOS).');
        }

        try {
            // Prompt user to select device
            device = await navigator.bluetooth.requestDevice({
                filters: [{ services: [SERVICE_UUID] }],
                optionalServices: [SERVICE_UUID]
            });

            device.addEventListener('gattserverdisconnected', handleDisconnect);

            // Connect to GATT server
            server = await device.gatt.connect();
            service = await server.getPrimaryService(SERVICE_UUID);

            // Get characteristics
            txAudioChar = await service.getCharacteristic(TX_AUDIO_UUID);
            rxAudioChar = await service.getCharacteristic(RX_AUDIO_UUID);
            controlChar = await service.getCharacteristic(CONTROL_UUID);
            deviceChar  = await service.getCharacteristic(DEVICE_UUID);

            // Subscribe to notifications
            await txAudioChar.startNotifications();
            txAudioChar.addEventListener('characteristicvaluechanged', onTxAudioNotify);

            await controlChar.startNotifications();
            controlChar.addEventListener('characteristicvaluechanged', onControlNotify);

            await deviceChar.startNotifications();
            deviceChar.addEventListener('characteristicvaluechanged', onDeviceNotify);

            // Initialize codec states
            rxDecodeState = AdpcmCodec.createState();
            txEncodeState = AdpcmCodec.createState();

            // Tell ESP32 to start audio
            await sendControl(CMD_AUDIO, 1);

            connected = true;
            console.log('[Speakermic] Connected to', device.name);
            if (onConnect) onConnect(device.name);

            return true;
        } catch (err) {
            console.error('[Speakermic] Connection failed:', err);
            throw err;
        }
    }

    function disconnect() {
        if (device && device.gatt.connected) {
            sendControl(CMD_AUDIO, 0).catch(() => {});
            device.gatt.disconnect();
        }
        cleanup();
    }

    function handleDisconnect() {
        console.log('[Speakermic] Disconnected');
        cleanup();
        if (onDisconnect) onDisconnect();
    }

    function cleanup() {
        connected = false;
        txAudioChar = null;
        rxAudioChar = null;
        controlChar = null;
        deviceChar = null;
        service = null;
        server = null;

        // Restore original mic track if we replaced it
        if (originalMicTrack && pcSender) {
            pcSender.replaceTrack(originalMicTrack).catch(() => {});
            originalMicTrack = null;
            pcSender = null;
        }

        if (captureWorklet) {
            captureWorklet.disconnect();
            captureWorklet = null;
        }
    }

    // --------------------------------------------------------
    // TX Audio: ESP32 Mic → Phone → WebRTC (radio TX)
    // --------------------------------------------------------

    function onTxAudioNotify(event) {
        if (!connected) return;
        const data = new Uint8Array(event.target.value.buffer);

        // Decode ADPCM from ESP32 mic
        const pcm = AdpcmCodec.decode(data, rxDecodeState);
        if (pcm.length === 0) return;

        // Feed decoded PCM to the mic MediaStreamTrack
        if (micStreamTrack && micStreamTrack._writer) {
            micStreamTrack._writer(pcm);
        }
    }

    /**
     * Set up the TX audio bridge:
     * Creates a MediaStreamTrack from decoded BLE mic audio
     * and replaces the WebRTC sender's mic track with it
     *
     * @param {RTCPeerConnection} pc - The WebRTC peer connection
     */
    async function setupTxBridge(pc) {
        // Find the audio sender
        const senders = pc.getSenders();
        pcSender = senders.find(s => s.track && s.track.kind === 'audio');
        if (!pcSender) {
            console.warn('[Speakermic] No audio sender found on peer connection');
            return;
        }

        // Save original mic track so we can restore it on disconnect
        originalMicTrack = pcSender.track;

        // Create a MediaStream from generated PCM audio
        // We use an AudioContext + ScriptProcessorNode (or AudioWorklet)
        // to generate a MediaStreamTrack from raw PCM samples
        if (!audioContext) {
            audioContext = new (window.AudioContext || window.webkitAudioContext)({
                sampleRate: SAMPLE_RATE
            });
        }

        // Buffer for incoming PCM frames from BLE
        const pcmQueue = [];
        const bufferNode = audioContext.createScriptProcessor(FRAME_SAMPLES, 0, 1);

        bufferNode.onaudioprocess = (e) => {
            const output = e.outputBuffer.getChannelData(0);
            if (pcmQueue.length > 0) {
                const frame = pcmQueue.shift();
                for (let i = 0; i < output.length && i < frame.length; i++) {
                    output[i] = frame[i] / 32768.0;  // Int16 → Float32
                }
                // Zero-fill remainder
                for (let i = frame.length; i < output.length; i++) {
                    output[i] = 0;
                }
            } else {
                // Silence when no data
                output.fill(0);
            }
        };

        const dest = audioContext.createMediaStreamDestination();
        bufferNode.connect(dest);

        // Attach writer function to the track for onTxAudioNotify to use
        const newTrack = dest.stream.getAudioTracks()[0];
        newTrack._writer = (pcmInt16) => {
            // Convert Int16Array to Float32Array
            const float32 = new Float32Array(pcmInt16.length);
            for (let i = 0; i < pcmInt16.length; i++) {
                float32[i] = pcmInt16[i] / 32768.0;
            }
            pcmQueue.push(float32);
        };

        micStreamTrack = newTrack;

        // Track starts disabled — only enabled during PTT
        newTrack.enabled = false;

        // Replace the WebRTC sender track
        await pcSender.replaceTrack(newTrack);
        console.log('[Speakermic] TX bridge active — mic track replaced');
    }

    // --------------------------------------------------------
    // RX Audio: WebRTC (radio RX) → Phone → ESP32 Speaker
    // --------------------------------------------------------

    /**
     * Set up the RX audio bridge:
     * Captures audio from the WebRTC RX stream, encodes as ADPCM,
     * and sends to ESP32 via BLE write
     *
     * @param {MediaStream} rxStream - The WebRTC remote audio stream
     */
    async function setupRxBridge(rxStream) {
        if (!rxStream || rxStream.getAudioTracks().length === 0) {
            console.warn('[Speakermic] No RX audio stream available');
            return;
        }

        if (!audioContext) {
            audioContext = new (window.AudioContext || window.webkitAudioContext)({
                sampleRate: SAMPLE_RATE
            });
        }

        // Try AudioWorklet first, fall back to ScriptProcessorNode
        try {
            // Create worklet from inline code (since ECHOCAT inlines everything)
            const workletCode = `
                class SpeakermicCapture extends AudioWorkletProcessor {
                    constructor() {
                        super();
                        this.buffer = [];
                        this.ratio = sampleRate / 8000;
                    }
                    process(inputs) {
                        const input = inputs[0];
                        if (!input || !input[0]) return true;
                        const data = input[0];
                        for (let i = 0; i < data.length; i += this.ratio) {
                            const idx = Math.floor(i);
                            if (idx < data.length) {
                                this.buffer.push(Math.round(Math.max(-1, Math.min(1, data[idx])) * 32767));
                            }
                        }
                        while (this.buffer.length >= 160) {
                            const frame = new Int16Array(this.buffer.splice(0, 160));
                            this.port.postMessage({ samples: frame }, [frame.buffer]);
                        }
                        return true;
                    }
                }
                registerProcessor('speakermic-capture', SpeakermicCapture);
            `;
            const blob = new Blob([workletCode], { type: 'application/javascript' });
            const url = URL.createObjectURL(blob);
            await audioContext.audioWorklet.addModule(url);
            URL.revokeObjectURL(url);

            const source = audioContext.createMediaStreamSource(rxStream);
            captureWorklet = new AudioWorkletNode(audioContext, 'speakermic-capture');

            captureWorklet.port.onmessage = (e) => {
                if (!connected || !rxAudioChar) return;
                const pcm = new Int16Array(e.data.samples);
                const adpcm = AdpcmCodec.encode(pcm, txEncodeState);
                rxAudioChar.writeValueWithoutResponse(adpcm).catch(() => {});
            };

            source.connect(captureWorklet);
            console.log('[Speakermic] RX bridge active (AudioWorklet)');
        } catch (err) {
            console.warn('[Speakermic] AudioWorklet failed, using ScriptProcessor:', err);

            // Fallback: ScriptProcessorNode
            const source = audioContext.createMediaStreamSource(rxStream);
            const processor = audioContext.createScriptProcessor(4096, 1, 1);
            const ratio = audioContext.sampleRate / SAMPLE_RATE;
            let buffer = [];

            processor.onaudioprocess = (e) => {
                if (!connected || !rxAudioChar) return;
                const input = e.inputBuffer.getChannelData(0);

                for (let i = 0; i < input.length; i += ratio) {
                    const idx = Math.floor(i);
                    if (idx < input.length) {
                        buffer.push(Math.round(Math.max(-1, Math.min(1, input[idx])) * 32767));
                    }
                }

                while (buffer.length >= FRAME_SAMPLES) {
                    const frame = new Int16Array(buffer.splice(0, FRAME_SAMPLES));
                    const adpcm = AdpcmCodec.encode(frame, txEncodeState);
                    rxAudioChar.writeValueWithoutResponse(adpcm).catch(() => {});
                }
            };

            source.connect(processor);
            processor.connect(audioContext.destination);  // Must connect to keep alive
            captureWorklet = processor;
            console.log('[Speakermic] RX bridge active (ScriptProcessor fallback)');
        }
    }

    // --------------------------------------------------------
    // Control Characteristic Handlers
    // --------------------------------------------------------

    function onControlNotify(event) {
        const data = new Uint8Array(event.target.value.buffer);
        if (data.length < 2) return;

        const cmd = data[0];
        const value = data[1];

        switch (cmd) {
            case CMD_PTT:
                console.log('[Speakermic] PTT:', value ? 'ON' : 'OFF');
                if (micStreamTrack) {
                    micStreamTrack.enabled = !!value;
                }
                if (onPtt) onPtt(!!value);
                break;

            case CMD_BATTERY:
                if (onBattery) onBattery(value);
                break;

            case CMD_STATUS:
                console.log('[Speakermic] Status:', value);
                break;
        }
    }

    function onDeviceNotify(event) {
        const data = new Uint8Array(event.target.value.buffer);
        if (data.length >= 1 && onBattery) {
            onBattery(data[0]);
        }
    }

    async function sendControl(cmd, value) {
        if (!controlChar) return;
        const data = new Uint8Array([cmd, value]);
        await controlChar.writeValueWithResponse(data);
    }

    // --------------------------------------------------------
    // Volume Control
    // --------------------------------------------------------

    async function setVolume(percent) {
        await sendControl(CMD_VOLUME, Math.max(0, Math.min(100, percent)));
    }

    // --------------------------------------------------------
    // Public API
    // --------------------------------------------------------

    return {
        connect,
        disconnect,
        setupTxBridge,
        setupRxBridge,
        setVolume,
        sendControl,

        get isConnected() { return connected; },
        get deviceName() { return device ? device.name : null; },

        // Event handlers
        set onConnect(fn) { onConnect = fn; },
        set onDisconnect(fn) { onDisconnect = fn; },
        set onPtt(fn) { onPtt = fn; },
        set onBattery(fn) { onBattery = fn; }
    };
})();
