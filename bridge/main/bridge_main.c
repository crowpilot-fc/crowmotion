// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowLink - wireless trainer bridge for CrowMotion
// Application entry point.
//
// Receives trainer channels broadcast by a CrowMotion head tracker over
// ESP-NOW and outputs them as wired PPM into any radio's trainer jack.
// Failsafe: channels snap to center if no valid frame arrives within
// CROWLINK_FAILSAFE_MS; the radio's trainer switch remains the ultimate
// override.
//
// Target board: ESP32-C6 Super Mini (battery pads + onboard TP4054).
// Onboard LED: heartbeat = waiting for the tracker, solid = receiving.

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "crowlink_proto.h"
#include "ppm.h"

#define CROWLINK_VERSION "0.1.0"

#define LED_GPIO 8          // C6 Super Mini onboard LED, active low
#define CH_CENTER 1500

static const char *TAG = "crowlink";

// Latest channels from the tracker, plus when they arrived.
static volatile int64_t s_last_rx_us = 0;
static uint16_t s_ch[CROWLINK_NUM_CH];
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data,
                    int len)
{
    if (len != sizeof(crowlink_frame_t)) {
        return;
    }
    const crowlink_frame_t *f = (const crowlink_frame_t *)data;
    if (f->magic != CROWLINK_MAGIC || f->ver != CROWLINK_PROTO_VER ||
        f->xsum != crowlink_xsum(f)) {
        return;
    }
    taskENTER_CRITICAL(&s_mux);
    memcpy(s_ch, f->ch, sizeof(s_ch));
    taskEXIT_CRITICAL(&s_mux);
    s_last_rx_us = esp_timer_get_time();
}

static void wifi_espnow_init(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(
        esp_wifi_set_channel(CROWLINK_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_recv));
}

// PPM output loop; also drives the LED from the link state. Frames are
// emitted back-to-back, so this task self-paces at ~44 Hz.
static void ppm_task(void *arg)
{
    uint16_t out[CROWLINK_NUM_CH];
    int64_t led_toggle_us = 0;
    bool led_on = false;
    bool linked_prev = false;

    for (;;) {
        int64_t now = esp_timer_get_time();
        bool linked =
            (now - s_last_rx_us) < (int64_t)CROWLINK_FAILSAFE_MS * 1000 &&
            s_last_rx_us != 0;

        if (linked) {
            taskENTER_CRITICAL(&s_mux);
            memcpy(out, s_ch, sizeof(out));
            taskEXIT_CRITICAL(&s_mux);
        } else {
            for (int i = 0; i < CROWLINK_NUM_CH; i++) {
                out[i] = CH_CENTER;  // failsafe
            }
        }

        if (linked != linked_prev) {
            linked_prev = linked;
            if (linked) {
                ESP_LOGI(TAG, "tracker link up");
            } else {
                ESP_LOGI(TAG, "tracker link lost, channels centered");
            }
        }

        // LED: solid while linked, short blink every second while waiting.
        if (linked) {
            gpio_set_level(LED_GPIO, 0);  // active low: on
        } else if (now - led_toggle_us >
                   (led_on ? 100000 : 900000)) {
            led_toggle_us = now;
            led_on = !led_on;
            gpio_set_level(LED_GPIO, led_on ? 0 : 1);
        }

        ppm_send_frame(out);  // blocks one 22.5 ms frame
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "CrowLink %s starting (bridge for CrowMotion)",
             CROWLINK_VERSION);

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(LED_GPIO, 1);  // off

    esp_err_t err = nvs_flash_init();  // required by the WiFi stack
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_espnow_init();
    ppm_init();
    xTaskCreate(ppm_task, "ppm", 3072, NULL, 5, NULL);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG,
             "listening on channel %d (mac %02x:%02x:%02x:%02x:%02x:%02x), "
             "PPM on GPIO%d",
             CROWLINK_WIFI_CHANNEL, mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5], PPM_GPIO);
}
