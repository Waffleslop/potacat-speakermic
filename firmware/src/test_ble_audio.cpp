// ============================================================
// POTACAT Speakermic — BLE Audio Test
// Streams mic audio over BLE when PTT (D1/GPIO2) is shorted to GND.
// No external hardware needed except a wire for PTT.
//
// Build:   pio run -e test-audio
// Upload:  pio run -e test-audio -t upload --upload-port COM23
// Monitor: pio device monitor --port COM23
// ============================================================

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <driver/i2s.h>

// ---- Pin Definitions ----
#define PIN_LED         21
#define PIN_PDM_CLK     42
#define PIN_PDM_DATA    41
#define PIN_PTT         2   // D1 — short to GND for PTT

// ---- Audio Config ----
#define SAMPLE_RATE     8000
#define FRAME_SAMPLES   160
#define ADPCM_HEADER    4
#define ADPCM_DATA      80
#define ADPCM_TOTAL     84

// ---- BLE UUIDs ----
#define BLE_NAME        "POTACAT-MIC"
#define SVC_UUID        "f47ac10b-58cc-4372-a567-0e02b2c3d479"
#define TX_AUDIO_UUID   "f47ac10b-58cc-4372-a567-0e02b2c30001"
#define RX_AUDIO_UUID   "f47ac10b-58cc-4372-a567-0e02b2c30002"
#define CONTROL_UUID    "f47ac10b-58cc-4372-a567-0e02b2c30003"
#define DEVICE_UUID     "f47ac10b-58cc-4372-a567-0e02b2c30004"

#define CMD_PTT     0x01
#define CMD_BATTERY 0x02
#define CMD_AUDIO   0x05

// ============================================================
// IMA-ADPCM Codec (inline for single-file test)
// ============================================================
static const int16_t stepTable[89] = {
    7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
    50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
    337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,
    1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,
    7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,
    24623,27086,29794,32767
};
static const int8_t indexTable[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};

struct AdpcmState { int16_t predicted; uint8_t stepIndex; };

static int16_t clamp16(int32_t v) { return v > 32767 ? 32767 : (v < -32768 ? -32768 : v); }
static uint8_t clampIdx(int32_t i) { return i < 0 ? 0 : (i > 88 ? 88 : i); }

int adpcmEncode(const int16_t* pcm, int n, uint8_t* out, AdpcmState& st) {
    out[0] = st.predicted & 0xFF;
    out[1] = (st.predicted >> 8) & 0xFF;
    out[2] = st.stepIndex;
    out[3] = 0;
    uint8_t* d = out + ADPCM_HEADER;
    int bi = 0; bool hi = false;
    for (int i = 0; i < n; i++) {
        int16_t step = stepTable[st.stepIndex];
        int32_t diff = pcm[i] - st.predicted;
        uint8_t nib = 0;
        if (diff < 0) { nib = 8; diff = -diff; }
        if (diff >= step) { nib |= 4; diff -= step; }
        if (diff >= (step>>1)) { nib |= 2; diff -= (step>>1); }
        if (diff >= (step>>2)) { nib |= 1; }
        // Decode to update predictor
        int32_t delta = (stepTable[st.stepIndex] >> 3);
        if (nib & 4) delta += stepTable[st.stepIndex];
        if (nib & 2) delta += (stepTable[st.stepIndex] >> 1);
        if (nib & 1) delta += (stepTable[st.stepIndex] >> 2);
        if (nib & 8) delta = -delta;
        st.predicted = clamp16(st.predicted + delta);
        st.stepIndex = clampIdx(st.stepIndex + indexTable[nib]);
        if (!hi) { d[bi] = nib & 0x0F; hi = true; }
        else { d[bi] |= (nib << 4); bi++; hi = false; }
    }
    return ADPCM_HEADER + bi + (hi ? 1 : 0);
}

// ============================================================
// Globals
// ============================================================
static bool bleConnected = false;
static bool pttActive = false;
static bool prevPtt = false;
static AdpcmState txState = {0, 0};
static uint32_t lastBlink = 0;
static uint32_t frameCount = 0;

static NimBLECharacteristic* txAudioChar = nullptr;
static NimBLECharacteristic* controlChar = nullptr;
static NimBLECharacteristic* deviceChar = nullptr;

// ---- BLE Callbacks ----
class ServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
        bleConnected = true;
        Serial.printf("[BLE] Connected: %s\n", info.getAddress().toString().c_str());
        s->updateConnParams(info.getConnHandle(), 8, 12, 0, 200);
    }
    void onDisconnect(NimBLEServer* s, NimBLEConnInfo& info, int reason) override {
        bleConnected = false;
        Serial.printf("[BLE] Disconnected (%d)\n", reason);
        NimBLEDevice::getAdvertising()->start();
    }
    void onMTUChange(uint16_t mtu, NimBLEConnInfo& info) override {
        Serial.printf("[BLE] MTU=%d\n", mtu);
    }
};

class CharCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& info) override {
        NimBLEAttValue val = pChar->getValue();
        if (val.size() >= 2) {
            Serial.printf("[BLE] Control write: cmd=0x%02X val=%d\n", val[0], val[1]);
        } else if (val.size() > 0) {
            Serial.printf("[BLE] RX audio: %d bytes\n", val.size());
        }
    }
};
static CharCB charCB;

// ============================================================
// Setup
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n==========================================");
    Serial.println("  POTACAT Speakermic - BLE Audio Test");
    Serial.println("  Short D1 (GPIO2) to GND for PTT");
    Serial.println("==========================================\n");

    // LED
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH); // off

    // PTT button
    pinMode(PIN_PTT, INPUT_PULLUP);

    // --- PDM Microphone ---
    i2s_config_t i2sCfg = {};
    i2sCfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    i2sCfg.sample_rate = SAMPLE_RATE;
    i2sCfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2sCfg.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    i2sCfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2sCfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2sCfg.dma_buf_count = 6;
    i2sCfg.dma_buf_len = FRAME_SAMPLES;
    i2sCfg.use_apll = false;

    if (i2s_driver_install(I2S_NUM_0, &i2sCfg, 0, NULL) != ESP_OK) {
        Serial.println("[MIC] FAIL: driver install");
    }
    i2s_pin_config_t pinCfg = {};
    pinCfg.bck_io_num = I2S_PIN_NO_CHANGE;
    pinCfg.ws_io_num = PIN_PDM_CLK;
    pinCfg.data_out_num = I2S_PIN_NO_CHANGE;
    pinCfg.data_in_num = PIN_PDM_DATA;
    if (i2s_set_pin(I2S_NUM_0, &pinCfg) != ESP_OK) {
        Serial.println("[MIC] FAIL: set pin");
    }
    Serial.println("[MIC] PDM mic ready at 8kHz");

    // Flush initial noisy samples
    int16_t flush[FRAME_SAMPLES];
    size_t br;
    for (int i = 0; i < 5; i++)
        i2s_read(I2S_NUM_0, flush, sizeof(flush), &br, portMAX_DELAY);

    // --- BLE ---
    NimBLEDevice::init(BLE_NAME);
    NimBLEDevice::setMTU(512);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCB());

    NimBLEService* svc = server->createService(SVC_UUID);

    txAudioChar = svc->createCharacteristic(TX_AUDIO_UUID, NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic* rxAudioChar = svc->createCharacteristic(
        RX_AUDIO_UUID, NIMBLE_PROPERTY::WRITE_NR);
    rxAudioChar->setCallbacks(&charCB);

    controlChar = svc->createCharacteristic(
        CONTROL_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    controlChar->setCallbacks(&charCB);

    deviceChar = svc->createCharacteristic(
        DEVICE_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    uint8_t info[4] = {100, 0, 1, 0}; // batt=100%, v0.1.0
    deviceChar->setValue(info, 4);

    svc->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SVC_UUID);
    adv->setName(BLE_NAME);
    adv->enableScanResponse(true);
    adv->start();

    Serial.println("[BLE] Advertising as POTACAT-MIC");
    Serial.println("[BLE] Service UUID: " SVC_UUID);
    Serial.println("\nReady! Short D1 to GND to start streaming mic audio.\n");
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
    uint32_t now = millis();

    // --- PTT detection ---
    bool pttPin = (digitalRead(PIN_PTT) == LOW);

    if (pttPin != prevPtt) {
        prevPtt = pttPin;
        if (pttPin && bleConnected) {
            // PTT pressed
            pttActive = true;
            txState = {0, 0}; // Reset ADPCM state
            frameCount = 0;
            digitalWrite(PIN_LED, LOW); // LED on
            Serial.println("[PTT] ON — streaming mic audio");

            // Notify phone
            uint8_t cmd[2] = {CMD_PTT, 1};
            controlChar->setValue(cmd, 2);
            controlChar->notify();
        } else if (!pttPin) {
            // PTT released
            if (pttActive) {
                Serial.printf("[PTT] OFF — sent %d frames\n", frameCount);
            }
            pttActive = false;
            digitalWrite(PIN_LED, HIGH); // LED off

            if (bleConnected) {
                uint8_t cmd[2] = {CMD_PTT, 0};
                controlChar->setValue(cmd, 2);
                controlChar->notify();
            }
        }
    }

    // --- Stream mic audio during PTT ---
    if (pttActive && bleConnected) {
        int16_t pcm[FRAME_SAMPLES];
        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_NUM_0, pcm, sizeof(pcm), &bytesRead, pdMS_TO_TICKS(25));

        if (err == ESP_OK && bytesRead >= FRAME_SAMPLES * 2) {
            uint8_t adpcm[ADPCM_TOTAL];
            int len = adpcmEncode(pcm, FRAME_SAMPLES, adpcm, txState);

            txAudioChar->setValue(adpcm, len);
            txAudioChar->notify();
            frameCount++;

            // Print level every 25 frames (~500ms)
            if (frameCount % 25 == 0) {
                int64_t sum = 0;
                for (int i = 0; i < FRAME_SAMPLES; i++) sum += abs(pcm[i]);
                int avg = sum / FRAME_SAMPLES;
                Serial.printf("[TX] frame=%d avg=%d\n", frameCount, avg);
            }
        }
    } else {
        // Not streaming — drain mic buffer to keep it fresh
        int16_t drain[FRAME_SAMPLES];
        size_t br;
        i2s_read(I2S_NUM_0, drain, sizeof(drain), &br, pdMS_TO_TICKS(5));
    }

    // --- LED blink when connected but not PTT ---
    if (!pttActive) {
        if (bleConnected) {
            // Brief green blink every 3s (using orange LED)
            if ((now - lastBlink) >= 3000) {
                digitalWrite(PIN_LED, LOW);
                delay(50);
                digitalWrite(PIN_LED, HIGH);
                lastBlink = now;
            }
        } else {
            // Slow blink when advertising
            if ((now - lastBlink) >= 1000) {
                static bool advLed = false;
                advLed = !advLed;
                digitalWrite(PIN_LED, advLed ? LOW : HIGH);
                lastBlink = now;
            }
        }
    }

    delay(1);
}
