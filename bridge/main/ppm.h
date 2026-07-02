// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowLink - wireless trainer bridge for CrowMotion
// PPM trainer-signal generator (RMT-backed, jitter-free).
//
// Output: standard positive 8-channel PPM into a radio's trainer jack.
// Each channel starts with a 300 us high pulse; the start-to-start time is
// the channel value in microseconds (988..2012). A final pulse plus a low
// sync gap pads the frame to 22.5 ms. Signal idles low at 3.3V logic,
// which trainer inputs accept. Wire: jack tip = signal, sleeve = GND,
// ring unconnected.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PPM_NUM_CH 8
#define PPM_GPIO 2               // free, non-strap pin on the C6 Super Mini
#define PPM_FRAME_US 22500
#define PPM_PULSE_US 300

// Set up the RMT channel. Call once.
void ppm_init(void);

// Emit one full PPM frame (blocks ~22.5 ms). Values in microseconds,
// clamped to 988..2012. Call back-to-back for a continuous signal.
void ppm_send_frame(const uint16_t ch_us[PPM_NUM_CH]);

#ifdef __cplusplus
}
#endif
