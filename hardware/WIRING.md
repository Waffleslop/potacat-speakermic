# Wiring Guide

## Pin Connections

### MAX98357A I2S Amplifier

| MAX98357A Pin | XIAO Pin | GPIO | Wire Color (suggested) |
|---------------|----------|------|------------------------|
| VIN           | 3V3      | —    | Red                    |
| GND           | GND      | —    | Black                  |
| BCLK          | D8       | 7    | Yellow                 |
| LRC           | D9       | 8    | Orange                 |
| DIN           | D10      | 9    | Green                  |
| GAIN          | —        | —    | Leave unconnected (15dB default) or tie to VIN for 18dB |
| SD            | —        | —    | Leave unconnected (always on) or tie to VIN |

Connect speaker + and - to the MAX98357A speaker output terminals.

### Buttons (all active LOW with internal pull-up)

| Button | XIAO Pin | GPIO | Function |
|--------|----------|------|----------|
| PTT    | D1       | 2    | Push-to-talk (momentary hold) |
| Power  | D0       | 1    | Long press: on/off. Short press: battery status. |
| Vol+   | D2       | 3    | Short: volume up. Long: max volume. |
| Vol-   | D3       | 4    | Short: volume down. Long: mute toggle. |

Each button connects between the XIAO pin and GND. No external resistors needed (firmware uses internal pull-ups).

```
XIAO Pin ----[Button]---- GND
```

### NeoPixel RGB LED

| NeoPixel Pin | XIAO Pin | GPIO |
|-------------|----------|------|
| VIN / 5V   | 3V3      | —    |
| GND         | GND      | —    |
| DIN         | D5       | 6    |

A single WS2812B NeoPixel works fine at 3.3V even though it's rated for 5V.

### Battery Voltage Divider

```
Battery + ----[200k]----+----[200k]---- GND
                        |
                    XIAO D4 (GPIO5)
```

Two 200k ohm resistors in series from battery positive to GND. The midpoint connects to GPIO5. This divides the battery voltage by 2 so the 3.0-4.2V range maps to 1.5-2.1V (safe for the 3.3V ADC).

### Battery

Solder battery wires directly to the XIAO's battery pads on the bottom of the board:
- **Negative (-)**: Pad closest to USB-C connector
- **Positive (+)**: Other pad

The XIAO has a built-in charging circuit — it charges via USB-C automatically.

## Complete Wiring Summary

```
                    XIAO ESP32S3 Sense
                   +------------------+
            USB-C  |                  |
                   |  D0 (GPIO1) ----+---- Power Button ---- GND
                   |  D1 (GPIO2) ----+---- PTT Button ------ GND
                   |  D2 (GPIO3) ----+---- Vol+ Button ----- GND
                   |  D3 (GPIO4) ----+---- Vol- Button ----- GND
                   |  D4 (GPIO5) ----+---- Voltage Divider midpoint
                   |  D5 (GPIO6) ----+---- NeoPixel DIN
                   |  D6 (GPIO43)    |     (free / debug TX)
                   |  D7 (GPIO44)    |     (free / debug RX)
                   |  D8 (GPIO7) ----+---- MAX98357A BCLK
                   |  D9 (GPIO8) ----+---- MAX98357A LRC
                   |  D10 (GPIO9) ---+---- MAX98357A DIN
                   |                  |
                   |  3V3 -----------+---- MAX98357A VIN
                   |                 +---- NeoPixel VIN
                   |  GND -----------+---- MAX98357A GND
                   |                 +---- NeoPixel GND
                   |                 +---- All button GND legs
                   |                 +---- Voltage divider GND
                   |                  |
                   |  BAT+ ----------+---- Battery +
                   |  BAT- ----------+---- Battery -
                   +------------------+
                   |  Sense Board     |
                   |  GPIO42 = PDM CLK|  (built-in mic)
                   |  GPIO41 = PDM DAT|  (built-in mic)
                   |  GPIO21 = LED    |  (built-in orange LED)
                   +------------------+
```

## Notes

- Keep wires to the MAX98357A short to minimize I2S signal noise
- The PDM microphone is built into the Sense expansion board — no wiring needed
- The built-in orange LED (GPIO21) lights during PTT as a secondary indicator
- After wiring, the camera module on the Sense board can be removed — it's not used
