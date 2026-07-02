// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// CrowLink transmitter (see espnow_tx.h).
//
// WiFi bring-up/teardown is serialized behind one mutex and driven from the
// transmit task, so enabling or disabling the bridge in the config UI takes
// effect live, and webconfig can borrow the radio for the hotspot without
// racing us.

#include "espnow_tx.h"

#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "config.h"
#include "para_ble.h"
#include "crowlink_proto.h"

static const char *TAG = "crowlink";

static SemaphoreHandle_t s_lock;
static bool s_up = false;      // WiFi + ESP-NOW currently running
static bool s_paused = false;  // webconfig owns the radio

static const uint8_t BCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Bring WiFi (STA, no connection) + ESP-NOW up. Caller holds s_lock.
static void link_up(void)
{
    if (s_up) {
        return;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        return;
    }
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_STA);
    if (esp_wifi_start() != ESP_OK) {
        esp_wifi_deinit();
        return;
    }
    esp_wifi_set_channel(CROWLINK_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) {
        esp_wifi_stop();
        esp_wifi_deinit();
        return;
    }
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = 0;  // current channel
    peer.ifidx = WIFI_IF_STA;
    esp_now_add_peer(&peer);
    s_up = true;
    ESP_LOGI(TAG, "bridge broadcast up (channel %d, %d Hz)",
             CROWLINK_WIFI_CHANNEL, CROWLINK_RATE_HZ);
}

// Tear everything down. Caller holds s_lock.
static void link_down(void)
{
    if (!s_up) {
        return;
    }
    esp_now_deinit();
    esp_wifi_stop();
    esp_wifi_deinit();
    s_up = false;
    ESP_LOGI(TAG, "bridge broadcast down");
}

static void tx_task(void *arg)
{
    crowlink_frame_t f = {.magic = CROWLINK_MAGIC, .ver = CROWLINK_PROTO_VER};
    uint8_t seq = 0;

    for (;;) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        bool want = config_get()->bridge_en && !s_paused;
        if (want && !s_up) {
            link_up();
        } else if (!want && s_up) {
            link_down();
        }
        if (s_up) {
            for (int i = 0; i < CROWLINK_NUM_CH; i++) {
                f.ch[i] = para_ble_get_channel(i);
            }
            f.seq = seq++;
            f.xsum = crowlink_xsum(&f);
            esp_now_send(BCAST, (const uint8_t *)&f, sizeof(f));
        }
        xSemaphoreGive(s_lock);
        vTaskDelay(pdMS_TO_TICKS(1000 / CROWLINK_RATE_HZ));
    }
}

void crowlink_tx_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    xTaskCreate(tx_task, "crowlink", 3072, NULL, 4, NULL);
}

void crowlink_tx_pause(void)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    link_down();
    s_paused = true;
    xSemaphoreGive(s_lock);
}

void crowlink_tx_resume(void)
{
    if (!s_lock) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_paused = false;  // the task brings the link back up if enabled
    xSemaphoreGive(s_lock);
}
