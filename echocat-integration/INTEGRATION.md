# Integrating Speakermic into ECHOCAT

This guide explains how to add POTACAT Speakermic support to the ECHOCAT remote page.

## Files to Add

The following JavaScript files need to be concatenated into ECHOCAT's inlined page:

1. **`adpcm.js`** — IMA-ADPCM codec (shared between ESP32 and browser)
2. **`speakermic-bridge.js`** — Web Bluetooth connection manager and audio bridge
3. **`speakermic-ui.js`** — UI controls (connect button, battery indicator)

The `speakermic-worklet.js` file is a reference — the worklet code is inlined as a Blob URL in `speakermic-bridge.js` to avoid subresource loading issues with ECHOCAT's self-signed HTTPS.

## Integration Steps

### 1. Add Scripts to Page Build

In your ECHOCAT build process (where `remote.html` is assembled), add these scripts in order:

```html
<script>/* Contents of adpcm.js */</script>
<script>/* Contents of speakermic-bridge.js */</script>
<script>/* Contents of speakermic-ui.js */</script>
```

These must load **after** the main `remote.js` (they reference ECHOCAT's PTT functions).

### 2. Initialize the UI

In `remote.js`, after the WebRTC peer connection is established and audio is connected, add:

```javascript
// After pc.ontrack fires and remoteStream is available:
SpeakermicUI.init(
    document.getElementById('bottom-bar'),  // Parent element for controls
    {
        pc: pc,                    // RTCPeerConnection
        rxStream: remoteStream,    // Remote audio MediaStream from ontrack
        pttStart: pttStart,        // Existing PTT start function
        pttStop: pttStop           // Existing PTT stop function
    }
);
```

### 3. Handle PTT from Speakermic

The speakermic sends PTT press/release events via BLE, which `SPEAKERMIC.onPtt` routes to your existing `pttStart()`/`pttStop()` functions. No additional PTT handling is needed — the existing WebSocket PTT messages to the server work unchanged.

### 4. Handle Mic Track Replacement

When the speakermic is connected and PTT is pressed, the bridge replaces the WebRTC sender's audio track with a track generated from the ESP32's microphone audio. When the speakermic disconnects, the original phone mic track is restored.

If your code re-creates the peer connection (e.g., on reconnect), call `SpeakermicUI.destroy()` and re-init.

## Browser Compatibility

| Browser | Support |
|---------|---------|
| Chrome (Android) | Full support via Web Bluetooth |
| Bluefy (iOS) | Full support via Web Bluetooth polyfill |
| Safari (iOS) | Not supported (no Web Bluetooth API) |
| Chrome (Desktop) | Works for testing (enable `chrome://flags/#enable-web-bluetooth`) |
| Firefox | Not supported |

## Testing Without Hardware

You can test the BLE connection flow using the **nRF Connect** app on your phone, which can simulate a BLE peripheral with custom GATT services.

For audio testing, the `adpcm.js` codec can be tested independently:

```javascript
const state = AdpcmCodec.createState();
const pcm = new Int16Array(160);  // Fill with test samples
const encoded = AdpcmCodec.encode(pcm, state);

const decState = AdpcmCodec.createState();
const decoded = AdpcmCodec.decode(encoded, decState);
// decoded should closely match original pcm
```
