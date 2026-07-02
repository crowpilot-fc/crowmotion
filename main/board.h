// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Per-board pin and feature profile.
//
// v1 targets exactly one board: the ESP32-C3 Super Mini. I2C pins are picked
// so all IMU wiring stays on one side of the board, alongside its 3V3 and
// GND pads.

#pragma once

#include "sdkconfig.h"

#if defined(CONFIG_CROWMOTION_BOARD_C3_SUPERMINI)
#define CROWMOTION_BOARD_NAME "ESP32-C3 Super Mini"
#define CROWMOTION_I2C_SDA_GPIO 4   // D2, right side (next to 3V3/GND)
#define CROWMOTION_I2C_SCL_GPIO 3   // D1, right side

#else
#error "Select a CrowMotion board: idf.py menuconfig -> CrowMotion Configuration"
#endif
