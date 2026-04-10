# Integrating Speakermic into ECHOCAT

Step-by-step instructions for adding POTACAT Speakermic support to ECHOCAT. All changes are in three files under `renderer/`. Zero server-side changes.

Reference files in this directory:
- `speakermic-patch.css` — Styles to append
- `speakermic-patch.html` — HTML snippets with insertion points
- `speakermic-patch.js` — Part A (6 one-line hooks) + Part B (new code block)

---

## Step 1: CSS — Append to `renderer/remote.css`

Copy the entire contents of `speakermic-patch.css` and **append it to the end** of `renderer/remote.css`.

---

## Step 2: HTML — Two insertions in `renderer/remote.html`

### 2a. Bottom Bar — Add Mic Button

Find this line inside `#bb-controls` (~line 926):
```html
<button type="button" id="scan-btn" class="bb-round-btn" title="Scan spots">Scan</button>
```

**INSERT AFTER IT** (before the PTT button):
```html
        <button type="button" id="speakermic-btn" class="bb-round-btn bb-mic-btn hidden" title="Connect Speakermic">
          <span id="speakermic-icon">&#x1F399;</span>
          <span id="speakermic-label">Mic</span>
        </button>
```

### 2b. Settings Overlay — Add Speakermic Section

Find this line (~line 206):
```html
<button type="button" id="rc-audio-refresh" class="ft8-filter-btn" style="margin:4px 0 8px;">Refresh Devices</button>
```

**INSERT AFTER IT** (before the `<div class="so-section-label">Display</div>` line):

Copy the Speakermic settings HTML block from `speakermic-patch.html` (the section labeled "INSERTION 2").

---

## Step 3: JavaScript — Hooks + New Code in `renderer/remote.js`

### 3a. Six Small Hooks (Part A)

These are one-line additions to existing functions. Open `speakermic-patch.js` and follow the "PART A" instructions. Summary:

| Hook | Function | What to add |
|------|----------|-------------|
| 1 | `showAudioControls()` | `if (smEnabled) speakermicBtn.classList.remove('hidden');` |
| 2 | `showConnectPrompt()` | `speakermicBtn.classList.add('hidden');` |
| 3 | `pc.ontrack` in `startAudio()` | `if (smConnected) smSetupRxBridge(event.streams[0]);` |
| 4 | `stopAudio()` | `smCleanupAudio();` |
| 5 | `pttStart()` | `if (smConnected && smMicTrack) smMicTrack.enabled = true;` |
| 6 | `pttStop()` | `if (smConnected && smMicTrack) smMicTrack.enabled = false;` |

### 3b. New Code Block (Part B)

Copy the entire "PART B" section from `speakermic-patch.js` (~250 lines).

**Insert it into `renderer/remote.js` just BEFORE these lines** (near the end of the file):
```javascript
  // Auto-connect on page load
  connect('');
})();
```

---

## Step 4: Test

1. Restart ECHOCAT (or delete `_cachedInlinedHtml` to force rebuild)
2. Open ECHOCAT on your phone
3. Go to Settings (gear icon)
4. Scroll to **Speakermic** section
5. Toggle **Enable** to On
6. The **Mic** button should appear in the bottom bar (after connecting audio)
7. Tap **Mic** → select **POTACAT-MIC** from the BLE picker
8. Short GPIO2 to GND on the ESP32 for PTT — audio should route through to your radio

## Browser Compatibility

| Browser | Works? | Notes |
|---------|--------|-------|
| Chrome (Android) | Yes | Native Web Bluetooth |
| Bluefy (iOS) | Yes | Web Bluetooth via CoreBluetooth |
| Safari (iOS) | No | No Web Bluetooth — Enable toggle shows "N/A" |
| Chrome (Desktop) | Yes | For testing; enable `chrome://flags/#enable-web-bluetooth` |
| Firefox | No | No Web Bluetooth |

## What This Does NOT Change

- No server-side changes (`remote-server.js`, `main.js`)
- No changes to the WebSocket/WebRTC protocol
- No changes to the audio bridge (`remote-audio.html`)
- Default UI is completely unchanged (Speakermic is off by default)
- All existing features work identically
