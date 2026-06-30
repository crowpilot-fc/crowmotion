// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Shared tracker state: the fusion task publishes live values here and the
// config web server reads them and posts commands back.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float yaw, pitch, roll;     // fused angles, degrees (after recenter ref applied in mapping)
    uint16_t pan_us, tilt_us;   // current output channel values, microseconds
} tracker_snapshot_t;

// Latest live values for the UI.
void tracker_get(tracker_snapshot_t *out);

// Request a recenter on the next fusion loop (double-tap / UI button).
void tracker_request_recenter(void);

// Request auto-detection of the board orientation from the current gravity
// vector (hold the board in the worn position, then call this).
void tracker_request_autodetect(void);

#ifdef __cplusplus
}
#endif
