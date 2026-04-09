#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../config.h"

// Forward declaration
class BleAudio;

// Callback for received control commands from phone
typedef void (*ControlCallback)(uint8_t cmd, uint8_t value);

// Callback for received audio data from phone (RX path: radio → speaker)
typedef void (*RxAudioCallback)(const uint8_t* data, size_t len);

class BleService : public NimBLEServerCallbacks,
                   public NimBLECharacteristicCallbacks {
public:
    bool begin();
    void end();

    // Advertising
    void startAdvertising();
    void stopAdvertising();

    // Send audio frame to phone (TX path: mic → phone)
    bool sendAudioFrame(const uint8_t* data, size_t len);

    // Send control notification to phone
    bool sendControl(uint8_t cmd, uint8_t value);

    // Update device info (battery, version)
    void updateDeviceInfo(uint8_t batteryPercent);

    // Set callbacks
    void setControlCallback(ControlCallback cb) { _controlCb = cb; }
    void setRxAudioCallback(RxAudioCallback cb) { _rxAudioCb = cb; }

    bool isConnected() const { return _connected; }
    bool isRunning() const { return _running; }

private:
    // NimBLEServerCallbacks
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override;

    // NimBLECharacteristicCallbacks
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override;

    NimBLEServer* _server = nullptr;
    NimBLEService* _service = nullptr;
    NimBLECharacteristic* _txAudioChar = nullptr;   // Notify: mic audio to phone
    NimBLECharacteristic* _rxAudioChar = nullptr;   // Write: speaker audio from phone
    NimBLECharacteristic* _controlChar = nullptr;   // RW + Notify: commands
    NimBLECharacteristic* _deviceChar = nullptr;    // Read + Notify: device info

    ControlCallback _controlCb = nullptr;
    RxAudioCallback _rxAudioCb = nullptr;

    bool _running = false;
    bool _connected = false;
    uint16_t _mtu = 23;  // Default BLE MTU
};

extern BleService bleService;
