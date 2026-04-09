#include "battery.h"

Battery battery;

void Battery::begin() {
    analogReadResolution(12);  // 12-bit ADC (0-4095)
    analogSetAttenuation(ADC_11db);  // Full range ~0-3.3V
    pinMode(PIN_BATTERY_ADC, INPUT);

    // Initial read
    readVoltage();
}

void Battery::update() {
    uint32_t now = millis();
    if ((now - _lastRead) >= BATTERY_READ_INTERVAL) {
        readVoltage();
        _lastRead = now;
    }
}

void Battery::readVoltage() {
    // Average multiple readings for stability
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogReadMilliVolts(PIN_BATTERY_ADC);
    }
    uint16_t adcMv = sum / 16;

    // Apply voltage divider factor
    _voltageMv = (uint16_t)(adcMv * BATTERY_DIVIDER_FACTOR);

    // Calculate percentage (simple linear mapping)
    _percent = voltageToPercent(_voltageMv);

    // Determine level
    if (_voltageMv < BATTERY_SHUTDOWN_MV) {
        _level = BatteryLevel::SHUTDOWN;
    } else if (_voltageMv < BATTERY_CRITICAL_MV) {
        _level = BatteryLevel::CRITICAL;
    } else if (_voltageMv < BATTERY_LOW_MV) {
        _level = BatteryLevel::LOW;
    } else if (_voltageMv < BATTERY_GOOD_MV) {
        _level = BatteryLevel::GOOD;
    } else {
        _level = BatteryLevel::FULL;
    }
}

uint8_t Battery::voltageToPercent(uint16_t mv) {
    // LiPo discharge curve approximation (3.0V=0%, 4.2V=100%)
    if (mv >= 4200) return 100;
    if (mv <= 3000) return 0;

    // Piecewise linear approximation of LiPo curve
    if (mv >= 4000) return 80 + (mv - 4000) * 20 / 200;
    if (mv >= 3700) return 30 + (mv - 3700) * 50 / 300;
    if (mv >= 3500) return 10 + (mv - 3500) * 20 / 200;
    return (mv - 3000) * 10 / 500;
}

bool Battery::needsWarning() {
    if (_level != BatteryLevel::LOW && _level != BatteryLevel::CRITICAL) {
        return false;
    }
    uint32_t now = millis();
    if ((now - _lastWarning) >= BATTERY_LOW_WARN_INTERVAL) {
        _lastWarning = now;
        return true;
    }
    return false;
}
