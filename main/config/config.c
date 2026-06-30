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
#include "cJSON.h"

#include "version.h"

static const char *TAG = "config";

#define CFG_NVS_NAMESPACE "freelook"
#define CFG_NVS_KEY "cfg"
#define CFG_VERSION 2   // bump when the struct layout changes incompatibly

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

char *config_to_json(void)
{
    freelook_config_t *c = &s_cfg;
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "pan_ch", c->pan_ch);
    cJSON_AddNumberToObject(j, "tilt_ch", c->tilt_ch);
    cJSON_AddNumberToObject(j, "pan_en", c->pan_enabled);
    cJSON_AddNumberToObject(j, "tilt_en", c->tilt_enabled);
    cJSON_AddNumberToObject(j, "pan_gain", c->pan_gain);
    cJSON_AddNumberToObject(j, "tilt_gain", c->tilt_gain);
    cJSON_AddNumberToObject(j, "pan_inv", c->pan_invert);
    cJSON_AddNumberToObject(j, "tilt_inv", c->tilt_invert);
    cJSON_AddNumberToObject(j, "deadband", c->deadband_deg);
    cJSON_AddNumberToObject(j, "pan_center", c->pan_center);
    cJSON_AddNumberToObject(j, "pan_min", c->pan_min);
    cJSON_AddNumberToObject(j, "pan_max", c->pan_max);
    cJSON_AddNumberToObject(j, "tilt_center", c->tilt_center);
    cJSON_AddNumberToObject(j, "tilt_min", c->tilt_min);
    cJSON_AddNumberToObject(j, "tilt_max", c->tilt_max);
    int rm[3] = {c->remap[0], c->remap[1], c->remap[2]};
    cJSON_AddItemToObject(j, "remap", cJSON_CreateIntArray(rm, 3));
    cJSON_AddNumberToObject(j, "tap", c->tap_intensity);
    cJSON_AddStringToObject(j, "name", c->name);
    cJSON_AddStringToObject(j, "wifi_ssid", c->wifi_ssid);  // password not exposed
    cJSON_AddStringToObject(j, "version", FREELOOK_VERSION);
    char *out = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    return out;
}

esp_err_t config_apply_json(const char *json)
{
    cJSON *j = cJSON_Parse(json);
    if (!j) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cJSON_GetObjectItem(j, "reset")) {
        config_set_defaults(&s_cfg);
        cJSON_Delete(j);
        return ESP_OK;
    }
    freelook_config_t *c = &s_cfg;
    cJSON *it;
#define GET_NUM(key, field)                                              \
    if ((it = cJSON_GetObjectItem(j, key)) && cJSON_IsNumber(it)) {       \
        c->field = it->valuedouble;                                      \
    }
    GET_NUM("pan_ch", pan_ch);
    GET_NUM("tilt_ch", tilt_ch);
    GET_NUM("pan_en", pan_enabled);
    GET_NUM("tilt_en", tilt_enabled);
    GET_NUM("pan_gain", pan_gain);
    GET_NUM("tilt_gain", tilt_gain);
    GET_NUM("pan_inv", pan_invert);
    GET_NUM("tilt_inv", tilt_invert);
    GET_NUM("deadband", deadband_deg);
    GET_NUM("pan_center", pan_center);
    GET_NUM("pan_min", pan_min);
    GET_NUM("pan_max", pan_max);
    GET_NUM("tilt_center", tilt_center);
    GET_NUM("tilt_min", tilt_min);
    GET_NUM("tilt_max", tilt_max);
    GET_NUM("tap", tap_intensity);
#undef GET_NUM
    cJSON *rm = cJSON_GetObjectItem(j, "remap");
    if (rm && cJSON_IsArray(rm) && cJSON_GetArraySize(rm) == 3) {
        for (int i = 0; i < 3; i++) {
            c->remap[i] = cJSON_GetArrayItem(rm, i)->valueint;
        }
    }
    if ((it = cJSON_GetObjectItem(j, "name")) && cJSON_IsString(it)) {
        strncpy(c->name, it->valuestring, sizeof(c->name) - 1);
        c->name[sizeof(c->name) - 1] = '\0';
    }
    if ((it = cJSON_GetObjectItem(j, "wifi_ssid")) && cJSON_IsString(it)) {
        strncpy(c->wifi_ssid, it->valuestring, sizeof(c->wifi_ssid) - 1);
        c->wifi_ssid[sizeof(c->wifi_ssid) - 1] = '\0';
    }
    if ((it = cJSON_GetObjectItem(j, "wifi_pass")) && cJSON_IsString(it)) {
        strncpy(c->wifi_pass, it->valuestring, sizeof(c->wifi_pass) - 1);
        c->wifi_pass[sizeof(c->wifi_pass) - 1] = '\0';
    }
    cJSON_Delete(j);
    return ESP_OK;
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
