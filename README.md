# POTACAT Speakermic

A BLE-connected speaker/microphone for [POTACAT ECHOCAT](https://github.com/Waffleslop/POTACAT) remote ham radio operation. Built on the Seeed XIAO ESP32S3 Sense.

## What It Does

The POTACAT Speakermic is a small, battery-powered device that connects to your phone via Bluetooth Low Energy. It bridges into ECHOCAT's existing WebRTC audio connection, giving you a dedicated hardware PTT button, microphone, and speaker for hands-free remote radio operation.

**Talk into the mic** -> audio goes through your phone -> ECHOCAT -> your shack computer -> your radio transmits.

**Radio receives audio** -> your shack computer -> ECHOCAT -> your phone -> audio comes out the speakermic speaker.

## Features

- Push-to-talk button with haptic LED feedback
- Built-in PDM microphone (on Sense expansion board)
- I2S speaker output via MAX98357A amplifier
- RGB LED status indicator (advertising, connected, PTT, battery)
- Startup/shutdown/connection sound effects
- Battery voltage monitoring with low-battery warnings
- Deep sleep power management (months of standby)
- Volume control buttons
- Works on Android (Chrome) and iOS (Bluefy browser)

## Hardware (~$28)

| Component | Notes |
|-----------|-------|
| Seeed XIAO ESP32S3 Sense | Microcontroller with built-in mic and LiPo charging |
| MAX98357A I2S Amp | Drives the speaker from digital I2S audio |
| 8 ohm speaker | 28-40mm, fits in 3D-printed case |
| WS2812B NeoPixel | Single RGB LED for status |
| 4x tactile buttons | PTT, Power, Vol+, Vol- |
| 3.7V LiPo battery | 500-1000mAh |
| 2x 200k ohm resistors | Battery voltage divider |

See [hardware/BOM.md](hardware/BOM.md) for purchase links and [hardware/WIRING.md](hardware/WIRING.md) for assembly.

## Quick Start

### 1. Build the Hardware

Wire everything per the [wiring guide](hardware/WIRING.md) and assemble in your case.

### 2. Flash Firmware

```bash
cd firmware
pio run -t upload
```

### 3. Integrate with ECHOCAT

Add the JavaScript files from `echocat-integration/` to your ECHOCAT build. See [INTEGRATION.md](echocat-integration/INTEGRATION.md).

### 4. Connect and Use

Power on the speakermic (long-press Power), open ECHOCAT on your phone, tap **Speakermic**, select **POTACAT-MIC** from the BLE picker. Done.

## How It Works

The ESP32S3 runs a BLE GATT server with a custom audio service. Your phone's ECHOCAT web page connects via Web Bluetooth, receives microphone audio as ADPCM-encoded BLE notifications (TX path), and sends radio receive audio as ADPCM BLE writes (RX path). The phone bridges all audio into the existing WebRTC connection to your shack — **zero server-side changes needed**.

See [docs/architecture.md](docs/architecture.md) for the full technical deep-dive.

## Button Controls

| Button | Short Press | Long Press | Hold |
|--------|------------|------------|------|
| PTT | — | — | Transmit |
| Power | Battery check | On / Off | — |
| Vol+ | Volume up | Max volume | — |
| Vol- | Volume down | Mute toggle | — |

## LED States

| Color | Pattern | Meaning |
|-------|---------|---------|
| Blue | Slow pulse | BLE advertising (waiting for connection) |
| Green | Brief blink every 10s | Connected, idle |
| Red | Solid | PTT active (transmitting) |
| Yellow | Blink every 5s | Low battery |
| Red | Fast blink | Critical battery |

## iOS Support

Safari doesn't support Web Bluetooth. iOS users should use [Bluefy](https://apps.apple.com/app/bluefy-web-ble-browser/id1492822055) (free). See [docs/ios-setup.md](docs/ios-setup.md).

## Project Structure

```
firmware/           ESP32 firmware (Arduino/C++, PlatformIO)
echocat-integration/  JavaScript modules for ECHOCAT phone page
hardware/           BOM, wiring guide, 3D print files
docs/               Architecture, setup guides
```

## License

MIT
