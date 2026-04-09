# Architecture

## System Overview

The POTACAT Speakermic is a BLE-connected speaker/microphone that integrates with [ECHOCAT](https://github.com/Waffleslop/POTACAT) for remote ham radio operation. It enables hands-free audio while operating your radio remotely via ECHOCAT.

```
[ESP32 Speakermic]  <--- BLE --->  [Phone Browser]  <--- WebRTC --->  [Shack Computer]  <--- USB/CAT --->  [Radio]
```

The phone acts as a bridge: the ECHOCAT web page connects to the speakermic via Web Bluetooth and bridges audio into the existing WebRTC connection to the shack.

## Audio Flow

### TX Path (Your Voice -> Radio)

1. You press the PTT button on the speakermic
2. ESP32 enables the PDM microphone and starts reading audio at 8kHz/16-bit mono
3. Audio is encoded with IMA-ADPCM (4:1 compression) into 84-byte frames (20ms each)
4. Frames are sent as BLE GATT notifications to the phone (50 frames/second)
5. Phone's ECHOCAT page decodes ADPCM back to PCM
6. PCM audio is wrapped in a MediaStreamTrack and replaces the WebRTC sender's mic track
7. Audio travels via WebRTC to the shack computer
8. Shack computer plays it through the radio's USB audio input
9. Radio transmits your voice

### RX Path (Radio -> Your Ear)

1. Radio receives audio and outputs it via USB audio
2. Shack computer captures this via a hidden Electron BrowserWindow
3. Audio is sent via WebRTC to the phone
4. Phone's ECHOCAT page captures the WebRTC audio via an AudioWorklet
5. Audio is downsampled to 8kHz and encoded with IMA-ADPCM
6. Encoded frames are written to the ESP32's BLE RX characteristic
7. ESP32 decodes ADPCM and plays through the MAX98357A amplifier to the speaker

## BLE Protocol

### GATT Service: `f47ac10b-58cc-4372-a567-0e02b2c3d479`

| Characteristic | UUID Suffix | Direction | Properties | Payload |
|---|---|---|---|---|
| TX Audio | `0001` | ESP32 -> Phone | Notify | 84-byte ADPCM frame |
| RX Audio | `0002` | Phone -> ESP32 | Write NR | 84-byte ADPCM frame |
| Control | `0003` | Bidirectional | R/W/Notify | 2-byte command |
| Device Info | `0004` | ESP32 -> Phone | Read/Notify | Battery %, FW version |

### Control Commands

| Byte 0 | Byte 1 | Direction | Description |
|--------|--------|-----------|-------------|
| `0x01` | 0/1 | ESP32 -> Phone | PTT state (button press/release) |
| `0x02` | 0-100 | ESP32 -> Phone | Battery level percentage |
| `0x03` | status | ESP32 -> Phone | Device status code |
| `0x04` | 0-100 | Phone -> ESP32 | Set volume |
| `0x05` | 0/1 | Phone -> ESP32 | Start/stop audio stream |

## Audio Codec: IMA-ADPCM

- 4:1 compression ratio
- 8kHz sample rate, 16-bit mono input
- Output: 32 kbps (well within BLE 5.0 throughput)
- Frame size: 160 samples (20ms) -> 84 bytes (4 header + 80 data)
- Header contains predictor value and step index for decoder sync
- Identical implementation in C (ESP32) and JavaScript (phone)

## Firmware Architecture

The ESP32 firmware runs on Arduino framework with a simple cooperative loop:

```
main.cpp
  |- buttons.update()    -- Debounce, detect short/long press
  |- battery.update()    -- Read ADC every 30s
  |- led.update()        -- Animate NeoPixel patterns
  |- bleAudio.update()   -- Process TX/RX audio frames
```

Audio processing runs synchronously in the main loop. At 8kHz with 160-sample frames, each frame takes 20ms. The I2S DMA handles buffering between the main loop iterations.

## Power Management

- **Deep sleep**: ~14-69 uA. Wake via GPIO1 (ext0 wakeup).
- **Active idle** (BLE advertising): ~10-20 mA
- **Active connected**: ~30-50 mA
- **Active streaming**: ~60-80 mA (mic + speaker + BLE)
- With a 1000mAh battery: ~8-12 hours active use, months in standby
