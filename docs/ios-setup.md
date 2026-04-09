# iOS Setup Guide

Safari on iOS does not support the Web Bluetooth API. To use the POTACAT Speakermic with an iPhone or iPad, use the **Bluefy** browser.

## Step 1: Install Bluefy

Download [Bluefy - Web BLE Browser](https://apps.apple.com/app/bluefy-web-ble-browser/id1492822055) from the App Store. It's free.

Bluefy is a web browser that adds Web Bluetooth support, allowing websites to connect to BLE devices — exactly what the speakermic needs.

## Step 2: Open ECHOCAT in Bluefy

1. Open Bluefy
2. Navigate to your ECHOCAT URL (e.g., `https://100.x.x.x:7300`)
3. Accept the self-signed certificate (same as you do in Safari)
4. Log in to ECHOCAT as usual

## Step 3: Connect the Speakermic

1. Connect ECHOCAT audio as usual (tap "Connect Audio")
2. Tap the **Speakermic** button
3. Bluefy will show the Bluetooth device picker
4. Select **POTACAT-MIC**
5. Audio is now routed through the speakermic

## Notes

- Bluefy uses iOS's WebKit engine (same as Safari) for everything except Web Bluetooth, which it implements via CoreBluetooth
- ECHOCAT features (spots, logging, FT8, etc.) all work normally in Bluefy
- You only need Bluefy for the speakermic connection — if you're not using the speakermic, Safari works fine
- Bluefy may need Location permission enabled for BLE scanning (iOS requirement)

## Alternative: Future Native App

A dedicated iOS companion app using CoreBluetooth is planned for a future release, which would eliminate the need for Bluefy.
