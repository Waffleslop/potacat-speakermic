# Setup Guide

## Prerequisites

- A built speakermic (see [BOM](../hardware/BOM.md) and [Wiring Guide](../hardware/WIRING.md))
- [PlatformIO](https://platformio.org/) installed (CLI or VS Code extension)
- USB-C cable
- An Android phone with Chrome, or an iOS device with [Bluefy](https://apps.apple.com/app/bluefy-web-ble-browser/id1492822055)
- ECHOCAT running on your shack computer

## Step 1: Flash the Firmware

1. Clone this repository:
   ```
   git clone https://github.com/Waffleslop/potacat-speakermic.git
   cd potacat-speakermic/firmware
   ```

2. Connect the XIAO ESP32S3 Sense via USB-C

3. Build and upload:
   ```
   pio run -t upload
   ```

4. Monitor serial output (optional):
   ```
   pio device monitor
   ```

The device should play a startup sound and begin BLE advertising (blue LED pulsing).

## Step 2: Integrate with ECHOCAT

See [INTEGRATION.md](../echocat-integration/INTEGRATION.md) for detailed instructions on adding the speakermic JavaScript modules to your ECHOCAT build.

## Step 3: Connect

1. Open ECHOCAT on your phone
   - **Android**: Use Chrome
   - **iOS**: Use [Bluefy](https://apps.apple.com/app/bluefy-web-ble-browser/id1492822055)

2. Connect to your radio as usual (authenticate, connect audio)

3. Tap the **Speakermic** button in the ECHOCAT interface

4. Select **POTACAT-MIC** from the Bluetooth device picker

5. The speakermic LED turns solid green — you're connected!

## Step 4: Use It

- **PTT**: Hold the PTT button to transmit. LED turns red during TX.
- **Volume**: Press Vol+ / Vol- buttons to adjust speaker volume
- **Battery**: Short-press the Power button to check battery (LED flashes green/yellow/red)
- **Power off**: Long-press the Power button for 3 seconds

## Troubleshooting

### Device not showing in Bluetooth picker
- Make sure the speakermic is powered on (blue LED pulsing)
- Make sure you're using Chrome (Android) or Bluefy (iOS)
- Try power cycling the speakermic

### No audio from speaker
- Check the MAX98357A wiring (BCLK, LRC, DIN)
- Check that the speaker is connected to the amplifier
- Try increasing volume with the Vol+ button
- Make sure ECHOCAT audio is connected (not just the speakermic)

### Mic audio not transmitting
- Verify PTT is being pressed (LED should turn red)
- Check the ECHOCAT PTT indicator on screen
- The PDM microphone is on the Sense expansion board — make sure it's seated properly

### Connection drops frequently
- Move the phone closer to the speakermic
- BLE range is typically 10-30 meters (line of sight)
- Check battery level — low battery can cause BLE instability
