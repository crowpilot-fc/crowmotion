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
#include "config.h"
#include "tracker.h"
#include "webconfig.h"
#include "para_ble.h"
#include "mpu6500.h"
#include "madgwick.h"
#include "mapping.h"

static const char *TAG = "freelook";

// --- Shared tracker state (read by the config web server) ---
static tracker_snapshot_t s_snap;
static volatile bool s_recenter_req = false;
static volatile bool s_autodetect_req = false;

void tracker_get(tracker_snapshot_t *out) { *out = s_snap; }
void tracker_request_recenter(void) { s_recenter_req = true; }
void tracker_request_autodetect(void) { s_autodetect_req = true; }

// Set the orientation remap so the IMU axis most aligned with gravity becomes
// canonical Z (up); the other two axes fill X/Y. Uses the raw (pre-remap) accel.
static void autodetect_orientation(float ax, float ay, float az)
{
    float a[3] = {ax, ay, az};
    int k = 0;
    for (int i = 1; i < 3; i++) {
        if (fabsf(a[i]) > fabsf(a[k])) k = i;
    }
    int sgn = (a[k] >= 0.0f) ? 1 : -1;
    int8_t *r = config_get()->remap;
    int oi = 0;
    for (int i = 0; i < 3; i++) {
        if (i != k) r[oi++] = (int8_t)(i + 1);   // canonical X, Y <- other axes
    }
    r[2] = (int8_t)(sgn * (k + 1));               // canonical Z <- gravity axis
    config_save();
    ESP_LOGI("orient", "auto-detected remap [%d %d %d]", r[0], r[1], r[2]);
}

#define FUSION_PERIOD_MS 10           // 100 Hz fusion loop
#define FUSION_BETA 0.1f
#define BIAS_SAMPLES 100              // ~1 s startup gyro-bias estimate
#define SETTLE_ITERS 50               // ~0.5 s for fusion to converge before centering
#define DEG_TO_RAD 0.01745329252f
#define STILL_DPS 2.0f                // gyro below this (bias-corrected) = "still"
#define BIAS_ALPHA 0.002f             // gyro-bias tracking rate while still (M4)
#define TAP_GYRO_MAX_DPS 40.0f        // ignore "taps" while rotating faster than this
#define TAP_GAP_MIN_MS 80             // double-tap: 2nd tap this long after the 1st
#define TAP_GAP_MAX_MS 600

// Board-orientation remap: rotate the IMU axes into the canonical frame (Z up)
// so the fusion's yaw = pan and pitch = tilt, using the configurable remap.
// remap[i] = signed (1-based) IMU axis feeding canonical axis i. The default
// (temple mount) is {+2,+3,+1}: canonical x<-imu y, y<-imu z, z<-imu x.
static void board_remap(imu_sample_t *s)
{
    const int8_t *r = config_get()->remap;
    float a[3] = {s->ax, s->ay, s->az};
    float g[3] = {s->gx, s->gy, s->gz};
    float ao[3], go[3];
    for (int i = 0; i < 3; i++) {
        int idx = (r[i] < 0 ? -r[i] : r[i]) - 1;  // 0..2
        float sign = (r[i] < 0) ? -1.0f : 1.0f;
        ao[i] = sign * a[idx];
        go[i] = sign * g[idx];
    }
    s->ax = ao[0];  s->ay = ao[1];  s->az = ao[2];
    s->gx = go[0];  s->gy = go[1];  s->gz = go[2];
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
    int tap_count = 0;
    for (;;) {
        imu_sample_t s;
        if (mpu6500_read(&s) == ESP_OK) {
            if (s_autodetect_req) {
                s_autodetect_req = false;
                autodetect_orientation(s.ax, s.ay, s.az);  // uses raw (pre-remap)
            }
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

            if (s_recenter_req) {
                s_recenter_req = false;
                mapping_recenter(yaw, pitch, roll);
            }

            // Tap gestures: count taps in a sequence and act when it ends.
            // 2 = recenter, 4 = toggle the config hotspot (3 reserved).
            float amag = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
            float jerk = (amag - last_amag) / dt;
            last_amag = amag;
            int64_t now_ms = now / 1000;

            if (settle < SETTLE_ITERS) {
                if (++settle == SETTLE_ITERS) {
                    mapping_recenter(yaw, pitch, roll);
                    ESP_LOGI("fusion", "centered; head motion now drives channels");
                }
            } else {
                float gmag = sqrtf(dgx * dgx + dgy * dgy + dgz * dgz);
                if (jerk > config_get()->tap_intensity && gmag < TAP_GYRO_MAX_DPS) {
                    int64_t gap = (last_tap_ms == 0) ? 1000000 : now_ms - last_tap_ms;
                    if (gap < TAP_GAP_MIN_MS) {
                        // debounce: same tap, ignore
                    } else if (gap <= TAP_GAP_MAX_MS) {
                        tap_count++;
                        last_tap_ms = now_ms;
                    } else {
                        tap_count = 1;
                        last_tap_ms = now_ms;
                    }
                }
                if (tap_count > 0 && last_tap_ms != 0 &&
                    (now_ms - last_tap_ms) > TAP_GAP_MAX_MS) {
                    if (tap_count == 2) {
                        mapping_recenter(yaw, pitch, roll);
                        ESP_LOGI("fusion", "double-tap recenter");
                    } else if (tap_count == 4) {
                        ESP_LOGI("fusion", "quad-tap: toggle config mode");
                        webconfig_request_toggle();
                    }
                    tap_count = 0;
                    last_tap_ms = 0;
                }
                mapping_update(yaw, pitch, roll);
            }

            // Publish a snapshot for the config UI.
            const freelook_config_t *cc = config_get();
            s_snap.yaw = yaw;
            s_snap.pitch = pitch;
            s_snap.roll = roll;
            s_snap.pan_us = para_ble_get_channel(cc->pan_ch);
            s_snap.tilt_us = para_ble_get_channel(cc->tilt_ch);

            if (++log_div >= 50) {  // ~2 Hz
                log_div = 0;
                ESP_LOGI("fusion", "ch pan %u  tilt %u", s_snap.pan_us, s_snap.tilt_us);
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

    // Load persisted settings (orientation, gains, limits, taps, channels).
    config_init();

    // Config web UI controller (quad-tap brings up the WiFi hotspot).
    webconfig_init();

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
