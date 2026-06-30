// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Madgwick 6DOF (IMU-only) orientation filter.
//
// Original implementation of Sebastian Madgwick's published gradient-descent
// AHRS algorithm (accelerometer + gyroscope, no magnetometer). Yaw is not
// gravity-referenced and will drift slowly; that drift is handled separately by
// gyro auto-calibration (M4) and a recenter (M6).

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float q0, q1, q2, q3;  // orientation quaternion (w, x, y, z)
    float beta;            // filter gain (accel correction strength)
} madgwick_t;

// Initialize to identity orientation with the given gain (typical 0.1).
void madgwick_init(madgwick_t *f, float beta);

// Seed orientation from one accelerometer reading (gravity) so the filter
// starts level-correct instead of ramping from identity. Yaw is set to 0.
void madgwick_set_from_accel(madgwick_t *f, float ax, float ay, float az);

// Advance the filter. Gyro in rad/s, accel in any consistent unit (normalized
// internally), dt in seconds.
void madgwick_update_imu(madgwick_t *f, float gx, float gy, float gz,
                         float ax, float ay, float az, float dt);

// Read the current orientation as aerospace Euler angles, in degrees.
void madgwick_get_euler_deg(const madgwick_t *f, float *yaw, float *pitch,
                            float *roll);

#ifdef __cplusplus
}
#endif
