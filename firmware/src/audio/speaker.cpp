#include "speaker.h"

Speaker speaker;

bool Speaker::begin() {
    if (_running) return true;

    // Configure I2S channel (Standard TX on I2S1)
    i2s_chan_config_t chanCfg = {
        .id = I2S_NUM_1,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 4,
        .dma_frame_num = ADPCM_FRAME_SAMPLES,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };

    esp_err_t err = i2s_new_channel(&chanCfg, &_txHandle, nullptr);
    if (err != ESP_OK) {
        log_e("Failed to create I2S TX channel: %d", err);
        return false;
    }

    // Configure standard I2S TX for MAX98357A
    i2s_std_config_t stdCfg = {
        .clk_cfg = {
            .sample_rate_hz = AUDIO_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)PIN_I2S_BCLK,
            .ws = (gpio_num_t)PIN_I2S_LRCLK,
            .dout = (gpio_num_t)PIN_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(_txHandle, &stdCfg);
    if (err != ESP_OK) {
        log_e("Failed to init I2S TX: %d", err);
        i2s_del_channel(_txHandle);
        _txHandle = nullptr;
        return false;
    }

    err = i2s_channel_enable(_txHandle);
    if (err != ESP_OK) {
        log_e("Failed to enable I2S TX: %d", err);
        i2s_del_channel(_txHandle);
        _txHandle = nullptr;
        return false;
    }

    _running = true;
    log_i("I2S speaker started at %d Hz", AUDIO_SAMPLE_RATE);
    return true;
}

void Speaker::end() {
    if (!_running) return;
    if (_txHandle) {
        i2s_channel_disable(_txHandle);
        i2s_del_channel(_txHandle);
        _txHandle = nullptr;
    }
    _running = false;
}

int Speaker::writeFrame(const int16_t* buffer, int numSamples) {
    if (!_running || !_txHandle) return 0;

    // Copy and apply volume
    int16_t scaled[ADPCM_FRAME_SAMPLES];
    int count = min(numSamples, (int)ADPCM_FRAME_SAMPLES);
    memcpy(scaled, buffer, count * sizeof(int16_t));
    applyVolume(scaled, count);

    size_t bytesWritten = 0;
    esp_err_t err = i2s_channel_write(_txHandle, scaled, count * sizeof(int16_t),
                                       &bytesWritten, pdMS_TO_TICKS(50));
    if (err != ESP_OK) {
        return 0;
    }

    return bytesWritten / sizeof(int16_t);
}

void Speaker::setVolume(uint8_t vol) {
    _volume = min(vol, (uint8_t)VOLUME_MAX);
}

void Speaker::applyVolume(int16_t* buffer, int numSamples) {
    if (_volume >= 100) return;  // No scaling needed at max
    if (_volume == 0) {
        memset(buffer, 0, numSamples * sizeof(int16_t));
        return;
    }

    // Simple linear volume: multiply by volume/100
    for (int i = 0; i < numSamples; i++) {
        buffer[i] = (int16_t)((int32_t)buffer[i] * _volume / 100);
    }
}
