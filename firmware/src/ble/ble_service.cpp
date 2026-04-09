#include "ble_service.h"

BleService bleService;

bool BleService::begin() {
    if (_running) return true;

    log_i("Initializing BLE...");

    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setMTU(BLE_MTU_SIZE);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // Max TX power for range

    // Create server
    _server = NimBLEDevice::createServer();
    _server->setCallbacks(this);

    // Create service
    _service = _server->createService(BLE_SERVICE_UUID);

    // TX Audio characteristic (ESP32 → Phone): Notify
    _txAudioChar = _service->createCharacteristic(
        BLE_CHAR_TX_AUDIO_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    // RX Audio characteristic (Phone → ESP32): Write Without Response
    _rxAudioChar = _service->createCharacteristic(
        BLE_CHAR_RX_AUDIO_UUID,
        NIMBLE_PROPERTY::WRITE_NR
    );
    _rxAudioChar->setCallbacks(this);

    // Control characteristic: Read + Write + Notify
    _controlChar = _service->createCharacteristic(
        BLE_CHAR_CONTROL_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );
    _controlChar->setCallbacks(this);

    // Device Info characteristic: Read + Notify
    _deviceChar = _service->createCharacteristic(
        BLE_CHAR_DEVICE_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // Set initial device info
    uint8_t info[4] = {
        100,                // Battery percent
        FW_VERSION_MAJOR,
        FW_VERSION_MINOR,
        FW_VERSION_PATCH
    };
    _deviceChar->setValue(info, sizeof(info));

    // Start service
    _service->start();

    _running = true;
    log_i("BLE service started");
    return true;
}

void BleService::end() {
    if (!_running) return;
    stopAdvertising();
    NimBLEDevice::deinit(true);
    _running = false;
    _connected = false;
}

void BleService::startAdvertising() {
    if (!_running) return;

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setName(BLE_DEVICE_NAME);
    adv->enableScanResponse(true);
    adv->setMinPreferred(0x06);  // Helps with iPhone connections
    adv->start();

    log_i("BLE advertising started");
}

void BleService::stopAdvertising() {
    if (!_running) return;
    NimBLEDevice::getAdvertising()->stop();
}

bool BleService::sendAudioFrame(const uint8_t* data, size_t len) {
    if (!_connected || !_txAudioChar) return false;

    _txAudioChar->setValue(data, len);
    _txAudioChar->notify();
    return true;
}

bool BleService::sendControl(uint8_t cmd, uint8_t value) {
    if (!_connected || !_controlChar) return false;

    uint8_t buf[2] = { cmd, value };
    _controlChar->setValue(buf, 2);
    _controlChar->notify();
    return true;
}

void BleService::updateDeviceInfo(uint8_t batteryPercent) {
    if (!_deviceChar) return;

    uint8_t info[4] = {
        batteryPercent,
        FW_VERSION_MAJOR,
        FW_VERSION_MINOR,
        FW_VERSION_PATCH
    };
    _deviceChar->setValue(info, sizeof(info));

    if (_connected) {
        _deviceChar->notify();
    }
}

// --- Server Callbacks ---

void BleService::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    _connected = true;
    log_i("BLE client connected: %s", connInfo.getAddress().toString().c_str());

    // Request optimal connection parameters for audio streaming
    pServer->updateConnParams(
        connInfo.getConnHandle(),
        BLE_MIN_INTERVAL, BLE_MAX_INTERVAL,
        BLE_LATENCY, BLE_TIMEOUT
    );
}

void BleService::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    _connected = false;
    log_i("BLE client disconnected (reason: %d)", reason);

    // Restart advertising so phone can reconnect
    startAdvertising();
}

void BleService::onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) {
    _mtu = MTU;
    log_i("MTU updated to %d", MTU);
}

// --- Characteristic Callbacks ---

void BleService::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) {
    NimBLEAttValue val = pChar->getValue();
    const uint8_t* data = val.data();
    size_t len = val.size();

    if (pChar == _rxAudioChar) {
        // Received audio data from phone (radio RX → speaker)
        if (_rxAudioCb && len > 0) {
            _rxAudioCb(data, len);
        }
    } else if (pChar == _controlChar) {
        // Received control command from phone
        if (_controlCb && len >= 2) {
            _controlCb(data[0], data[1]);
        }
    }
}
