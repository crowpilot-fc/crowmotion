// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Application entry point.
//
// Original implementation. Inspired by RC HeadTracker (dlktdr) and
// HeadTracker (ysoldak); no code from those projects is reused.
//
// Build target: ESP32-C3 (Super Mini or Seeed XIAO), ESP-IDF + NimBLE.

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"
#include "para_ble.h"
#include "mpu6500.h"
#include "madgwick.h"
#include "mapping.h"

static const char *TAG = "freelook";

#define FUSION_PERIOD_MS 10           // 100 Hz fusion loop
#define FUSION_BETA 0.1f
#define BIAS_SAMPLES 100              // ~1 s startup gyro-bias estimate
#define SETTLE_ITERS 50               // ~0.5 s for fusion to converge before centering
#define DEG_TO_RAD 0.01745329252f
#define STILL_DPS 2.0f                // gyro below this (bias-corrected) = "still"
#define BIAS_ALPHA 0.002f             // gyro-bias tracking rate while still (M4)
#define TAP_JERK_G_PER_S 20.0f        // accel-magnitude jerk above this = a tap (M6)
#define TAP_GYRO_MAX_DPS 40.0f        // ignore "taps" while rotating faster than this
#define TAP_GAP_MIN_MS 80             // double-tap: 2nd tap this long after the 1st
#define TAP_GAP_MAX_MS 600

// Board-orientation remap for the temple mount: the board sits vertical with
// its +X axis up and +Z pointing out the side of the head (confirmed from the
// gravity reading, ~+1g on X when worn). Rotate the IMU axes into the canonical
// frame (Z up) so the fusion's yaw = pan and pitch = tilt. Signs are corrected
// per-axis in the mapping layer.
static void board_remap(imu_sample_t *s)
{
    float ax = s->ax, ay = s->ay, az = s->az;
    float gx = s->gx, gy = s->gy, gz = s->gz;
    s->ax = ay;  s->ay = az;  s->az = ax;
    s->gx = gy;  s->gy = gz;  s->gz = gx;
}

// Read the IMU, run Madgwick 6DOF fusion with continuous gyro auto-calibration
// (M4) and the temple-mount remap, then drive the pan/tilt channels (M5).
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
            board_remap(&s);
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

    // Seed orientation from gravity so fusion starts level-correct (no ramp).
    imu_sample_t s0;
    if (mpu6500_read(&s0) == ESP_OK) {
        board_remap(&s0);
        madgwick_set_from_accel(&filt, s0.ax, s0.ay, s0.az);
    }

    int64_t t_prev = esp_timer_get_time();
    int log_div = 0;
    int settle = 0;
    float last_amag = 1.0f;
    int64_t last_tap_ms = 0;
    for (;;) {
        imu_sample_t s;
        if (mpu6500_read(&s) == ESP_OK) {
            board_remap(&s);
            int64_t now = esp_timer_get_time();
            float dt = (now - t_prev) / 1e6f;
            t_prev = now;
            if (dt <= 0.0f || dt > 0.1f) {
                dt = FUSION_PERIOD_MS / 1000.0f;  // guard against jitter
            }

            // Continuous gyro auto-calibration (M4): while the board is still,
            // slowly track the gyro bias so yaw does not drift.
            float dgx = s.gx - bx, dgy = s.gy - by, dgz = s.gz - bz;
            if (fabsf(dgx) < STILL_DPS && fabsf(dgy) < STILL_DPS &&
                fabsf(dgz) < STILL_DPS) {
                bx += BIAS_ALPHA * dgx;
                by += BIAS_ALPHA * dgy;
                bz += BIAS_ALPHA * dgz;
                dgx = s.gx - bx;
                dgy = s.gy - by;
                dgz = s.gz - bz;
            }
            madgwick_update_imu(&filt, dgx * DEG_TO_RAD, dgy * DEG_TO_RAD,
                                dgz * DEG_TO_RAD, s.ax, s.ay, s.az, dt);

            float yaw, pitch, roll;
            madgwick_get_euler_deg(&filt, &yaw, &pitch, &roll);

            // M6: a tap is a sharp spike in accel-magnitude jerk. Two taps within
            // a short window recenter to wherever the head is currently pointing.
            float amag = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
            float jerk = (amag - last_amag) / dt;
            last_amag = amag;

            // Once fusion has settled, set "look forward" as center, then map
            // head motion onto the PARA channels every loop.
            if (settle < SETTLE_ITERS) {
                if (++settle == SETTLE_ITERS) {
                    mapping_recenter(yaw, pitch, roll);
                    ESP_LOGI("fusion", "centered; head motion now drives channels");
                }
            } else {
                // A tap is a jerk spike while the board is not rotating fast.
                float gmag = sqrtf(dgx * dgx + dgy * dgy + dgz * dgz);
                if (jerk > TAP_JERK_G_PER_S && gmag < TAP_GYRO_MAX_DPS) {
                    int64_t now_ms = now / 1000;
                    int64_t gap = now_ms - last_tap_ms;
                    if (gap > TAP_GAP_MIN_MS && gap < TAP_GAP_MAX_MS) {
                        mapping_recenter(yaw, pitch, roll);
                        ESP_LOGI("fusion", "double-tap recenter");
                        last_tap_ms = 0;  // consumed; need two fresh taps again
                    } else {
                        last_tap_ms = now_ms;  // first tap
                    }
                }
                mapping_update(yaw, pitch, roll);
            }

            if (++log_div >= 10) {  // ~10 Hz
                log_div = 0;
                ESP_LOGI("fusion", "ch pan %u  tilt %u",
                         para_ble_get_channel(0), para_ble_get_channel(1));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(FUSION_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "FreeLook starting (%s)", FREELOOK_BOARD_NAME);

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
        ESP_LOGW(TAG, "Continuing without IMU. Check the wiring (pins logged "
                      "above), AD0=GND, VCC=3V3, then reset.");
    }

    ESP_LOGI(TAG, "FreeLook up. Waiting for radio (X20S) to connect.");
}
