// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Persistent configuration model (saved in NVS).
//
// Holds everything the config UI can change. Runtime state (the recenter
// reference, the live gyro bias) is not stored here.

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // --- Output channel assignment (0-based PARA channel; TR1=0 .. TR8=7) ---
    uint8_t pan_ch;
    uint8_t tilt_ch;
    uint8_t pan_enabled;
    uint8_t tilt_enabled;

    // --- Response ---
    float pan_gain;       // microseconds per degree
    float tilt_gain;
    int8_t pan_invert;    // +1 or -1
    int8_t tilt_invert;
    float deadband_deg;   // dead zone around center

    // --- Output range (microseconds) ---
    uint16_t pan_center, pan_min, pan_max;
    uint16_t tilt_center, tilt_min, tilt_max;

    // --- Board orientation ---
    // Canonical axis i (0=x,1=y,2=z) is fed by IMU axis |remap[i]|-1, with the
    // sign of remap[i]. So remap entries are in {+/-1,+/-2,+/-3}. The default
    // temple-mount remap is {+2,+3,+1} (canonical x<-imu y, y<-imu z, z<-imu x).
    int8_t remap[3];

    // --- Tap detection ---
    float tap_intensity;  // accel-magnitude jerk threshold (g/s)

    // --- Identity ---
    char name[20];        // BLE device name and AP SSID base

    // --- Home WiFi (station) for internet / server OTA ---
    char wifi_ssid[33];
    char wifi_pass[65];
} crowmotion_config_t;

// Load config from NVS, or apply defaults if absent/invalid. Call once at boot.
void config_init(void);

// The live, in-RAM config. Edit via this pointer, then call config_save().
crowmotion_config_t *config_get(void);

// Persist the current config to NVS.
esp_err_t config_save(void);

// Fill a config with factory defaults (does not save).
void config_set_defaults(crowmotion_config_t *c);

// Serialize the live config to a malloc'd JSON string (caller frees with free()).
char *config_to_json(void);

// Apply fields present in a JSON string to the live config (does not save).
esp_err_t config_apply_json(const char *json);

#ifdef __cplusplus
}
#endif
