// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Axis-to-channel mapping (Milestone 5).
//
// Turns fused yaw/pitch/roll into PARA trainer channel values, with a per-axis
// output channel, gain, and inversion, relative to a recenter reference.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Capture the given orientation as the new center (the "look forward" zero).
void mapping_recenter(float yaw_deg, float pitch_deg, float roll_deg);

// Map orientation to PARA channels and push them. Call from the fusion loop.
void mapping_update(float yaw_deg, float pitch_deg, float roll_deg);

#ifdef __cplusplus
}
#endif
