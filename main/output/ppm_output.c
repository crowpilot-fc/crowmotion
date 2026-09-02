// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// Crowmotion - hardware-timed CPPM/PPM trainer output.

#include "ppm_output.h"

#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"

static const char *TAG = "ppm";

#define PPM_RESOLUTION_HZ 1000000

static rmt_channel_handle_t s_channel;
static rmt_encoder_handle_t s_encoder;
static int s_gpio = -1;
static bool s_enabled;

static void drive_idle(bool inverted)
{
    gpio_reset_pin(s_gpio);
    gpio_set_direction(s_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_gpio, inverted ? 1 : 0);
}

esp_err_t ppm_output_init(int gpio_num)
{
    s_gpio = gpio_num;
    drive_idle(false);

    rmt_tx_channel_config_t channel_config = {
        .gpio_num = gpio_num,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = PPM_RESOLUTION_HZ,
        .mem_block_symbols = 48,
        .trans_queue_depth = 2,
    };
    esp_err_t err = rmt_new_tx_channel(&channel_config, &s_channel);
    if (err != ESP_OK) return err;

    rmt_copy_encoder_config_t encoder_config = {};
    err = rmt_new_copy_encoder(&encoder_config, &s_encoder);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "PPM prepared on GPIO%d, %d us frame", gpio_num,
             CROWMOTION_PPM_FRAME_US);
    return ESP_OK;
}

esp_err_t ppm_output_set_enabled(bool enabled, bool inverted)
{
    if (s_channel == NULL) return ESP_ERR_INVALID_STATE;
    if (enabled == s_enabled) return ESP_OK;

    esp_err_t err = ESP_OK;
    if (enabled) {
        err = rmt_enable(s_channel);
    } else {
        err = rmt_tx_wait_all_done(s_channel, 100);
        if (err == ESP_OK) err = rmt_disable(s_channel);
        drive_idle(inverted);
    }
    if (err == ESP_OK) s_enabled = enabled;
    return err;
}

esp_err_t ppm_output_send_frame(
    const uint16_t channels[CROWMOTION_CHANNEL_COUNT], bool inverted)
{
    if (!s_enabled) return ESP_ERR_INVALID_STATE;

    rmt_symbol_word_t symbols[CROWMOTION_CHANNEL_COUNT + 1];
    uint32_t used_us = 0;
    uint8_t active = inverted ? 0 : 1;
    uint8_t idle = inverted ? 1 : 0;

    for (int i = 0; i < CROWMOTION_CHANNEL_COUNT; i++) {
        uint16_t value = channels[i];
        if (value < CROWMOTION_CHANNEL_MIN_US) value = CROWMOTION_CHANNEL_MIN_US;
        if (value > CROWMOTION_CHANNEL_MAX_US) value = CROWMOTION_CHANNEL_MAX_US;
        symbols[i].level0 = active;
        symbols[i].duration0 = CROWMOTION_PPM_PULSE_US;
        symbols[i].level1 = idle;
        symbols[i].duration1 = value - CROWMOTION_PPM_PULSE_US;
        used_us += value;
    }

    uint32_t sync_us = CROWMOTION_PPM_FRAME_US - used_us -
                       CROWMOTION_PPM_PULSE_US;
    symbols[CROWMOTION_CHANNEL_COUNT].level0 = active;
    symbols[CROWMOTION_CHANNEL_COUNT].duration0 = CROWMOTION_PPM_PULSE_US;
    symbols[CROWMOTION_CHANNEL_COUNT].level1 = idle;
    symbols[CROWMOTION_CHANNEL_COUNT].duration1 = sync_us;

    rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
        .flags.eot_level = idle,
    };
    esp_err_t err = rmt_transmit(s_channel, s_encoder, symbols,
                                 sizeof(symbols), &transmit_config);
    if (err != ESP_OK) return err;
    return rmt_tx_wait_all_done(s_channel, 100);
}
