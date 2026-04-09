#include "buttons.h"

Buttons buttons;

void Buttons::begin() {
    _buttons[0] = { PIN_BTN_PTT,   ButtonId::PTT,   true, true, false, 0, 0, false };
    _buttons[1] = { PIN_BTN_POWER, ButtonId::POWER, true, true, false, 0, 0, false };
    _buttons[2] = { PIN_BTN_A,     ButtonId::BTN_A, true, true, false, 0, 0, false };
    _buttons[3] = { PIN_BTN_B,     ButtonId::BTN_B, true, true, false, 0, 0, false };

    for (auto& btn : _buttons) {
        pinMode(btn.pin, INPUT_PULLUP);
    }
}

void Buttons::update() {
    uint32_t now = millis();

    // PTT is handled specially (momentary hold, not press/release)
    processPtt(_buttons[0], now);

    // Other buttons: short press / long press
    for (int i = 1; i < 4; i++) {
        processButton(_buttons[i], now);
    }
}

void Buttons::setCallback(ButtonCallback cb) {
    _callback = cb;
}

void Buttons::processPtt(ButtonState& btn, uint32_t now) {
    bool reading = digitalRead(btn.pin) == LOW;  // Active LOW

    // Debounce
    if (reading != btn.lastReading) {
        btn.debounceTime = now;
    }
    btn.lastReading = reading;

    if ((now - btn.debounceTime) < BTN_DEBOUNCE_MS) return;

    bool newState = reading;
    if (newState != btn.stableState) {
        btn.stableState = newState;
        if (newState) {
            // PTT pressed
            _pttHeld = true;
            if (_callback) _callback(btn.id, ButtonEvent::PRESSED);
        } else {
            // PTT released
            _pttHeld = false;
            if (_callback) _callback(btn.id, ButtonEvent::RELEASED);
        }
    }
}

void Buttons::processButton(ButtonState& btn, uint32_t now) {
    bool reading = digitalRead(btn.pin) == LOW;  // Active LOW

    // Debounce
    if (reading != btn.lastReading) {
        btn.debounceTime = now;
    }
    btn.lastReading = reading;

    if ((now - btn.debounceTime) < BTN_DEBOUNCE_MS) return;

    bool pressed = reading;

    if (pressed && !btn.wasPressed) {
        // Just pressed down
        btn.wasPressed = true;
        btn.pressStartTime = now;
        btn.longPressFired = false;
    }

    if (pressed && btn.wasPressed && !btn.longPressFired) {
        // Check for long press while held
        uint32_t holdTime = (btn.id == ButtonId::POWER) ? BTN_POWER_OFF_MS : BTN_LONG_PRESS_MS;
        if ((now - btn.pressStartTime) >= holdTime) {
            btn.longPressFired = true;
            if (_callback) _callback(btn.id, ButtonEvent::LONG_PRESS);
        }
    }

    if (!pressed && btn.wasPressed) {
        // Released
        btn.wasPressed = false;
        if (!btn.longPressFired) {
            if (_callback) _callback(btn.id, ButtonEvent::SHORT_PRESS);
        }
    }
}
