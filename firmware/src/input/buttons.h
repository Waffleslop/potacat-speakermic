#pragma once

#include <Arduino.h>
#include "../config.h"

// Button event types
enum class ButtonEvent {
    NONE,
    SHORT_PRESS,
    LONG_PRESS,
    PRESSED,        // Momentary — currently held down
    RELEASED        // Momentary — just released
};

// Button ID
enum class ButtonId {
    PTT,
    POWER,
    BTN_A,
    BTN_B
};

// Callback signature: void onButton(ButtonId id, ButtonEvent event)
typedef void (*ButtonCallback)(ButtonId id, ButtonEvent event);

class Buttons {
public:
    void begin();
    void update();  // Call every loop iteration
    void setCallback(ButtonCallback cb);

    bool isPttHeld() const { return _pttHeld; }

private:
    struct ButtonState {
        uint8_t pin;
        ButtonId id;
        bool lastReading;
        bool stableState;
        bool wasPressed;
        uint32_t debounceTime;
        uint32_t pressStartTime;
        bool longPressFired;
    };

    ButtonState _buttons[4];
    ButtonCallback _callback = nullptr;
    bool _pttHeld = false;

    void processButton(ButtonState& btn, uint32_t now);
    void processPtt(ButtonState& btn, uint32_t now);
};

extern Buttons buttons;
