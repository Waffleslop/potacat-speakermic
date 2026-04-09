#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "../config.h"

enum class LedState {
    OFF,
    ADVERTISING_PULSE,  // Blue slow pulse
    CONNECTED_IDLE,     // Green blink every 10s
    PTT_ACTIVE,         // Red solid
    LOW_BATTERY,        // Yellow blink every 5s
    CRITICAL_BATTERY,   // Red fast blink
    FLASH_BLUE,         // Power on flash
    FLASH_RED           // Power off flash
};

class Led {
public:
    void begin();
    void update();  // Call every loop iteration
    void setState(LedState state);
    LedState getState() const { return _state; }

    // One-shot flash (returns to previous state after duration)
    void flash(uint32_t color, uint16_t durationMs);

private:
    Adafruit_NeoPixel _pixel;
    LedState _state = LedState::OFF;
    LedState _prevState = LedState::OFF;
    uint32_t _stateStartTime = 0;
    uint32_t _lastToggle = 0;
    bool _flashActive = false;
    uint32_t _flashEnd = 0;
    bool _ledOn = false;

    void setColor(uint32_t color);
    void off();

    // Colors
    static constexpr uint32_t COLOR_BLUE    = 0x000044;
    static constexpr uint32_t COLOR_GREEN   = 0x004400;
    static constexpr uint32_t COLOR_RED     = 0x440000;
    static constexpr uint32_t COLOR_YELLOW  = 0x443300;
    static constexpr uint32_t COLOR_OFF     = 0x000000;
};

extern Led led;
