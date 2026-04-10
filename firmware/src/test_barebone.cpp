// ============================================================
// POTACAT Speakermic — Bare Board Test
// Tests ONLY built-in hardware: PDM mic, orange LED, BLE, serial
// No external components needed!
//
// Build:   pio run -e test
// Upload:  pio run -e test -t upload
// Monitor: pio device monitor
// ============================================================

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <driver/i2s.h>

// Built-in hardware pins (no external wiring needed)
#define PIN_LED         21      // Built-in orange LED (active LOW)
#define PIN_PDM_CLK     42      // PDM mic clock (Sense board)
#define PIN_PDM_DATA    41      // PDM mic data (Sense board)

// BLE
#define BLE_DEVICE_NAME "POTACAT-MIC"
#define BLE_SERVICE_UUID "f47ac10b-58cc-4372-a567-0e02b2c3d479"

// Audio
#define SAMPLE_RATE     8000
#define FRAME_SAMPLES   160

// Globals
static bool micReady = false;
static bool bleConnected = false;
static NimBLEServer* pServer = nullptr;
static uint32_t lastPrint = 0;
static uint32_t lastBlink = 0;
static bool ledOn = false;

// ---- BLE Callbacks ----
class ServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
        bleConnected = true;
        Serial.printf("[BLE] Client connected: %s\n", info.getAddress().toString().c_str());
    }
    void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
        bleConnected = false;
        Serial.printf("[BLE] Disconnected (reason %d). Restarting advertising...\n", reason);
        NimBLEDevice::getAdvertising()->start();
    }
    void onMTUChange(uint16_t mtu, NimBLEConnInfo& info) override {
        Serial.printf("[BLE] MTU changed to %d\n", mtu);
    }
};

// ---- PDM Mic Init (legacy I2S driver) ----
bool initMic() {
    i2s_config_t i2sCfg = {};
    i2sCfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    i2sCfg.sample_rate = SAMPLE_RATE;
    i2sCfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2sCfg.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    i2sCfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2sCfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2sCfg.dma_buf_count = 4;
    i2sCfg.dma_buf_len = FRAME_SAMPLES;
    i2sCfg.use_apll = false;

    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2sCfg, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("  -> FAIL: i2s_driver_install error %d\n", err);
        return false;
    }

    i2s_pin_config_t pinCfg = {};
    pinCfg.bck_io_num = I2S_PIN_NO_CHANGE;
    pinCfg.ws_io_num = PIN_PDM_CLK;
    pinCfg.data_out_num = I2S_PIN_NO_CHANGE;
    pinCfg.data_in_num = PIN_PDM_DATA;

    err = i2s_set_pin(I2S_NUM_0, &pinCfg);
    if (err != ESP_OK) {
        Serial.printf("  -> FAIL: i2s_set_pin error %d\n", err);
        return false;
    }

    return true;
}

// ---- Setup ----
void setup() {
    Serial.begin(115200);
    delay(1500);  // Give serial monitor time to connect
    Serial.println("\n========================================");
    Serial.println("  POTACAT Speakermic - Bare Board Test");
    Serial.println("========================================\n");

    // --- Test 1: Built-in LED ---
    Serial.println("[TEST 1] Built-in LED (GPIO21)");
    pinMode(PIN_LED, OUTPUT);
    for (int i = 0; i < 3; i++) {
        digitalWrite(PIN_LED, LOW);   // ON (active low)
        delay(200);
        digitalWrite(PIN_LED, HIGH);  // OFF
        delay(200);
    }
    Serial.println("  -> LED blinked 3 times. Did you see it? (orange LED near USB port)\n");

    // --- Test 2: PDM Microphone ---
    Serial.println("[TEST 2] PDM Microphone (GPIO42/41)");
    if (!initMic()) {
        Serial.println("  -> FAIL: Could not initialize PDM microphone!");
        Serial.println("  -> Is the Sense expansion board attached?\n");
    } else {
        micReady = true;
        Serial.println("  -> PDM microphone initialized at 8kHz/16-bit/mono");

        // Discard first read (often noisy)
        int16_t discard[FRAME_SAMPLES];
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_0, discard, sizeof(discard), &bytesRead, portMAX_DELAY);

        Serial.println("  -> Reading 5 frames... speak or tap the board!");
        for (int f = 0; f < 5; f++) {
            int16_t samples[FRAME_SAMPLES];
            esp_err_t err = i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
            if (err == ESP_OK && bytesRead > 0) {
                int count = bytesRead / sizeof(int16_t);
                int16_t minVal = 32767, maxVal = -32768;
                int64_t sum = 0;
                for (int i = 0; i < count; i++) {
                    if (samples[i] < minVal) minVal = samples[i];
                    if (samples[i] > maxVal) maxVal = samples[i];
                    sum += abs(samples[i]);
                }
                int avg = sum / count;
                Serial.printf("  -> Frame %d: %d samples, min=%d max=%d avg=%d  ",
                              f + 1, count, minVal, maxVal, avg);
                int bars = avg / 500;
                if (bars > 20) bars = 20;
                for (int i = 0; i < bars; i++) Serial.print("|");
                Serial.println();
            } else {
                Serial.printf("  -> Frame %d: read error %d\n", f + 1, err);
            }
            delay(100);
        }
        Serial.println("  -> Mic test complete. Higher avg = louder sound.\n");
    }

    // --- Test 3: BLE ---
    Serial.println("[TEST 3] Bluetooth Low Energy");
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setMTU(512);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCB());

    NimBLEService* svc = pServer->createService(BLE_SERVICE_UUID);
    svc->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setName(BLE_DEVICE_NAME);
    adv->enableScanResponse(true);
    adv->start();

    Serial.printf("  -> BLE advertising as '%s'\n", BLE_DEVICE_NAME);
    Serial.println("  -> Open 'nRF Connect' on your phone and look for POTACAT-MIC");
    Serial.println("  -> Try connecting to verify BLE works\n");

    // --- Summary ---
    Serial.println("========================================");
    Serial.println("  All tests running!");
    Serial.println("  LED:  blinking every 2s");
    Serial.println("  MIC:  printing levels every 2s (speak!)");
    Serial.println("  BLE:  advertising (connect with nRF Connect)");
    Serial.println("========================================\n");
}

// ---- Loop ----
void loop() {
    uint32_t now = millis();

    // Blink LED every 2 seconds
    if ((now - lastBlink) >= 1000) {
        ledOn = !ledOn;
        digitalWrite(PIN_LED, ledOn ? LOW : HIGH);
        lastBlink = now;

        // When BLE connected, double-blink
        if (bleConnected && !ledOn) {
            delay(100);
            digitalWrite(PIN_LED, LOW);
            delay(100);
            digitalWrite(PIN_LED, HIGH);
        }
    }

    // Print mic levels every 2 seconds
    if (micReady && (now - lastPrint) >= 2000) {
        int16_t samples[FRAME_SAMPLES];
        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(50));

        if (err == ESP_OK && bytesRead > 0) {
            int count = bytesRead / sizeof(int16_t);
            int64_t sum = 0;
            int16_t peak = 0;
            for (int i = 0; i < count; i++) {
                int16_t abs_val = abs(samples[i]);
                sum += abs_val;
                if (abs_val > peak) peak = abs_val;
            }
            int avg = sum / count;

            Serial.printf("[MIC] avg=%5d peak=%5d  BLE=%s  ",
                          avg, peak, bleConnected ? "CONNECTED" : "advertising");
            int bars = avg / 300;
            if (bars > 30) bars = 30;
            for (int i = 0; i < bars; i++) Serial.print("#");
            Serial.println();
        }
        lastPrint = now;
    }

    delay(10);
}
