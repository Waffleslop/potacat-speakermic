#include "mic.h"

Mic mic;

bool Mic::begin() {
    if (_running) return true;

    // Configure I2S channel (PDM RX on I2S0)
    i2s_chan_config_t chanCfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 4,
        .dma_frame_num = ADPCM_FRAME_SAMPLES,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };

    esp_err_t err = i2s_new_channel(&chanCfg, nullptr, &_rxHandle);
    if (err != ESP_OK) {
        log_e("Failed to create I2S RX channel: %d", err);
        return false;
    }

    // Configure PDM RX
    i2s_pdm_rx_config_t pdmCfg = {
        .clk_cfg = {
            .sample_rate_hz = AUDIO_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .dn_sample_mode = I2S_PDM_DSR_8S,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_PDM_SLOT_RIGHT,  // Built-in mic is on right channel
        },
        .gpio_cfg = {
            .clk = (gpio_num_t)PIN_PDM_CLK,
            .din = (gpio_num_t)PIN_PDM_DATA,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    err = i2s_channel_init_pdm_rx_mode(_rxHandle, &pdmCfg);
    if (err != ESP_OK) {
        log_e("Failed to init PDM RX: %d", err);
        i2s_del_channel(_rxHandle);
        _rxHandle = nullptr;
        return false;
    }

    err = i2s_channel_enable(_rxHandle);
    if (err != ESP_OK) {
        log_e("Failed to enable I2S RX: %d", err);
        i2s_del_channel(_rxHandle);
        _rxHandle = nullptr;
        return false;
    }

    _running = true;
    log_i("PDM microphone started at %d Hz", AUDIO_SAMPLE_RATE);
    return true;
}

void Mic::end() {
    if (!_running) return;
    if (_rxHandle) {
        i2s_channel_disable(_rxHandle);
        i2s_del_channel(_rxHandle);
        _rxHandle = nullptr;
    }
    _running = false;
}

int Mic::readFrame(int16_t* buffer, int maxSamples) {
    if (!_running || !_rxHandle) return 0;

    size_t bytesRead = 0;
    size_t bytesToRead = maxSamples * sizeof(int16_t);

    esp_err_t err = i2s_channel_read(_rxHandle, buffer, bytesToRead, &bytesRead, pdMS_TO_TICKS(50));
    if (err != ESP_OK) {
        return 0;
    }

    return bytesRead / sizeof(int16_t);
}
