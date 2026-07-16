// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Per-board pin and feature profile.
//
// Each profile picks the IMU I2C pins so all wiring stays on one side of the
// board, alongside its 3V3 and GND pads, and describes the onboard status
// LED (plain GPIO or an addressable WS2812).
//
// Hardware status: the C3 Super Mini is bench-verified. The C6 and S3
// Super Mini pin maps are from board pinouts and must be re-checked on real
// hardware before those targets are advertised as supported.

#pragma once

#include "sdkconfig.h"

#if defined(CONFIG_CROWMOTION_BOARD_C3_SUPERMINI)
#define CROWMOTION_BOARD_NAME "ESP32-C3 Super Mini"
#define CROWMOTION_I2C_SDA_GPIO 4   // D2, right side (next to 3V3/GND)
#define CROWMOTION_I2C_SCL_GPIO 3   // D1, right side
#define CROWMOTION_LED_GPIO 8       // single blue LED, active low, strap pin
#define CROWMOTION_LED_ACTIVE_LOW 1

#elif defined(CONFIG_CROWMOTION_BOARD_C6_SUPERMINI)
#define CROWMOTION_BOARD_NAME "ESP32-C6 Super Mini"
#define CROWMOTION_I2C_SDA_GPIO 4   // mirrors the C3 Super Mini layout (verify on hardware)
#define CROWMOTION_I2C_SCL_GPIO 3
#define CROWMOTION_LED_GPIO 8       // onboard LED, active low (same board CrowLink uses)
#define CROWMOTION_LED_ACTIVE_LOW 1

#elif defined(CONFIG_CROWMOTION_BOARD_S3_SUPERMINI)
#define CROWMOTION_BOARD_NAME "ESP32-S3 Super Mini"
#define CROWMOTION_I2C_SDA_GPIO 8   // non-strapping pair, common I2C pick (verify on hardware)
#define CROWMOTION_I2C_SCL_GPIO 9
#define CROWMOTION_LED_WS2812_GPIO 48  // onboard addressable RGB LED (some revisions use 47)

#else
#error "Select a CrowMotion board: idf.py menuconfig -> CrowMotion Configuration"
#endif
