// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Locked ESP32-C3 Super Mini pin assignment for the integrated build.

#pragma once

#define CROWMOTION_BOARD_NAME "ESP32-C3 Super Mini"
#define CROWMOTION_I2C_SDA_GPIO 4   // D2, right side (next to 3V3/GND)
#define CROWMOTION_I2C_SCL_GPIO 3   // D1, right side
#define CROWMOTION_LED_GPIO 8       // single blue LED, active low, strap pin
#define CROWMOTION_LED_ACTIVE_LOW 1
#define CROWMOTION_PPM_GPIO 6       // 2.2k series resistor to jack tip
#define CROWMOTION_JACK_DETECT_GPIO 7 // active low, switched jack contact
