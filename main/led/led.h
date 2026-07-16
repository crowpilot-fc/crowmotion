// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Onboard status LED. The pin and type (plain GPIO or WS2812) come from the
// board profile in board.h. Driven only after boot, so boot-time strapping
// levels are unaffected.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_OFF,
    LED_SEARCHING,   // advertising, waiting for the radio: slow heartbeat
    LED_CONNECTED,   // radio link up: solid on
    LED_CONFIG,      // WiFi config hotspot active: double-blink
    LED_OTA,         // firmware update in progress: fast blink
} led_state_t;

// Configure the LED and start the pattern task.
void led_init(void);

// Set the status pattern. Ignored while a fault is latched.
void led_set(led_state_t s);

// Latch an error blink-code (e.g. 2 = IMU not found). Overrides status until
// cleared. Pass 0 to clear.
void led_fault(int blinks);

// One brief confirmation flash (e.g. a recenter), then resume the pattern.
void led_flash(void);

#ifdef __cplusplus
}
#endif
