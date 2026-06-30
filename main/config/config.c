// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Persistent configuration (NVS-backed).

#include "config.h"

#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "config";

#define CFG_NVS_NAMESPACE "freelook"
#define CFG_NVS_KEY "cfg"
#define CFG_VERSION 1   // bump when the struct layout changes incompatibly

static freelook_config_t s_cfg;

void config_set_defaults(freelook_config_t *c)
{
    memset(c, 0, sizeof(*c));
    c->pan_ch = 0;          // TR1 (radio mixes this onto an output channel)
    c->tilt_ch = 1;         // TR2
    c->pan_enabled = 1;
    c->tilt_enabled = 1;
    c->pan_gain = 8.0f;     // ~full deflection at +/-64 deg
    c->tilt_gain = 8.0f;
    c->pan_invert = 1;
    c->tilt_invert = 1;
    c->deadband_deg = 0.0f;
    c->pan_center = 1500;
    c->pan_min = 988;
    c->pan_max = 2012;
    c->tilt_center = 1500;
    c->tilt_min = 988;
    c->tilt_max = 2012;
    c->remap[0] = 2;        // temple mount: canonical x <- +imu y
    c->remap[1] = 3;        //               canonical y <- +imu z
    c->remap[2] = 1;        //               canonical z <- +imu x
    c->tap_intensity = 20.0f;
    strncpy(c->name, "FreeLook", sizeof(c->name) - 1);
}

void config_init(void)
{
    config_set_defaults(&s_cfg);

    nvs_handle_t h;
    if (nvs_open(CFG_NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no saved config, using defaults");
        return;
    }

    uint8_t ver = 0;
    nvs_get_u8(h, "ver", &ver);
    freelook_config_t loaded;
    size_t len = sizeof(loaded);
    esp_err_t err = nvs_get_blob(h, CFG_NVS_KEY, &loaded, &len);
    nvs_close(h);

    if (err == ESP_OK && ver == CFG_VERSION && len == sizeof(loaded)) {
        s_cfg = loaded;
        ESP_LOGI(TAG, "loaded saved config (name=%s)", s_cfg.name);
    } else {
        ESP_LOGW(TAG, "saved config missing/incompatible (ver %d), using defaults", ver);
    }
}

freelook_config_t *config_get(void)
{
    return &s_cfg;
}

esp_err_t config_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(CFG_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u8(h, "ver", CFG_VERSION);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, CFG_NVS_KEY, &s_cfg, sizeof(s_cfg));
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "config saved");
    } else {
        ESP_LOGE(TAG, "config save failed: %s", esp_err_to_name(err));
    }
    return err;
}
