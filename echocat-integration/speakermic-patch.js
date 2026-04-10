// ============================================================
// POTACAT Speakermic — JavaScript for remote.js
//
// TWO PARTS:
//   Part A: Small hooks into existing functions (5 one-line additions)
//   Part B: New code block to insert before the IIFE closing })();
//
// ============================================================


// ════════════════════════════════════════════════════════════
// PART A: Hooks into existing functions
// These are single lines to add to existing functions.
// ════════════════════════════════════════════════════════════

// ── Hook 1: showAudioControls() ──
// In the existing showAudioControls() function, ADD this line at the end:
//
//   function showAudioControls() {
//     audioConnectBtn.classList.add('hidden');
//     bbControls.classList.remove('hidden');
//     if (smEnabled) speakermicBtn.classList.remove('hidden');   // ← ADD THIS
//   }


// ── Hook 2: showConnectPrompt() ──
// In the existing showConnectPrompt() function, ADD this line:
//
//   function showConnectPrompt() {
//     audioConnectBtn.textContent = 'Tap to Connect Audio';
//     audioConnectBtn.classList.remove('hidden');
//     bbControls.classList.add('hidden');
//     speakermicBtn.classList.add('hidden');                     // ← ADD THIS
//   }


// ── Hook 3: pc.ontrack handler ──
// Inside startAudio(), in the pc.ontrack handler, ADD after the gainNode wiring
// (after the try/catch block that connects source → gainNode), before the closing };
//
//   pc.ontrack = (event) => {
//     ... existing code ...
//     if (smConnected) smSetupRxBridge(event.streams[0]);       // ← ADD THIS
//   };


// ── Hook 4: stopAudio() ──
// In the existing stopAudio() function, ADD after `if (pc) { pc.close(); pc = null; }`:
//
//   function stopAudio() {
//     ...
//     if (pc) { pc.close(); pc = null; }
//     smCleanupAudio();                                         // ← ADD THIS
//     ...


// ── Hook 5: pttStart() — enable speakermic mic track ──
// In pttStart(), ADD after the localAudioStream mic enable line:
//
//   if (localAudioStream) localAudioStream.getAudioTracks().forEach(t => { t.enabled = true; });
//   if (smConnected && smMicTrack) smMicTrack.enabled = true;   // ← ADD THIS


// ── Hook 6: pttStop() — disable speakermic mic track ──
// In pttStop(), ADD after the localAudioStream mic disable line:
//
//   if (localAudioStream) localAudioStream.getAudioTracks().forEach(t => { t.enabled = false; });
//   if (smConnected && smMicTrack) smMicTrack.enabled = false;  // ← ADD THIS



// ════════════════════════════════════════════════════════════
// PART B: New code block
// Insert this ENTIRE block into remote.js just BEFORE:
//   // Auto-connect on page load
//   connect('');
// })();
// ════════════════════════════════════════════════════════════

  // ── POTACAT Speakermic ────────────────────────────────────
  // BLE-connected speaker/microphone for hands-free operation.
  // https://github.com/Waffleslop/potacat-speakermic

  // ── IMA-ADPCM Codec ──
  var SM_STEP = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767];
  var SM_IDX = [-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8];

  function smAdpcmDecode(input, state) {
    if (input.length < 4) return new Float32Array(0);
    state.p = (input[0] | (input[1] << 8)) << 16 >> 16;
    state.i = Math.min(88, Math.max(0, input[2]));
    var dl = input.length - 4, pcm = new Float32Array(dl * 2), sc = 0;
    for (var b = 0; b < dl; b++) {
      for (var ni = 0; ni < 2; ni++) {
        var nib = ni === 0 ? (input[4 + b] & 0x0F) : ((input[4 + b] >> 4) & 0x0F);
        var step = SM_STEP[state.i], delta = (step >> 3);
        if (nib & 4) delta += step;
        if (nib & 2) delta += (step >> 1);
        if (nib & 1) delta += (step >> 2);
        if (nib & 8) delta = -delta;
        state.p = Math.max(-32768, Math.min(32767, state.p + delta));
        state.i = Math.min(88, Math.max(0, state.i + SM_IDX[nib]));
        pcm[sc++] = state.p / 32768.0;
      }
    }
    return pcm.subarray(0, sc);
  }

  function smAdpcmEncode(pcmInt16, state) {
    var n = pcmInt16.length, out = new Uint8Array(4 + Math.ceil(n / 2));
    out[0] = state.p & 0xFF; out[1] = (state.p >> 8) & 0xFF;
    out[2] = state.i; out[3] = 0;
    var bi = 0, hi = false;
    for (var i = 0; i < n; i++) {
      var step = SM_STEP[state.i], diff = pcmInt16[i] - state.p, nib = 0;
      if (diff < 0) { nib = 8; diff = -diff; }
      if (diff >= step) { nib |= 4; diff -= step; }
      if (diff >= (step >> 1)) { nib |= 2; diff -= (step >> 1); }
      if (diff >= (step >> 2)) { nib |= 1; }
      var d2 = (SM_STEP[state.i] >> 3);
      if (nib & 4) d2 += SM_STEP[state.i];
      if (nib & 2) d2 += (SM_STEP[state.i] >> 1);
      if (nib & 1) d2 += (SM_STEP[state.i] >> 2);
      if (nib & 8) d2 = -d2;
      state.p = Math.max(-32768, Math.min(32767, state.p + d2));
      state.i = Math.min(88, Math.max(0, state.i + SM_IDX[nib]));
      if (!hi) { out[4 + bi] = nib & 0x0F; hi = true; }
      else { out[4 + bi] |= (nib << 4); bi++; hi = false; }
    }
    return out;
  }

  // ── BLE UUIDs ──
  var SM_SVC  = 'f47ac10b-58cc-4372-a567-0e02b2c3d479';
  var SM_TX   = 'f47ac10b-58cc-4372-a567-0e02b2c30001';
  var SM_RX   = 'f47ac10b-58cc-4372-a567-0e02b2c30002';
  var SM_CTL  = 'f47ac10b-58cc-4372-a567-0e02b2c30003';
  var SM_DEV  = 'f47ac10b-58cc-4372-a567-0e02b2c30004';

  // ── State ──
  var smDevice = null, smServer = null;
  var smTxChar = null, smRxChar = null, smCtlChar = null, smDevChar = null;
  var smConnected = false;
  var smEnabled = localStorage.getItem('echocat-speakermic-enabled') === 'true';
  var smAutoReconnect = localStorage.getItem('echocat-speakermic-auto-reconnect') !== 'false';
  var smBatteryPct = 0;
  var smRxDecState = { p: 0, i: 0 };
  var smTxEncState = { p: 0, i: 0 };
  var smMicTrack = null;
  var smOriginalTrack = null;
  var smCaptureNode = null;
  var smMicAudioCtx = null;
  var smMicBufNode = null;
  var smMicPcmQueue = [];
  var smNextPlayTime = 0;

  // ── DOM Elements ──
  var speakermicBtn = document.getElementById('speakermic-btn');
  var smIcon = document.getElementById('speakermic-icon');
  var smLabel = document.getElementById('speakermic-label');
  var soSmEnable = document.getElementById('so-speakermic-enable');
  var soSmSettings = document.getElementById('so-speakermic-settings');
  var soSmConnect = document.getElementById('so-speakermic-connect');
  var soSmStatus = document.getElementById('so-speakermic-status');
  var soSmDetails = document.getElementById('so-speakermic-details');
  var soSmVolRow = document.getElementById('so-speakermic-vol-row');
  var soSmBattFill = document.getElementById('so-speakermic-batt-fill');
  var soSmBattPct = document.getElementById('so-speakermic-batt-pct');
  var soSmVol = document.getElementById('so-speakermic-vol');
  var soSmVolLabel = document.getElementById('so-speakermic-vol-label');
  var soSmReconnect = document.getElementById('so-speakermic-reconnect');

  // ── Feature Detection ──
  var smHasBluetooth = !!(navigator.bluetooth);

  // ── Enable Toggle ──
  function smSetEnabled(on) {
    smEnabled = on;
    localStorage.setItem('echocat-speakermic-enabled', on);
    if (soSmEnable) {
      soSmEnable.classList.toggle('active', on);
      soSmEnable.textContent = on ? 'On' : 'Off';
    }
    if (soSmSettings) soSmSettings.style.display = on ? '' : 'none';
    if (on && audioEnabled) {
      speakermicBtn.classList.remove('hidden');
    } else {
      speakermicBtn.classList.add('hidden');
      if (smConnected) smDisconnect();
    }
  }

  // Restore saved state
  if (soSmEnable) {
    if (!smHasBluetooth) {
      soSmEnable.textContent = 'N/A';
      soSmEnable.disabled = true;
      soSmEnable.style.opacity = '0.4';
      soSmEnable.parentElement.querySelector('span').textContent = 'Web Bluetooth not supported';
    } else {
      smSetEnabled(smEnabled);
      soSmEnable.addEventListener('click', function () { smSetEnabled(!smEnabled); });
    }
  }

  // Restore volume
  var smSavedVol = localStorage.getItem('echocat-speakermic-vol');
  if (smSavedVol && soSmVol) { soSmVol.value = smSavedVol; soSmVolLabel.textContent = smSavedVol + '%'; }

  // Restore auto-reconnect
  if (soSmReconnect) {
    soSmReconnect.classList.toggle('active', smAutoReconnect);
    soSmReconnect.textContent = smAutoReconnect ? 'On' : 'Off';
    soSmReconnect.addEventListener('click', function () {
      smAutoReconnect = !smAutoReconnect;
      soSmReconnect.classList.toggle('active', smAutoReconnect);
      soSmReconnect.textContent = smAutoReconnect ? 'On' : 'Off';
      localStorage.setItem('echocat-speakermic-auto-reconnect', smAutoReconnect);
    });
  }

  // Volume slider
  if (soSmVol) {
    soSmVol.addEventListener('input', function () {
      var v = parseInt(soSmVol.value);
      soSmVolLabel.textContent = v + '%';
      smSendControl(0x04, v);
      localStorage.setItem('echocat-speakermic-vol', v);
    });
  }

  // ── Connect / Disconnect ──
  async function smConnect() {
    if (!smHasBluetooth) return;
    if (smConnected) return;
    smUpdateBtn('connecting');
    try {
      smDevice = await navigator.bluetooth.requestDevice({
        filters: [{ services: [SM_SVC] }]
      });
      smDevice.addEventListener('gattserverdisconnected', smOnDisconnect);
      smServer = await smDevice.gatt.connect();
      var svc = await smServer.getPrimaryService(SM_SVC);

      smTxChar = await svc.getCharacteristic(SM_TX);
      smRxChar = await svc.getCharacteristic(SM_RX);
      smCtlChar = await svc.getCharacteristic(SM_CTL);
      smDevChar = await svc.getCharacteristic(SM_DEV);

      await smTxChar.startNotifications();
      smTxChar.addEventListener('characteristicvaluechanged', smOnTxAudio);

      await smCtlChar.startNotifications();
      smCtlChar.addEventListener('characteristicvaluechanged', smOnControl);

      // Read initial device info
      try {
        var dv = await smDevChar.readValue();
        smBatteryPct = dv.getUint8(0);
      } catch (e) {}

      // Tell ESP32 to start audio
      smSendControl(0x05, 1);

      // Set saved volume
      var v = parseInt(localStorage.getItem('echocat-speakermic-vol') || '70');
      smSendControl(0x04, v);

      smRxDecState = { p: 0, i: 0 };
      smTxEncState = { p: 0, i: 0 };
      smConnected = true;

      // Set up audio bridges if WebRTC is already connected
      if (pc && audioEnabled) {
        smSetupTxBridge();
        // RX bridge needs the remote stream — get it from gainNode's source
        // It's set up via the Hook 3 in pc.ontrack, but if already connected:
        if (remoteAudio && remoteAudio.srcObject) {
          smSetupRxBridge(remoteAudio.srcObject);
        }
      }

      smUpdateBtn('connected');
      smUpdateSettings();
      console.log('[Speakermic] Connected to ' + (smDevice.name || 'device'));
    } catch (err) {
      console.error('[Speakermic] Connect failed:', err);
      smUpdateBtn('error');
      setTimeout(function () { if (!smConnected) smUpdateBtn('idle'); }, 3000);
    }
  }

  function smDisconnect() {
    if (smDevice && smDevice.gatt.connected) {
      try { smSendControl(0x05, 0); } catch (e) {}
      smDevice.gatt.disconnect();
    }
    smCleanupAll();
  }

  function smOnDisconnect() {
    console.log('[Speakermic] Disconnected');
    smCleanupAll();
    // Auto-reconnect: restart advertising scan after a delay
    if (smAutoReconnect && smEnabled) {
      smUpdateBtn('idle');
      // Can't auto-connect Web Bluetooth (requires user gesture), but update UI
      if (soSmStatus) soSmStatus.textContent = 'Disconnected — tap Mic to reconnect';
    }
  }

  function smCleanupAll() {
    smCleanupAudio();
    smConnected = false;
    smTxChar = null; smRxChar = null; smCtlChar = null; smDevChar = null;
    smServer = null;
    smUpdateBtn('idle');
    smUpdateSettings();
  }

  function smCleanupAudio() {
    // Restore original mic track
    if (smOriginalTrack && pc) {
      try {
        var senders = pc.getSenders();
        var audioSender = senders.find(function (s) { return s.track && s.track.kind === 'audio'; });
        if (audioSender) audioSender.replaceTrack(smOriginalTrack).catch(function () {});
      } catch (e) {}
    }
    smOriginalTrack = null;
    smMicTrack = null;
    smMicPcmQueue = [];

    // Clean up mic audio context
    if (smMicBufNode) { try { smMicBufNode.disconnect(); } catch (e) {} smMicBufNode = null; }
    if (smMicAudioCtx) { try { smMicAudioCtx.close(); } catch (e) {} smMicAudioCtx = null; }

    // Clean up RX capture node
    if (smCaptureNode) {
      try { smCaptureNode.disconnect(); } catch (e) {}
      smCaptureNode = null;
    }
  }

  // ── TX Audio Bridge: ESP32 Mic → WebRTC ──
  function smSetupTxBridge() {
    if (!pc) return;
    var senders = pc.getSenders();
    var audioSender = senders.find(function (s) { return s.track && s.track.kind === 'audio'; });
    if (!audioSender) { console.warn('[Speakermic] No audio sender on pc'); return; }

    smOriginalTrack = audioSender.track;

    // Create an AudioContext at 8kHz to match the BLE audio rate
    smMicAudioCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 8000 });
    smMicPcmQueue = [];

    smMicBufNode = smMicAudioCtx.createScriptProcessor(160, 0, 1);
    smMicBufNode.onaudioprocess = function (e) {
      var out = e.outputBuffer.getChannelData(0);
      if (smMicPcmQueue.length > 0) {
        var frame = smMicPcmQueue.shift();
        for (var i = 0; i < out.length && i < frame.length; i++) out[i] = frame[i];
        for (var j = frame.length; j < out.length; j++) out[j] = 0;
      } else {
        for (var k = 0; k < out.length; k++) out[k] = 0;
      }
    };

    var dest = smMicAudioCtx.createMediaStreamDestination();
    smMicBufNode.connect(dest);

    smMicTrack = dest.stream.getAudioTracks()[0];
    smMicTrack.enabled = false; // Only during PTT

    audioSender.replaceTrack(smMicTrack).then(function () {
      console.log('[Speakermic] TX bridge active — mic track replaced');
    }).catch(function (err) {
      console.error('[Speakermic] replaceTrack failed:', err);
    });
  }

  // ── RX Audio Bridge: WebRTC → ESP32 Speaker ──
  function smSetupRxBridge(remoteStream) {
    if (!audioCtx || !gainNode || !smRxChar) return;
    if (smCaptureNode) { try { smCaptureNode.disconnect(); } catch (e) {} }

    var ratio = audioCtx.sampleRate / 8000;
    var buffer = [];
    smTxEncState = { p: 0, i: 0 };

    smCaptureNode = audioCtx.createScriptProcessor(4096, 1, 1);
    smCaptureNode.onaudioprocess = function (e) {
      if (!smConnected || !smRxChar) return;
      var input = e.inputBuffer.getChannelData(0);
      // Downsample to 8kHz
      for (var i = 0; i < input.length; i += ratio) {
        var idx = Math.floor(i);
        if (idx < input.length) {
          buffer.push(Math.round(Math.max(-1, Math.min(1, input[idx])) * 32767));
        }
      }
      // Encode and send ADPCM frames (160 samples = 20ms each)
      while (buffer.length >= 160) {
        var frame = new Int16Array(buffer.splice(0, 160));
        var adpcm = smAdpcmEncode(frame, smTxEncState);
        smRxChar.writeValueWithoutResponse(adpcm).catch(function () {});
      }
    };

    // Tap the gain node output (parallel to existing RX chain)
    gainNode.connect(smCaptureNode);
    // Must connect to destination to keep ScriptProcessor alive (outputs silence)
    smCaptureNode.connect(audioCtx.destination);

    console.log('[Speakermic] RX bridge active — radio audio → ESP32 speaker');
  }

  // ── BLE Notification Handlers ──
  function smOnTxAudio(event) {
    if (!smConnected) return;
    var data = new Uint8Array(event.target.value.buffer);
    if (data.length < 5) return;

    // Decode ADPCM from ESP32 mic
    var pcm = smAdpcmDecode(data, smRxDecState);
    if (pcm.length === 0) return;

    // Feed to the mic MediaStreamTrack buffer
    if (smMicTrack && smMicPcmQueue) {
      smMicPcmQueue.push(pcm);
      // Prevent queue from growing unbounded if not draining
      while (smMicPcmQueue.length > 10) smMicPcmQueue.shift();
    }
  }

  function smOnControl(event) {
    var d = new Uint8Array(event.target.value.buffer);
    if (d.length < 2) return;

    switch (d[0]) {
      case 0x01: // PTT
        if (d[1] === 1) {
          if (smMicTrack) smMicTrack.enabled = true;
          pttStart();
        } else {
          pttStop();
          if (smMicTrack) smMicTrack.enabled = false;
        }
        break;

      case 0x02: // Battery
        smBatteryPct = d[1];
        smUpdateBtn(smConnected ? 'connected' : 'idle');
        smUpdateBattery(d[1]);
        break;
    }
  }

  function smSendControl(cmd, val) {
    if (!smCtlChar || !smConnected) return;
    var data = new Uint8Array([cmd, val]);
    smCtlChar.writeValueWithResponse(data).catch(function () {});
  }

  // ── UI Updates ──
  function smUpdateBtn(state) {
    if (!speakermicBtn) return;
    speakermicBtn.classList.remove('mic-connected', 'mic-connecting', 'mic-error');

    switch (state) {
      case 'connecting':
        speakermicBtn.classList.add('mic-connecting');
        smLabel.textContent = '...';
        break;
      case 'connected':
        speakermicBtn.classList.add('mic-connected');
        smLabel.textContent = smBatteryPct > 0 ? smBatteryPct + '%' : 'On';
        break;
      case 'error':
        speakermicBtn.classList.add('mic-error');
        smLabel.textContent = 'Err';
        break;
      default: // idle
        smLabel.textContent = 'Mic';
        break;
    }
  }

  function smUpdateSettings() {
    if (!soSmConnect) return;
    if (smConnected) {
      soSmConnect.textContent = 'Disconnect';
      soSmConnect.style.borderColor = 'var(--accent)';
      soSmConnect.style.color = 'var(--accent)';
      soSmStatus.textContent = smDevice ? (smDevice.name || 'Connected') : 'Connected';
      soSmStatus.style.color = 'var(--pota)';
      if (soSmDetails) soSmDetails.style.display = '';
      if (soSmVolRow) soSmVolRow.style.display = '';
      smUpdateBattery(smBatteryPct);
    } else {
      soSmConnect.textContent = 'Connect';
      soSmConnect.style.borderColor = '';
      soSmConnect.style.color = '';
      soSmStatus.textContent = 'Not connected';
      soSmStatus.style.color = 'var(--text-dim)';
      if (soSmDetails) soSmDetails.style.display = 'none';
      if (soSmVolRow) soSmVolRow.style.display = 'none';
    }
  }

  function smUpdateBattery(pct) {
    if (soSmBattFill) soSmBattFill.style.width = pct + '%';
    if (soSmBattPct) soSmBattPct.textContent = pct + '%';
    // Color: green > yellow > red
    if (soSmBattFill) {
      if (pct > 25) soSmBattFill.style.background = 'var(--pota)';
      else if (pct > 10) soSmBattFill.style.background = '#facc15';
      else soSmBattFill.style.background = 'var(--accent)';
    }
  }

  // ── Button Handlers ──
  if (speakermicBtn) {
    speakermicBtn.addEventListener('click', function () {
      if (smConnected) smDisconnect();
      else smConnect();
    });
  }

  if (soSmConnect) {
    soSmConnect.addEventListener('click', function () {
      if (smConnected) smDisconnect();
      else smConnect();
    });
  }

  // ── End Speakermic ───────────────────────────────────────
