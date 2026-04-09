#include "led.h"

Led led;

void Led::begin() {
    _pixel = Adafruit_NeoPixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
    _pixel.begin();
    _pixel.setBrightness(30);  // Keep it dim to save power
    off();

    // Also set up built-in LED
    pinMode(PIN_LED_BUILTIN, OUTPUT);
    digitalWrite(PIN_LED_BUILTIN, HIGH);  // Active LOW — HIGH = off
}

void Led::update() {
    uint32_t now = millis();

    // Handle one-shot flash
    if (_flashActive) {
        if (now >= _flashEnd) {
            _flashActive = false;
            _state = _prevState;
            _stateStartTime = now;
        } else {
            return;  // Flash is showing, don't override
        }
    }

    switch (_state) {
        case LedState::OFF:
            off();
            break;

        case LedState::ADVERTISING_PULSE: {
            // Sine-wave pulse on blue channel
            uint32_t elapsed = (now - _stateStartTime) % LED_PULSE_PERIOD;
            float phase = (float)elapsed / LED_PULSE_PERIOD * 2.0f * PI;
            uint8_t brightness = (uint8_t)((sin(phase) + 1.0f) * 0.5f * 60.0f + 2.0f);
            _pixel.setPixelColor(0, 0, 0, brightness);
            _pixel.show();
            break;
        }

        case LedState::CONNECTED_IDLE:
            if ((now - _lastToggle) >= LED_BLINK_INTERVAL) {
                setColor(COLOR_GREEN);
                _lastToggle = now;
                _ledOn = true;
            } else if (_ledOn && (now - _lastToggle) >= LED_BLINK_DURATION) {
                off();
                _ledOn = false;
            }
            break;

        case LedState::PTT_ACTIVE:
            setColor(COLOR_RED);
            // Also light built-in LED during TX
            digitalWrite(PIN_LED_BUILTIN, LOW);  // Active LOW = on
            break;

        case LedState::LOW_BATTERY:
            if ((now - _lastToggle) >= LED_LOW_BATT_INTERVAL) {
                setColor(COLOR_YELLOW);
                _lastToggle = now;
                _ledOn = true;
            } else if (_ledOn && (now - _lastToggle) >= LED_BLINK_DURATION) {
                off();
                _ledOn = false;
            }
            break;

        case LedState::CRITICAL_BATTERY:
            if ((now - _lastToggle) >= LED_CRITICAL_INTERVAL) {
                _ledOn = !_ledOn;
                if (_ledOn) setColor(COLOR_RED);
                else off();
                _lastToggle = now;
            }
            break;

        case LedState::FLASH_BLUE:
            setColor(COLOR_BLUE);
            break;

        case LedState::FLASH_RED:
            setColor(COLOR_RED);
            break;
    }
}

void Led::setState(LedState state) {
    if (state == _state && !_flashActive) return;
    _prevState = _state;
    _state = state;
    _stateStartTime = millis();
    _lastToggle = 0;
    _ledOn = false;

    // Turn off built-in LED when leaving PTT
    if (_prevState == LedState::PTT_ACTIVE && state != LedState::PTT_ACTIVE) {
        digitalWrite(PIN_LED_BUILTIN, HIGH);
    }
}

void Led::flash(uint32_t color, uint16_t durationMs) {
    _prevState = _state;
    _flashActive = true;
    _flashEnd = millis() + durationMs;
    setColor(color);
}

void Led::setColor(uint32_t color) {
    _pixel.setPixelColor(0, color);
    _pixel.show();
}

void Led::off() {
    _pixel.setPixelColor(0, COLOR_OFF);
    _pixel.show();
}
