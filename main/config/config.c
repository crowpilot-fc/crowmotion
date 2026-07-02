// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Persistent configuration (NVS-backed).

#include "config.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "cJSON.h"

#include "version.h"

static const char *TAG = "config";

#define CFG_NVS_NAMESPACE "crowmotion"
#define CFG_NVS_KEY "cfg"
#define CFG_VERSION 4   // bump when the struct layout changes incompatibly

static crowmotion_config_t s_cfg;

void config_set_defaults(crowmotion_config_t *c)
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
    c->tap_intensity = 25.0f;
    c->bridge_en = 0;
    strncpy(c->name, "CrowMotion", sizeof(c->name) - 1);
    strncpy(c->ap_pass, "crowmotion", sizeof(c->ap_pass) - 1);
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
    crowmotion_config_t loaded;
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

crowmotion_config_t *config_get(void)
{
    return &s_cfg;
}

char *config_to_json(void)
{
    crowmotion_config_t *c = &s_cfg;
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
    cJSON_AddNumberToObject(j, "bridge_en", c->bridge_en);
    cJSON_AddStringToObject(j, "name", c->name);
    cJSON_AddStringToObject(j, "wifi_ssid", c->wifi_ssid);  // passwords (wifi_pass,
                                                            // ap_pass) never exposed
    cJSON_AddStringToObject(j, "version", CROWMOTION_VERSION);
    char *out = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    return out;
}

// Commit a fully validated config in one shot so the 100 Hz fusion task can
// never observe a word-torn field mid-write (it may still see a mix of old
// and new fields across one 10 ms cycle, which is harmless and self-corrects).
static portMUX_TYPE s_cfg_mux = portMUX_INITIALIZER_UNLOCKED;

static void config_commit(const crowmotion_config_t *nc)
{
    taskENTER_CRITICAL(&s_cfg_mux);
    s_cfg = *nc;
    taskEXIT_CRITICAL(&s_cfg_mux);
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static uint16_t clamp_us(double v)
{
    if (v < 988.0) return 988;
    if (v > 2012.0) return 2012;
    return (uint16_t)v;
}

esp_err_t config_apply_json(const char *json)
{
    cJSON *j = cJSON_Parse(json);
    if (!j) {
        return ESP_ERR_INVALID_ARG;
    }

    // Work on a copy; commit atomically at the end.
    crowmotion_config_t nc = s_cfg;

    if (cJSON_GetObjectItem(j, "reset")) {
        config_set_defaults(&nc);
        cJSON_Delete(j);
        config_commit(&nc);
        return ESP_OK;
    }

    crowmotion_config_t *c = &nc;
    cJSON *it;

    // Channel assignment: only 0..7 (TR1..TR8) is meaningful; ignore junk.
    if ((it = cJSON_GetObjectItem(j, "pan_ch")) && cJSON_IsNumber(it) &&
        it->valuedouble >= 0 && it->valuedouble <= 7) {
        c->pan_ch = (uint8_t)it->valuedouble;
    }
    if ((it = cJSON_GetObjectItem(j, "tilt_ch")) && cJSON_IsNumber(it) &&
        it->valuedouble >= 0 && it->valuedouble <= 7) {
        c->tilt_ch = (uint8_t)it->valuedouble;
    }
    if ((it = cJSON_GetObjectItem(j, "pan_en")) && cJSON_IsNumber(it)) {
        c->pan_enabled = it->valuedouble != 0 ? 1 : 0;
    }
    if ((it = cJSON_GetObjectItem(j, "tilt_en")) && cJSON_IsNumber(it)) {
        c->tilt_enabled = it->valuedouble != 0 ? 1 : 0;
    }

    // Response shaping, clamped to sane physical ranges.
    if ((it = cJSON_GetObjectItem(j, "pan_gain")) && cJSON_IsNumber(it)) {
        c->pan_gain = clampf(it->valuedouble, 0.5f, 50.0f);
    }
    if ((it = cJSON_GetObjectItem(j, "tilt_gain")) && cJSON_IsNumber(it)) {
        c->tilt_gain = clampf(it->valuedouble, 0.5f, 50.0f);
    }
    if ((it = cJSON_GetObjectItem(j, "pan_inv")) && cJSON_IsNumber(it)) {
        c->pan_invert = it->valuedouble < 0 ? -1 : 1;
    }
    if ((it = cJSON_GetObjectItem(j, "tilt_inv")) && cJSON_IsNumber(it)) {
        c->tilt_invert = it->valuedouble < 0 ? -1 : 1;
    }
    if ((it = cJSON_GetObjectItem(j, "deadband")) && cJSON_IsNumber(it)) {
        c->deadband_deg = clampf(it->valuedouble, 0.0f, 45.0f);  // never negative
    }

    // Output range: clamp to the PARA channel range, then enforce min <= max
    // (an inverted pair would invert the output clamp in mapping.c).
    if ((it = cJSON_GetObjectItem(j, "pan_center")) && cJSON_IsNumber(it)) {
        c->pan_center = clamp_us(it->valuedouble);
    }
    if ((it = cJSON_GetObjectItem(j, "pan_min")) && cJSON_IsNumber(it)) {
        c->pan_min = clamp_us(it->valuedouble);
    }
    if ((it = cJSON_GetObjectItem(j, "pan_max")) && cJSON_IsNumber(it)) {
        c->pan_max = clamp_us(it->valuedouble);
    }
    if ((it = cJSON_GetObjectItem(j, "tilt_center")) && cJSON_IsNumber(it)) {
        c->tilt_center = clamp_us(it->valuedouble);
    }
    if ((it = cJSON_GetObjectItem(j, "tilt_min")) && cJSON_IsNumber(it)) {
        c->tilt_min = clamp_us(it->valuedouble);
    }
    if ((it = cJSON_GetObjectItem(j, "tilt_max")) && cJSON_IsNumber(it)) {
        c->tilt_max = clamp_us(it->valuedouble);
    }
    if (c->pan_min > c->pan_max) {
        uint16_t t = c->pan_min; c->pan_min = c->pan_max; c->pan_max = t;
    }
    if (c->tilt_min > c->tilt_max) {
        uint16_t t = c->tilt_min; c->tilt_min = c->tilt_max; c->tilt_max = t;
    }

    if ((it = cJSON_GetObjectItem(j, "tap")) && cJSON_IsNumber(it)) {
        c->tap_intensity = clampf(it->valuedouble, 6.0f, 60.0f);
    }
    if ((it = cJSON_GetObjectItem(j, "bridge_en")) && cJSON_IsNumber(it)) {
        c->bridge_en = it->valuedouble != 0 ? 1 : 0;
    }

    // Orientation remap: each entry must be a number in {+/-1,+/-2,+/-3};
    // reject the whole array otherwise (board_remap would index out of
    // bounds on anything else).
    cJSON *rm = cJSON_GetObjectItem(j, "remap");
    if (rm && cJSON_IsArray(rm) && cJSON_GetArraySize(rm) == 3) {
        int8_t nr[3];
        bool ok = true;
        for (int i = 0; i < 3; i++) {
            cJSON *e = cJSON_GetArrayItem(rm, i);
            int v = (e && cJSON_IsNumber(e)) ? e->valueint : 0;
            if (v < -3 || v > 3 || v == 0) {
                ok = false;
                break;
            }
            nr[i] = (int8_t)v;
        }
        if (ok) {
            c->remap[0] = nr[0];
            c->remap[1] = nr[1];
            c->remap[2] = nr[2];
        } else {
            ESP_LOGW(TAG, "rejected invalid remap array");
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
    // Hotspot passphrase: WPA2 needs 8..64 chars; ignore anything shorter
    // (an empty string would otherwise silently open the hotspot).
    if ((it = cJSON_GetObjectItem(j, "ap_pass")) && cJSON_IsString(it) &&
        strlen(it->valuestring) >= 8 && strlen(it->valuestring) < sizeof(c->ap_pass)) {
        strncpy(c->ap_pass, it->valuestring, sizeof(c->ap_pass) - 1);
        c->ap_pass[sizeof(c->ap_pass) - 1] = '\0';
    }

    cJSON_Delete(j);
    config_commit(&nc);
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
