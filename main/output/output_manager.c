// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// Crowmotion - selects Bluetooth or local trainer-jack PPM output.

#include "output_manager.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "channel_state.h"
#include "config.h"
#include "led.h"
#include "para_ble.h"
#include "ppm_output.h"

static const char *TAG = "output";

#define OUTPUT_POLL_MS 20
#define JACK_DEBOUNCE_SAMPLES 5
#define CENTER_BLE_SETTLE_MS 80
#define CENTER_PPM_FRAMES 3

static volatile bool s_paused;
static volatile bool s_ready;
static volatile bool s_jack_inserted;
static volatile crowmotion_active_output_t s_active = CROWMOTION_OUTPUT_OFF;

static bool read_jack(void)
{
    return gpio_get_level(CROWMOTION_JACK_DETECT_GPIO) == 0;
}

static crowmotion_active_output_t requested_output(void)
{
    if (s_paused) return CROWMOTION_OUTPUT_OFF;

    switch ((crowmotion_output_mode_t)config_get()->output_mode) {
    case CROWMOTION_OUTPUT_MODE_BLUETOOTH:
        return CROWMOTION_OUTPUT_BLUETOOTH;
    case CROWMOTION_OUTPUT_MODE_PPM:
        return CROWMOTION_OUTPUT_PPM;
    case CROWMOTION_OUTPUT_MODE_AUTO:
    default:
        return s_jack_inserted ? CROWMOTION_OUTPUT_PPM
                               : CROWMOTION_OUTPUT_BLUETOOTH;
    }
}

static void send_centered_ppm(bool inverted)
{
    uint16_t centered[CROWMOTION_CHANNEL_COUNT];
    for (int i = 0; i < CROWMOTION_CHANNEL_COUNT; i++) {
        centered[i] = CROWMOTION_CHANNEL_CENTER_US;
    }
    for (int i = 0; i < CENTER_PPM_FRAMES; i++) {
        ppm_output_send_frame(centered, inverted);
    }
}

static void switch_output(crowmotion_active_output_t next)
{
    if (next == s_active) return;

    bool inverted = config_get()->ppm_inverted != 0;
    channel_state_set_inhibit(CHANNEL_INHIBIT_OUTPUT_SWITCH, true);

    if (s_active == CROWMOTION_OUTPUT_PPM) {
        send_centered_ppm(inverted);
        ESP_ERROR_CHECK(ppm_output_set_enabled(false, inverted));
    } else if (s_active == CROWMOTION_OUTPUT_BLUETOOTH) {
        vTaskDelay(pdMS_TO_TICKS(CENTER_BLE_SETTLE_MS));
        para_ble_pause();
    }

    if (next == CROWMOTION_OUTPUT_PPM) {
        para_ble_pause();
        ESP_ERROR_CHECK(ppm_output_set_enabled(true, inverted));
        send_centered_ppm(inverted);
        led_set(LED_CONNECTED);
        ESP_LOGI(TAG, "trainer output: PPM jack (%s polarity)",
                 inverted ? "inverted" : "positive");
    } else if (next == CROWMOTION_OUTPUT_BLUETOOTH) {
        para_ble_resume();
        led_set(LED_SEARCHING);
        ESP_LOGI(TAG, "trainer output: Bluetooth PARA");
    } else {
        para_ble_pause();
        led_set(LED_OFF);
        ESP_LOGI(TAG, "trainer output: off");
    }

    s_active = next;
    channel_state_set_inhibit(CHANNEL_INHIBIT_OUTPUT_SWITCH,
                              next == CROWMOTION_OUTPUT_OFF);
}

static void output_task(void *arg)
{
    bool candidate = read_jack();
    int stable_samples = JACK_DEBOUNCE_SAMPLES;
    s_jack_inserted = candidate;
    s_ready = true;

    for (;;) {
        bool raw = read_jack();
        if (raw == candidate) {
            if (stable_samples < JACK_DEBOUNCE_SAMPLES) stable_samples++;
        } else {
            candidate = raw;
            stable_samples = 1;
        }
        if (stable_samples == JACK_DEBOUNCE_SAMPLES &&
            candidate != s_jack_inserted) {
            s_jack_inserted = candidate;
            ESP_LOGI(TAG, "trainer cable %s", candidate ? "inserted" : "removed");
        }

        crowmotion_active_output_t wanted = requested_output();
        if (wanted != s_active) switch_output(wanted);

        if (s_active == CROWMOTION_OUTPUT_PPM) {
            uint16_t channels[CROWMOTION_CHANNEL_COUNT];
            channel_state_copy(channels);
            esp_err_t err = ppm_output_send_frame(
                channels, config_get()->ppm_inverted != 0);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "PPM frame failed: %s", esp_err_to_name(err));
                channel_state_set_inhibit(CHANNEL_INHIBIT_OUTPUT_SWITCH, true);
                ppm_output_set_enabled(false, config_get()->ppm_inverted != 0);
                s_active = CROWMOTION_OUTPUT_OFF;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(OUTPUT_POLL_MS));
        }
    }
}

esp_err_t output_manager_init(void)
{
    gpio_config_t jack = {
        .pin_bit_mask = 1ULL << CROWMOTION_JACK_DETECT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&jack);
    if (err != ESP_OK) return err;

    err = ppm_output_init(CROWMOTION_PPM_GPIO);
    if (err != ESP_OK) return err;

    channel_state_set_inhibit(CHANNEL_INHIBIT_OUTPUT_SWITCH, true);
    if (xTaskCreate(output_task, "output", 3072, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void output_manager_pause(void)
{
    s_paused = true;
    for (int i = 0; i < 30 && s_active != CROWMOTION_OUTPUT_OFF; i++) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void output_manager_resume(void)
{
    s_paused = false;
}

bool output_manager_is_ble_connected(void)
{
    return para_ble_is_connected();
}

bool output_manager_is_ready(void)
{
    return s_ready;
}

bool output_manager_is_jack_inserted(void)
{
    return s_jack_inserted;
}

crowmotion_active_output_t output_manager_active_output(void)
{
    return s_active;
}
