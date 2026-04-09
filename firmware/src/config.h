#pragma once

// ============================================================
// POTACAT Speakermic — Pin & Constant Configuration
// Hardware: Seeed XIAO ESP32S3 Sense
// ============================================================

// --- I2S Speaker Output (MAX98357A) ---
#define PIN_I2S_BCLK        7   // D8 — Bit clock
#define PIN_I2S_LRCLK       8   // D9 — Word select (left/right clock)
#define PIN_I2S_DOUT        9   // D10 — Data out

// --- PDM Microphone (built-in on Sense board) ---
#define PIN_PDM_CLK         42  // Sense board — PDM clock
#define PIN_PDM_DATA        41  // Sense board — PDM data

// --- Buttons (active LOW, internal pull-up) ---
#define PIN_BTN_PTT         2   // D1 — Push-to-talk (momentary hold)
#define PIN_BTN_A           3   // D2 — Volume up / function
#define PIN_BTN_B           4   // D3 — Volume down / function
#define PIN_BTN_POWER       1   // D0 — Power on/off (RTC-capable for deep sleep wake)

// --- Battery ADC ---
#define PIN_BATTERY_ADC     5   // D4 — Via 2x 200k ohm voltage divider

// --- LEDs ---
#define PIN_NEOPIXEL        6   // D5 — WS2812B single RGB LED
#define PIN_LED_BUILTIN     21  // Built-in orange LED (active LOW)

// --- UART Debug (free pins) ---
#define PIN_DEBUG_TX        43  // D6
#define PIN_DEBUG_RX        44  // D7

// ============================================================
// Audio Configuration
// ============================================================
#define AUDIO_SAMPLE_RATE       8000    // 8kHz for voice
#define AUDIO_BIT_DEPTH         16      // 16-bit samples
#define AUDIO_CHANNELS          1       // Mono

// ADPCM frame: 160 samples = 20ms at 8kHz
#define ADPCM_FRAME_SAMPLES     160
#define ADPCM_FRAME_DATA_BYTES  80      // 160 samples * 4 bits / 8
#define ADPCM_FRAME_HEADER      4       // predictor (2) + step index (2)
#define ADPCM_FRAME_TOTAL_BYTES 84      // header + data

// Audio buffers
#define AUDIO_BUFFER_FRAMES     4       // Ring buffer depth
#define SPEAKER_BUFFER_MS       60      // Jitter buffer before playback starts

// ============================================================
// BLE Configuration
// ============================================================
#define BLE_DEVICE_NAME         "POTACAT-MIC"
#define BLE_MTU_SIZE            512

// Service UUID
#define BLE_SERVICE_UUID        "f47ac10b-58cc-4372-a567-0e02b2c3d479"

// Characteristic UUIDs
#define BLE_CHAR_TX_AUDIO_UUID  "f47ac10b-58cc-4372-a567-0e02b2c30001"
#define BLE_CHAR_RX_AUDIO_UUID  "f47ac10b-58cc-4372-a567-0e02b2c30002"
#define BLE_CHAR_CONTROL_UUID   "f47ac10b-58cc-4372-a567-0e02b2c30003"
#define BLE_CHAR_DEVICE_UUID    "f47ac10b-58cc-4372-a567-0e02b2c30004"

// BLE connection parameters
#define BLE_MIN_INTERVAL        8       // 10ms (units of 1.25ms)
#define BLE_MAX_INTERVAL        12      // 15ms
#define BLE_LATENCY             0
#define BLE_TIMEOUT             200     // 2000ms

// ============================================================
// Control Protocol Commands
// ============================================================
#define CMD_PTT             0x01    // PTT state: 0=off, 1=on
#define CMD_BATTERY         0x02    // Battery level: 0-100%
#define CMD_STATUS          0x03    // Device status
#define CMD_VOLUME          0x04    // Volume set: 0-100%
#define CMD_AUDIO_CTRL      0x05    // Audio control: 0=stop, 1=start

// Device status values
#define STATUS_IDLE         0x00
#define STATUS_ADVERTISING  0x01
#define STATUS_CONNECTED    0x02
#define STATUS_STREAMING    0x03
#define STATUS_LOW_BATTERY  0x04

// ============================================================
// Button Timing (ms)
// ============================================================
#define BTN_DEBOUNCE_MS         30
#define BTN_LONG_PRESS_MS       2000
#define BTN_POWER_OFF_MS        3000

// ============================================================
// Battery Thresholds (millivolts)
// ============================================================
#define BATTERY_FULL_MV         4200    // Fully charged
#define BATTERY_GOOD_MV         3700    // Good
#define BATTERY_LOW_MV          3500    // Low warning
#define BATTERY_CRITICAL_MV     3300    // Critical warning
#define BATTERY_SHUTDOWN_MV     3100    // Auto power off
#define BATTERY_READ_INTERVAL   30000   // Read every 30 seconds
#define BATTERY_LOW_WARN_INTERVAL 60000 // Warning beep every 60s

// Voltage divider: 2x 200k ohm, factor = 2.0
#define BATTERY_DIVIDER_FACTOR  2.0f

// ============================================================
// LED Timing (ms)
// ============================================================
#define LED_PULSE_PERIOD        2000    // BLE advertising pulse cycle
#define LED_BLINK_INTERVAL      10000   // Connected idle blink
#define LED_BLINK_DURATION      100     // Brief flash duration
#define LED_LOW_BATT_INTERVAL   5000    // Low battery blink
#define LED_CRITICAL_INTERVAL   250     // Critical battery fast blink

// ============================================================
// Speaker Volume
// ============================================================
#define VOLUME_DEFAULT          70      // Default volume (0-100)
#define VOLUME_STEP             10      // Volume change per button press
#define VOLUME_MIN              0
#define VOLUME_MAX              100

// ============================================================
// Firmware Version
// ============================================================
#define FW_VERSION_MAJOR        0
#define FW_VERSION_MINOR        1
#define FW_VERSION_PATCH        0
