# ECHOCAT Speakermic Integration Plan

## Overview

Add POTACAT Speakermic support to ECHOCAT's remote page (`remote.html` / `remote.js` / `remote.css`). The speakermic connects via Web Bluetooth and bridges audio into the existing WebRTC connection. **Zero server-side changes needed** — all changes are in the three renderer files that get inlined at serve time.

## Files to Modify

| File | Changes |
|------|---------|
| `renderer/remote.html` | Add speakermic button in bottom bar, settings section in overlay |
| `renderer/remote.js` | Add ADPCM codec, Web Bluetooth bridge, PTT integration, settings logic |
| `renderer/remote.css` | Add styles for speakermic button and status indicator |
| (none) | No server-side changes to `lib/remote-server.js` |

## Build Process

ECHOCAT inlines all three files at serve time via `_buildInlinedHtml()` in `remote-server.js`. No build tooling changes — just edit the source files and restart the server (or set `this._cachedInlinedHtml = null`).

---

## UI Design

### Principle: Opt-In Feature

Most ECHOCAT users will not have a speakermic. The feature is **off by default** — no UI clutter until the user enables it in settings. This follows the same pattern as Bluetooth PTT and Remote CW.

### 1. Settings Overlay — Speakermic Section (Gate)

Add a new section in the settings overlay, placed **after the Audio section** (after the Input/Output device selects and Refresh Devices button). This groups it logically with audio settings.

```
── Audio ──────────────────────────────
  Input Device:     [USB Audio ▾]
  Output Device:    [USB Audio ▾]
  [Refresh Devices]

── Speakermic ─────────────────────────     ← NEW SECTION
  Enable:           [Off]                ← master toggle, shows Mic button in bottom bar
  ─ below only visible when enabled ─
  Device:           [Connect] status
  Battery:          87% ████████░░
  Speaker Vol:      [────●────] 70%
  Auto-Reconnect:   [On]
```

The **Enable toggle** is the gate:
- **Off (default)**: Bottom bar is untouched. No Mic button. No Web Bluetooth usage. Zero impact.
- **On**: The Mic button appears in the bottom bar. User can now connect.

Settings stored in `localStorage`:
- `echocat-speakermic-enabled` — Master enable, default `'false'`
- `echocat-speakermic-vol` — Volume level (0-100), default 70
- `echocat-speakermic-auto-reconnect` — Auto-reconnect on disconnect, default `'true'`

### 2. Bottom Bar — Speakermic Button (Only When Enabled)

When the Enable toggle is On and audio is connected, the Mic button appears in `#bb-controls`:

```
Default:  [Live/Vol] [Scan] [PTT] [HALT]
Enabled:  [Live/Vol] [Mic] [Scan] [PTT] [HALT]
```

The button is a round button (same size as Scan) showing connection state:

| State | Appearance |
|-------|------------|
| Not connected | Dim gray circle with mic icon, labeled "Mic" |
| Connecting | Pulsing blue border, "..." |
| Connected | Green border + green mic icon, shows battery % |
| Error | Red border, "Mic" |

**When tapped**: Opens the Web Bluetooth device picker (first time) or disconnects (if already connected).

**When Enable is toggled Off**: Mic button is hidden from the bottom bar. If currently connected, the speakermic is disconnected and the original phone mic track is restored.

---

## HTML Changes (`renderer/remote.html`)

### A. Bottom Bar — Add Speakermic Button (Hidden by Default)

Inside `#bb-controls`, add the speakermic button between the `.bb-stack` (Live/Vol) and the Scan button. It starts **hidden** — only shown when the Enable toggle is On.

```html
<!-- After the .bb-stack div (Live/Vol buttons) -->
<!-- Before the scan-btn -->

<button type="button" id="speakermic-btn" class="bb-round-btn bb-mic-btn hidden" title="Connect Speakermic">
  <span id="speakermic-icon">&#x1F399;</span>
  <span id="speakermic-label">Mic</span>
</button>
```

The resulting `#bb-controls` structure becomes:

```html
<div id="bb-controls" class="bb-controls hidden">
  <div class="bb-stack">
    <button id="audio-btn" ...>Live</button>
    <button id="vol-boost-btn" ...>Vol 1x</button>
  </div>
  <button id="speakermic-btn" class="bb-round-btn bb-mic-btn hidden" title="Connect Speakermic">
    <span id="speakermic-icon">&#x1F399;</span>
    <span id="speakermic-label">Mic</span>
  </button>
  <button id="scan-btn" class="bb-round-btn" title="Scan spots">Scan</button>
  <button id="ptt-btn" class="ptt-button">PTT</button>
  <button id="estop-btn" class="bb-halt-btn">HALT</button>
</div>
```

The `hidden` class is removed/added by the Enable toggle in settings.

### B. Settings Overlay — Add Speakermic Section

Insert after the Audio section's "Refresh Devices" button row (after the `so-row` containing `#so-refresh-devices`), before the Display section:

```html
<!-- ── Speakermic ── -->
<div class="so-section-label">Speakermic</div>

<div class="so-row">
  <label class="so-label">Enable</label>
  <div class="so-controls">
    <button type="button" class="rc-btn rc-toggle" id="so-speakermic-enable">Off</button>
    <span style="font-size:11px;color:var(--text-dim);margin-left:6px;">Shows Mic button in toolbar</span>
  </div>
</div>

<!-- Below rows only visible when enabled -->
<div id="so-speakermic-settings" style="display:none;">

  <div class="so-row">
    <label class="so-label">Device</label>
    <div class="so-controls">
      <button type="button" class="rc-btn" id="so-speakermic-connect" style="min-width:90px;">Connect</button>
      <span id="so-speakermic-status" style="font-size:12px;color:var(--text-dim);margin-left:6px;">Not connected</span>
    </div>
  </div>

  <div class="so-row" id="so-speakermic-details" style="display:none;">
    <label class="so-label">Battery</label>
    <div class="so-controls">
      <div style="display:flex;align-items:center;gap:6px;">
        <div id="so-speakermic-batt-bar" style="width:80px;height:10px;background:#2a2a4a;border-radius:4px;overflow:hidden;">
          <div id="so-speakermic-batt-fill" style="height:100%;width:100%;background:var(--pota);border-radius:4px;"></div>
        </div>
        <span id="so-speakermic-batt-pct" style="font-size:12px;color:var(--text-dim);">--%</span>
      </div>
    </div>
  </div>

  <div class="so-row" id="so-speakermic-vol-row" style="display:none;">
    <label class="so-label">Speaker Vol</label>
    <div class="so-controls">
      <input type="range" id="so-speakermic-vol" min="0" max="100" value="70" style="width:100px;">
      <span id="so-speakermic-vol-label" style="font-size:12px;color:var(--text-dim);margin-left:4px;min-width:30px;">70%</span>
    </div>
  </div>

  <div class="so-row">
    <label class="so-label">Auto-Reconnect</label>
    <div class="so-controls">
      <button type="button" class="rc-btn rc-toggle active" id="so-speakermic-reconnect">On</button>
    </div>
  </div>

</div>
```

The `#so-speakermic-settings` wrapper is hidden until Enable is toggled On.
The `#so-speakermic-details` and `#so-speakermic-vol-row` rows within it are hidden until a speakermic is actually connected.

---

## CSS Changes (`renderer/remote.css`)

Add after existing bottom-bar styles:

```css
/* ── Speakermic Button ── */
.bb-mic-btn {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 1px;
  font-size: 11px;
  color: var(--text-dim);
  background: #1a1a2e;
  border: 2px solid #2a2a4a;
  transition: border-color 0.3s, color 0.3s;
}
.bb-mic-btn #speakermic-icon { font-size: 18px; line-height: 1; }
.bb-mic-btn #speakermic-label { font-size: 10px; }

/* Connected state */
.bb-mic-btn.mic-connected {
  border-color: var(--pota);
  color: var(--pota);
}
.bb-mic-btn.mic-connected #speakermic-label { font-size: 9px; }

/* Connecting state */
.bb-mic-btn.mic-connecting {
  border-color: #4a9eff;
  animation: mic-pulse 1.5s infinite;
}
@keyframes mic-pulse {
  0%, 100% { border-color: #4a9eff; }
  50% { border-color: #1a3a6e; }
}

/* Error state */
.bb-mic-btn.mic-error {
  border-color: var(--accent);
  color: var(--accent);
}
```

---

## JavaScript Changes (`renderer/remote.js`)

All speakermic code goes at the end of the IIFE, before the closing `})();`. This keeps it self-contained and avoids touching existing code except for two small integration hooks.

### A. ADPCM Codec (~80 lines)

Inline the IMA-ADPCM decoder/encoder. Same code as `echocat-integration/adpcm.js` but defined as local functions within the IIFE scope.

### B. Speakermic Bridge (~200 lines)

Web Bluetooth connection manager + audio bridge. Key functions:

```javascript
// ── Speakermic State ──
var smDevice = null;        // BluetoothDevice
var smServer = null;        // BluetoothRemoteGATTServer
var smTxChar = null;        // TX Audio characteristic (ESP32 → phone)
var smRxChar = null;        // RX Audio characteristic (phone → ESP32)
var smCtlChar = null;       // Control characteristic
var smDevChar = null;       // Device info characteristic
var smConnected = false;
var smRxDecodeState = null; // ADPCM state for decoding mic from ESP32
var smTxEncodeState = null; // ADPCM state for encoding speaker to ESP32
var smMicTrack = null;      // MediaStreamTrack from decoded BLE mic
var smOriginalTrack = null; // Original mic track (saved for restore)
var smCaptureNode = null;   // AudioWorklet/ScriptProcessor for RX capture
var smNextPlayTime = 0;
```

Core functions:

```javascript
async function smConnect() { ... }      // Web Bluetooth scan + connect + discover
function smDisconnect() { ... }          // Clean disconnect
function smOnDisconnect() { ... }        // Handle unexpected disconnect
function smOnTxAudio(event) { ... }      // BLE notify → decode ADPCM → feed mic track
function smOnControl(event) { ... }      // PTT, battery, status from ESP32
function smSetupTxBridge() { ... }       // Create mic MediaStreamTrack, replace WebRTC sender
function smSetupRxBridge() { ... }       // Capture WebRTC RX audio, encode, send to ESP32
function smSendControl(cmd, val) { ... } // Send command to ESP32
function smSetVolume(pct) { ... }        // Set ESP32 speaker volume
function smUpdateUI() { ... }            // Update button and settings UI
```

### C. Integration with Existing Audio (2 small hooks)

**Hook 1: After WebRTC connects** — Set up speakermic audio bridges when `pc` is ready.

In the existing `pc.ontrack` handler (or `pc.onconnectionstatechange` `'connected'` handler), add:

```javascript
// After existing ontrack/connection code:
if (smConnected) {
    smSetupTxBridge();
    smSetupRxBridge();
}
```

**Hook 2: PTT from speakermic** — When the ESP32's PTT button is pressed, call existing `pttStart()`/`pttStop()`.

```javascript
function smOnControl(event) {
    var d = new Uint8Array(event.target.value.buffer);
    if (d.length < 2) return;
    if (d[0] === 0x01) {
        // PTT from speakermic
        if (d[1] === 1) {
            if (smMicTrack) smMicTrack.enabled = true;
            pttStart();   // ← calls existing function
        } else {
            pttStop();    // ← calls existing function
            if (smMicTrack) smMicTrack.enabled = false;
        }
    }
    if (d[0] === 0x02) smUpdateBattery(d[1]);
}
```

### D. TX Audio Bridge — Mic replacement

When the speakermic is connected and PTT is pressed:

1. The ESP32 sends ADPCM-encoded mic audio as BLE notifications
2. The phone decodes ADPCM to PCM
3. PCM is fed into a `ScriptProcessorNode` → `MediaStreamDestination` → `MediaStreamTrack`
4. This track **replaces** the phone's mic track on the WebRTC sender via `RTCRtpSender.replaceTrack()`
5. Audio now flows: ESP32 mic → BLE → phone → WebRTC → shack → radio

When the speakermic disconnects, the original phone mic track is restored.

```javascript
async function smSetupTxBridge() {
    // Find the audio sender on the peer connection
    var senders = pc.getSenders();
    var audioSender = senders.find(function(s) { return s.track && s.track.kind === 'audio'; });
    if (!audioSender) return;

    smOriginalTrack = audioSender.track;

    // Create AudioContext at 8kHz for BLE mic audio
    var smAudioCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 8000 });
    var pcmQueue = [];
    var bufNode = smAudioCtx.createScriptProcessor(160, 0, 1);
    bufNode.onaudioprocess = function(e) {
        var out = e.outputBuffer.getChannelData(0);
        if (pcmQueue.length > 0) {
            var frame = pcmQueue.shift();
            for (var i = 0; i < out.length && i < frame.length; i++) out[i] = frame[i];
            for (var j = frame.length; j < out.length; j++) out[j] = 0;
        } else {
            out.fill(0);
        }
    };
    var dest = smAudioCtx.createMediaStreamDestination();
    bufNode.connect(dest);

    smMicTrack = dest.stream.getAudioTracks()[0];
    smMicTrack.enabled = false; // Only enabled during PTT
    smMicTrack._writer = function(pcmFloat32) { pcmQueue.push(pcmFloat32); };

    await audioSender.replaceTrack(smMicTrack);
}
```

### E. RX Audio Bridge — Radio to speaker

Captures the existing WebRTC RX audio, downsamples to 8kHz, encodes to ADPCM, and sends to the ESP32 speaker:

```javascript
function smSetupRxBridge() {
    // Tap the existing audioCtx chain — connect a capture node to the RX source
    // The remote audio source is already connected: source → gainNode → rxAnalyser → destination
    // We add a branch: source → captureNode (which encodes and sends via BLE)

    // Use ScriptProcessorNode for broad compatibility
    var ratio = audioCtx.sampleRate / 8000;
    var buffer = [];
    smCaptureNode = audioCtx.createScriptProcessor(4096, 1, 1);

    smCaptureNode.onaudioprocess = function(e) {
        if (!smConnected || !smRxChar) return;
        var input = e.inputBuffer.getChannelData(0);
        // Downsample
        for (var i = 0; i < input.length; i += ratio) {
            var idx = Math.floor(i);
            if (idx < input.length) {
                buffer.push(Math.round(Math.max(-1, Math.min(1, input[idx])) * 32767));
            }
        }
        // Encode and send ADPCM frames
        while (buffer.length >= 160) {
            var frame = new Int16Array(buffer.splice(0, 160));
            var adpcm = smAdpcmEncode(frame, smTxEncodeState);
            smRxChar.writeValueWithoutResponse(adpcm).catch(function(){});
        }
    };

    // Connect to the gain node output (parallel to existing chain)
    gainNode.connect(smCaptureNode);
    smCaptureNode.connect(audioCtx.destination); // Required to keep processor alive (outputs silence)
}
```

### F. Enable Toggle + Button Event Handlers

```javascript
var speakermicBtn = document.getElementById('speakermic-btn');
var soSmEnable = document.getElementById('so-speakermic-enable');
var soSmSettings = document.getElementById('so-speakermic-settings');
var soSmConnect = document.getElementById('so-speakermic-connect');
var smEnabled = localStorage.getItem('echocat-speakermic-enabled') === 'true';

// ── Enable toggle (master gate) ──
function smSetEnabled(on) {
    smEnabled = on;
    localStorage.setItem('echocat-speakermic-enabled', on);
    soSmEnable.classList.toggle('active', on);
    soSmEnable.textContent = on ? 'On' : 'Off';
    soSmSettings.style.display = on ? '' : 'none';

    if (on) {
        // Show Mic button in bottom bar (only if audio controls are visible)
        speakermicBtn.classList.remove('hidden');
    } else {
        // Hide Mic button, disconnect if connected
        speakermicBtn.classList.add('hidden');
        if (smConnected) smDisconnect();
    }
}

// Restore saved state on load
smSetEnabled(smEnabled);

soSmEnable.addEventListener('click', function() {
    smSetEnabled(!smEnabled);
});

// ── Bottom bar button ──
speakermicBtn.addEventListener('click', function() {
    if (smConnected) {
        smDisconnect();
    } else {
        smConnect();
    }
});

// ── Settings overlay connect button ──
soSmConnect.addEventListener('click', function() {
    if (smConnected) {
        smDisconnect();
    } else {
        smConnect();
    }
});

// ── Volume slider ──
var soSmVol = document.getElementById('so-speakermic-vol');
soSmVol.addEventListener('input', function() {
    var v = parseInt(soSmVol.value);
    document.getElementById('so-speakermic-vol-label').textContent = v + '%';
    smSetVolume(v);
    localStorage.setItem('echocat-speakermic-vol', v);
});

// ── Auto-reconnect toggle ──
var soSmReconnect = document.getElementById('so-speakermic-reconnect');
soSmReconnect.addEventListener('click', function() {
    var on = !soSmReconnect.classList.contains('active');
    soSmReconnect.classList.toggle('active', on);
    soSmReconnect.textContent = on ? 'On' : 'Off';
    localStorage.setItem('echocat-speakermic-auto-reconnect', on);
});
```

### G. UI Update Function

```javascript
function smUpdateUI() {
    var btn = speakermicBtn;
    var label = document.getElementById('speakermic-label');
    var statusEl = document.getElementById('so-speakermic-status');
    var connectBtn = document.getElementById('so-speakermic-connect');
    var detailsRow = document.getElementById('so-speakermic-details');
    var volRow = document.getElementById('so-speakermic-vol-row');

    btn.classList.remove('mic-connected', 'mic-connecting', 'mic-error');

    if (smConnected) {
        btn.classList.add('mic-connected');
        label.textContent = smBatteryPct + '%';
        statusEl.textContent = smDevice.name || 'Connected';
        statusEl.style.color = 'var(--pota)';
        connectBtn.textContent = 'Disconnect';
        connectBtn.style.borderColor = 'var(--accent)';
        connectBtn.style.color = 'var(--accent)';
        detailsRow.style.display = '';
        volRow.style.display = '';
    } else {
        label.textContent = 'Mic';
        statusEl.textContent = 'Not connected';
        statusEl.style.color = 'var(--text-dim)';
        connectBtn.textContent = 'Connect';
        connectBtn.style.borderColor = '';
        connectBtn.style.color = '';
        detailsRow.style.display = 'none';
        volRow.style.display = 'none';
    }
}
```

---

## Integration Sequence Diagram

```
User taps "Mic" button in bottom bar
    │
    ▼
smConnect() — navigator.bluetooth.requestDevice({ filters: [POTACAT service UUID] })
    │
    ▼
User selects POTACAT-MIC from device picker
    │
    ▼
Connect GATT → Discover service → Get 4 characteristics
    │
    ▼
Subscribe to TX Audio notifications (mic data from ESP32)
Subscribe to Control notifications (PTT state, battery)
    │
    ▼
smSetupTxBridge() — Create mic MediaStreamTrack, replace WebRTC sender track
smSetupRxBridge() — Tap RX audio chain, encode ADPCM, write to ESP32
    │
    ▼
Update UI: button turns green, battery shown, settings populated
    │
    ▼
READY — Audio bridges active

───────────────────────────────────────────────────

User presses PTT button on ESP32 (or phone screen)
    │
    ▼
ESP32 sends Control notify: [0x01, 0x01] (PTT ON)
    │
    ▼
smOnControl() → enables smMicTrack → calls pttStart()
    │
    ▼
pttStart() sends PTT WebSocket message to shack server (existing flow)
ESP32 starts streaming ADPCM mic audio via TX Audio notifications
    │
    ▼
smOnTxAudio() → decode ADPCM → write to smMicTrack buffer → WebRTC sends to shack
    │
    ▼
Shack server routes audio to radio USB input → Radio transmits

───────────────────────────────────────────────────

Radio receives audio
    │
    ▼
Shack server captures via hidden BrowserWindow → WebRTC sends to phone
    │
    ▼
Phone receives via pc.ontrack → gainNode → smCaptureNode (RX bridge)
    │
    ▼
smCaptureNode downsamples to 8kHz + ADPCM encodes → BLE write to ESP32
    │
    ▼
ESP32 decodes ADPCM → I2S → MAX98357A → Speaker
```

---

## What Does NOT Change

- `lib/remote-server.js` — No changes
- `main.js` — No changes
- `renderer/remote-audio.html` — No changes
- `preload-remote-audio.js` — No changes
- WebSocket PTT protocol — Same `{ type: 'ptt', state: true/false }`
- WebRTC signaling — Same SDP/ICE exchange
- Any other ECHOCAT functionality

The server sees normal WebRTC audio and WebSocket PTT regardless of whether the audio originates from the phone mic or the BLE speakermic.

---

## Testing Plan

1. **Enable Off (default)** — Verify bottom bar shows `[Live/Vol] [Scan] [PTT] [HALT]` with no Mic button. All existing ECHOCAT features work unchanged.
2. **Enable On** — Toggle Enable in settings. Verify Mic button appears in bottom bar. Verify remaining settings rows become visible.
3. **Enable Off again** — Toggle back Off. Verify Mic button disappears. If connected, verify speakermic disconnects and phone mic track restores.
4. **Connect speakermic** — Tap Mic button, pick POTACAT-MIC. Verify green border, battery %, settings update.
5. **RX audio** — Verify radio audio comes out ESP32 speaker (requires MAX98357A).
6. **TX audio (PTT from ESP32)** — Press ESP32 PTT button. Verify mic audio reaches shack radio.
7. **TX audio (PTT from phone)** — Tap PTT on screen. Should use speakermic mic (not phone mic).
8. **Disconnect mid-session** — Disconnect speakermic. Verify original phone mic track restored, phone mic works for PTT.
9. **Battery level** — Verify battery % shows in Mic button label and settings battery bar.
10. **Auto-reconnect** — Force BLE disconnect, verify ESP32 re-advertises and phone reconnects.
11. **Persist across reload** — Enable toggle On, reload page. Verify Mic button still present.
12. **No Web Bluetooth** — On Safari/Firefox, verify Enable toggle is hidden or shows "Not supported" message.

---

## Implementation Order

### Step 1: CSS (5 min)
Add the `.bb-mic-btn` styles and state classes.

### Step 2: HTML (10 min)
Add the speakermic button to `#bb-controls` in the bottom bar.
Add the Speakermic section to the settings overlay.

### Step 3: JavaScript — ADPCM Codec (copy-paste)
Inline the ADPCM encode/decode functions.

### Step 4: JavaScript — Connection Manager
`smConnect()`, `smDisconnect()`, BLE event handlers, UI updates.

### Step 5: JavaScript — TX Audio Bridge
Decode BLE mic audio → MediaStreamTrack → replace WebRTC sender.

### Step 6: JavaScript — RX Audio Bridge
Capture WebRTC RX audio → ADPCM encode → BLE write to ESP32.

### Step 7: JavaScript — PTT Integration
Wire ESP32 PTT button to existing `pttStart()`/`pttStop()`.

### Step 8: JavaScript — Settings
Volume slider, auto-reconnect toggle, localStorage persistence.

### Step 9: Test end-to-end
Connect speakermic → PTT → verify audio flows both directions.
