// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Application entry point.
//
// Original implementation. Inspired by RC HeadTracker (dlktdr) and
// HeadTracker (ysoldak); no code from those projects is reused.
//
// Build target: Seeed Studio XIAO ESP32-C3, ESP-IDF + NimBLE.

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "para_ble.h"
#include "mpu6500.h"

static const char *TAG = "freelook";

// M2 bring-up: read the IMU and print accel (g), gyro (dps), and temperature.
// This is a verification path; M3 replaces it with the fusion pipeline.
static void imu_log_task(void *arg)
{
    imu_sample_t s;
    for (;;) {
        if (mpu6500_read(&s) == ESP_OK) {
            ESP_LOGI("imu",
                     "acc[g] % .3f % .3f % .3f  gyr[dps] % .2f % .2f % .2f  %.1fC",
                     s.ax, s.ay, s.az, s.gx, s.gy, s.gz, s.temp_c);
        } else {
            ESP_LOGW("imu", "read failed");
        }
        vTaskDelay(pdMS_TO_TICKS(200));  // 5 Hz to keep the log readable
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "FreeLook starting (XIAO ESP32-C3)");

    // NVS is needed by the BLE stack and, later (M7), by settings persistence.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Milestone 1: stand up the PARA wireless trainer link and stream
    // centered channels. The IMU pipeline (M2+) is layered on later.
    if (para_ble_start() != 0) {
        ESP_LOGE(TAG, "PARA link failed to start");
        return;
    }

    // Bring up the IMU. If it is not wired yet, keep running so the PARA link
    // still advertises and streams centered channels.
    if (mpu6500_init() == ESP_OK) {
        xTaskCreate(imu_log_task, "imu_log", 3072, NULL, 4, NULL);
    } else {
        ESP_LOGW(TAG, "Continuing without IMU. Wire SDA=D10/GPIO10, "
                      "SCL=D7/GPIO20, AD0=GND, VCC=3V3, then reset.");
    }

    ESP_LOGI(TAG, "FreeLook up. Waiting for radio (X20S) to connect.");
}
