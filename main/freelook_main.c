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

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "para_ble.h"
#include "mpu6500.h"
#include "madgwick.h"

static const char *TAG = "freelook";

#define FUSION_PERIOD_MS 10           // 100 Hz fusion loop
#define FUSION_BETA 0.1f
#define BIAS_SAMPLES 100              // ~1 s startup gyro-bias estimate
#define DEG_TO_RAD 0.01745329252f

// M3: read the IMU, run Madgwick 6DOF fusion, and log yaw/pitch/roll.
// A short startup average removes the bulk of the gyro bias so the angles hold
// still at rest; proper continuous auto-calibration is M4.
static void fusion_task(void *arg)
{
    madgwick_t filt;
    madgwick_init(&filt, FUSION_BETA);

    // Estimate gyro bias assuming the board is roughly still at startup.
    float bx = 0, by = 0, bz = 0;
    int got = 0;
    for (int i = 0; i < BIAS_SAMPLES; i++) {
        imu_sample_t s;
        if (mpu6500_read(&s) == ESP_OK) {
            bx += s.gx;
            by += s.gy;
            bz += s.gz;
            got++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (got > 0) {
        bx /= got;
        by /= got;
        bz /= got;
    }
    ESP_LOGI("fusion", "startup gyro bias [dps] % .2f % .2f % .2f", bx, by, bz);

    int64_t t_prev = esp_timer_get_time();
    int log_div = 0;
    for (;;) {
        imu_sample_t s;
        if (mpu6500_read(&s) == ESP_OK) {
            int64_t now = esp_timer_get_time();
            float dt = (now - t_prev) / 1e6f;
            t_prev = now;
            if (dt <= 0.0f || dt > 0.1f) {
                dt = FUSION_PERIOD_MS / 1000.0f;  // guard against jitter
            }

            float gx = (s.gx - bx) * DEG_TO_RAD;
            float gy = (s.gy - by) * DEG_TO_RAD;
            float gz = (s.gz - bz) * DEG_TO_RAD;
            madgwick_update_imu(&filt, gx, gy, gz, s.ax, s.ay, s.az, dt);

            if (++log_div >= 10) {  // ~10 Hz
                log_div = 0;
                float yaw, pitch, roll;
                madgwick_get_euler_deg(&filt, &yaw, &pitch, &roll);
                ESP_LOGI("fusion", "yaw % 7.1f  pitch % 7.1f  roll % 7.1f",
                         yaw, pitch, roll);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(FUSION_PERIOD_MS));
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
        xTaskCreate(fusion_task, "fusion", 4096, NULL, 4, NULL);
    } else {
        ESP_LOGW(TAG, "Continuing without IMU. Wire SDA=D10/GPIO10, "
                      "SCL=D7/GPIO20, AD0=GND, VCC=3V3, then reset.");
    }

    ESP_LOGI(TAG, "FreeLook up. Waiting for radio (X20S) to connect.");
}
