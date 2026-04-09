#pragma once

#include <Arduino.h>
#include "../config.h"

enum class BatteryLevel {
    FULL,       // >3.7V
    GOOD,       // 3.5-3.7V
    LOW,        // 3.3-3.5V
    CRITICAL,   // <3.3V
    SHUTDOWN    // <3.1V
};

class Battery {
public:
    void begin();
    void update();  // Call every loop iteration

    uint16_t getVoltage() const { return _voltageMv; }
    uint8_t getPercent() const { return _percent; }
    BatteryLevel getLevel() const { return _level; }
    bool needsWarning();  // Returns true when it's time to play warning sound

private:
    uint16_t _voltageMv = 4200;
    uint8_t _percent = 100;
    BatteryLevel _level = BatteryLevel::FULL;
    uint32_t _lastRead = 0;
    uint32_t _lastWarning = 0;

    void readVoltage();
    uint8_t voltageToPercent(uint16_t mv);
};

extern Battery battery;
