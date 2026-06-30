// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Madgwick 6DOF (IMU-only) orientation filter, Milestone 3.
//
// Original implementation of the published Madgwick gradient-descent AHRS
// (IMU variant). Written from the algorithm, not copied from any project.

#include "madgwick.h"

#include <math.h>

void madgwick_init(madgwick_t *f, float beta)
{
    f->q0 = 1.0f;
    f->q1 = 0.0f;
    f->q2 = 0.0f;
    f->q3 = 0.0f;
    f->beta = beta;
}

void madgwick_set_from_accel(madgwick_t *f, float ax, float ay, float az)
{
    // Roll about X and pitch about Y from the gravity direction; yaw = 0.
    float roll = atan2f(ay, az);
    float pitch = atan2f(-ax, sqrtf(ay * ay + az * az));
    float cr = cosf(roll * 0.5f), sr = sinf(roll * 0.5f);
    float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
    f->q0 = cp * cr;
    f->q1 = cp * sr;
    f->q2 = sp * cr;
    f->q3 = -sp * sr;
}

void madgwick_update_imu(madgwick_t *f, float gx, float gy, float gz,
                         float ax, float ay, float az, float dt)
{
    float q0 = f->q0, q1 = f->q1, q2 = f->q2, q3 = f->q3;
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;

    // Rate of change of quaternion from the gyroscope.
    qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    qDot2 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
    qDot3 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
    qDot4 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

    // Apply accelerometer correction only if the reading is usable.
    if (!(ax == 0.0f && ay == 0.0f && az == 0.0f)) {
        recipNorm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        float _2q0 = 2.0f * q0, _2q1 = 2.0f * q1, _2q2 = 2.0f * q2, _2q3 = 2.0f * q3;
        float _4q0 = 4.0f * q0, _4q1 = 4.0f * q1, _4q2 = 4.0f * q2;
        float _8q1 = 8.0f * q1, _8q2 = 8.0f * q2;
        float q0q0 = q0 * q0, q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3;

        // Gradient descent step (objective: align accel with gravity).
        s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 +
             _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 +
             _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;
        recipNorm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;

        qDot1 -= f->beta * s0;
        qDot2 -= f->beta * s1;
        qDot3 -= f->beta * s2;
        qDot4 -= f->beta * s3;
    }

    // Integrate and renormalize.
    q0 += qDot1 * dt;
    q1 += qDot2 * dt;
    q2 += qDot3 * dt;
    q3 += qDot4 * dt;
    recipNorm = 1.0f / sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    f->q0 = q0 * recipNorm;
    f->q1 = q1 * recipNorm;
    f->q2 = q2 * recipNorm;
    f->q3 = q3 * recipNorm;
}

void madgwick_get_euler_deg(const madgwick_t *f, float *yaw, float *pitch,
                            float *roll)
{
    float q0 = f->q0, q1 = f->q1, q2 = f->q2, q3 = f->q3;
    const float rad_to_deg = 57.29577951f;

    float r = atan2f(2.0f * (q0 * q1 + q2 * q3), 1.0f - 2.0f * (q1 * q1 + q2 * q2));
    float sp = 2.0f * (q0 * q2 - q3 * q1);
    if (sp > 1.0f) sp = 1.0f;
    if (sp < -1.0f) sp = -1.0f;
    float p = asinf(sp);
    float y = atan2f(2.0f * (q0 * q3 + q1 * q2), 1.0f - 2.0f * (q2 * q2 + q3 * q3));

    *roll = r * rad_to_deg;
    *pitch = p * rad_to_deg;
    *yaw = y * rad_to_deg;
}
