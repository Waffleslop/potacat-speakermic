// ============================================================
// POTACAT Speakermic — Main Firmware
// Hardware: Seeed XIAO ESP32S3 Sense
// https://github.com/Waffleslop/potacat-speakermic
// ============================================================

#include <Arduino.h>
#include "config.h"
#include "input/buttons.h"
#include "input/battery.h"
#include "output/led.h"
#include "audio/mic.h"
#include "audio/speaker.h"
#include "audio/tones.h"
#include "audio/adpcm.h"
#include "ble/ble_service.h"
#include "ble/ble_audio.h"

// --- Device State Machine ---
enum class DeviceState {
    BOOTING,
    ACTIVE,         // BLE advertising or connected, ready to operate
    SHUTTING_DOWN,
    SLEEPING
};

static DeviceState deviceState = DeviceState::BOOTING;
static bool bleWasConnected = false;
static LedState prePttLedState = LedState::CONNECTED_IDLE;

// --- Forward declarations ---
void onButtonEvent(ButtonId id, ButtonEvent event);
void onBleControl(uint8_t cmd, uint8_t value);
void onBleRxAudio(const uint8_t* data, size_t len);
void enterSleep();
void updateBatteryStatus();

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    log_i("POTACAT Speakermic v%d.%d.%d starting...",
          FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);

    // Check wake reason
    esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();

    // Initialize subsystems
    led.begin();
    buttons.begin();
    battery.begin();

    // If woken from deep sleep, verify power button is still held
    if (wakeReason == ESP_SLEEP_WAKEUP_EXT0) {
        // Wait for button to be held for 2 seconds to confirm power on
        uint32_t start = millis();
        while (digitalRead(PIN_BTN_POWER) == LOW) {
            if ((millis() - start) >= BTN_LONG_PRESS_MS) {
                break;  // Confirmed power on
            }
            delay(10);
        }
        if (digitalRead(PIN_BTN_POWER) == HIGH) {
            // Button released too early — go back to sleep
            enterSleep();
            return;
        }
    }

    // Power on sequence
    deviceState = DeviceState::ACTIVE;
    led.setState(LedState::FLASH_BLUE);

    // Start speaker first (needed for startup sound)
    speaker.begin();
    tones.playStartup();

    // Initialize BLE
    bleService.begin();
    bleService.setControlCallback(onBleControl);
    bleService.setRxAudioCallback(onBleRxAudio);
    bleService.startAdvertising();

    // Initialize BLE audio pipeline
    bleAudio.begin();

    // Set up button callback
    buttons.setCallback(onButtonEvent);

    // Start LED advertising pattern
    led.setState(LedState::ADVERTISING_PULSE);

    log_i("Ready — BLE advertising as '%s'", BLE_DEVICE_NAME);
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
    if (deviceState != DeviceState::ACTIVE) return;

    // Update all subsystems
    buttons.update();
    battery.update();
    led.update();
    bleAudio.update();

    // Track BLE connection state changes
    bool bleConnected = bleService.isConnected();
    if (bleConnected != bleWasConnected) {
        bleWasConnected = bleConnected;
        if (bleConnected) {
            log_i("BLE connected — starting RX audio");
            tones.playConnected();
            bleAudio.startRx();
            led.setState(LedState::CONNECTED_IDLE);
        } else {
            log_i("BLE disconnected");
            tones.playDisconnected();
            bleAudio.stopTx();
            bleAudio.stopRx();
            led.setState(LedState::ADVERTISING_PULSE);
        }
    }

    // Battery monitoring
    updateBatteryStatus();

    // Small yield to prevent watchdog
    delay(1);
}

// ============================================================
// Button Handler
// ============================================================
void onButtonEvent(ButtonId id, ButtonEvent event) {
    switch (id) {
        case ButtonId::PTT:
            if (event == ButtonEvent::PRESSED) {
                if (!bleService.isConnected()) break;
                log_i("PTT pressed");
                bleAudio.startTx();
                bleService.sendControl(CMD_PTT, 1);
                prePttLedState = led.getState();
                led.setState(LedState::PTT_ACTIVE);
            } else if (event == ButtonEvent::RELEASED) {
                log_i("PTT released");
                bleAudio.stopTx();
                bleService.sendControl(CMD_PTT, 0);
                led.setState(prePttLedState);
            }
            break;

        case ButtonId::POWER:
            if (event == ButtonEvent::SHORT_PRESS) {
                // Battery status readout
                BatteryLevel level = battery.getLevel();
                uint8_t pct = battery.getPercent();
                log_i("Battery: %d%% (%dmV)", pct, battery.getVoltage());
                // Flash LED color based on level
                switch (level) {
                    case BatteryLevel::FULL:
                    case BatteryLevel::GOOD:
                        led.flash(0x004400, 500);  // Green
                        break;
                    case BatteryLevel::LOW:
                        led.flash(0x443300, 500);  // Yellow
                        break;
                    case BatteryLevel::CRITICAL:
                    case BatteryLevel::SHUTDOWN:
                        led.flash(0x440000, 500);  // Red
                        break;
                }
            } else if (event == ButtonEvent::LONG_PRESS) {
                // Power off
                log_i("Power off requested");
                deviceState = DeviceState::SHUTTING_DOWN;
                bleAudio.stopTx();
                bleAudio.stopRx();
                bleService.end();
                led.setState(LedState::FLASH_RED);
                tones.playShutdown();
                delay(300);
                enterSleep();
            }
            break;

        case ButtonId::BTN_A:
            if (event == ButtonEvent::SHORT_PRESS) {
                // Volume up
                uint8_t vol = speaker.getVolume();
                vol = min((int)vol + VOLUME_STEP, VOLUME_MAX);
                speaker.setVolume(vol);
                log_i("Volume: %d%%", vol);
            } else if (event == ButtonEvent::LONG_PRESS) {
                // Max volume
                speaker.setVolume(VOLUME_MAX);
                log_i("Volume: MAX");
            }
            break;

        case ButtonId::BTN_B:
            if (event == ButtonEvent::SHORT_PRESS) {
                // Volume down
                uint8_t vol = speaker.getVolume();
                vol = (vol >= VOLUME_STEP) ? vol - VOLUME_STEP : VOLUME_MIN;
                speaker.setVolume(vol);
                log_i("Volume: %d%%", vol);
            } else if (event == ButtonEvent::LONG_PRESS) {
                // Mute toggle
                static bool muted = false;
                static uint8_t savedVol = VOLUME_DEFAULT;
                muted = !muted;
                if (muted) {
                    savedVol = speaker.getVolume();
                    speaker.setVolume(0);
                    log_i("Muted");
                } else {
                    speaker.setVolume(savedVol);
                    log_i("Unmuted: %d%%", savedVol);
                }
            }
            break;
    }
}

// ============================================================
// BLE Control Command Handler
// ============================================================
void onBleControl(uint8_t cmd, uint8_t value) {
    switch (cmd) {
        case CMD_VOLUME:
            speaker.setVolume(value);
            log_i("Volume set to %d%% (via BLE)", value);
            break;

        case CMD_AUDIO_CTRL:
            if (value == 1) {
                bleAudio.startRx();
            } else {
                bleAudio.stopRx();
            }
            break;

        default:
            log_w("Unknown BLE control command: 0x%02X = %d", cmd, value);
            break;
    }
}

// ============================================================
// BLE RX Audio Handler
// ============================================================
void onBleRxAudio(const uint8_t* data, size_t len) {
    bleAudio.onRxData(data, len);
}

// ============================================================
// Battery Status
// ============================================================
void updateBatteryStatus() {
    BatteryLevel level = battery.getLevel();

    // Update BLE device info
    bleService.updateDeviceInfo(battery.getPercent());

    // Send battery level via control characteristic
    static uint32_t lastBattNotify = 0;
    uint32_t now = millis();
    if ((now - lastBattNotify) >= BATTERY_READ_INTERVAL) {
        bleService.sendControl(CMD_BATTERY, battery.getPercent());
        lastBattNotify = now;
    }

    // LED state based on battery
    if (level == BatteryLevel::CRITICAL) {
        if (led.getState() != LedState::PTT_ACTIVE) {
            led.setState(LedState::CRITICAL_BATTERY);
        }
    } else if (level == BatteryLevel::LOW) {
        if (led.getState() == LedState::CONNECTED_IDLE) {
            led.setState(LedState::LOW_BATTERY);
        }
    }

    // Audio warning
    if (battery.needsWarning()) {
        tones.playLowBattery();
    }

    // Auto shutdown on critically low battery
    if (level == BatteryLevel::SHUTDOWN) {
        log_w("Battery critically low — shutting down!");
        deviceState = DeviceState::SHUTTING_DOWN;
        bleAudio.stopTx();
        bleAudio.stopRx();
        bleService.end();
        tones.playLowBattery();
        delay(200);
        enterSleep();
    }
}

// ============================================================
// Deep Sleep
// ============================================================
void enterSleep() {
    led.setState(LedState::OFF);
    led.update();
    speaker.end();
    mic.end();

    // Configure wake on power button press (active LOW)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_POWER, 0);

    log_i("Entering deep sleep...");
    Serial.flush();
    delay(50);

    esp_deep_sleep_start();
}
