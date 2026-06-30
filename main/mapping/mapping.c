// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Axis-to-channel mapping (Milestone 5).

#include "mapping.h"

#include <math.h>

#include "para_ble.h"

// Axis order: 0 = pan (yaw), 1 = tilt (pitch), 2 = roll.
typedef struct {
    uint8_t ch;     // output PARA channel index
    float gain;     // microseconds per degree
    int invert;     // -1 to invert, +1 otherwise
} axis_cfg_t;

// Defaults: pan -> ch0, tilt -> ch1, roll -> ch2. 8 us/deg gives full
// deflection at about +/-64 degrees. Tune per taste (and per mounting).
static axis_cfg_t s_cfg[3] = {
    {0, 8.0f, +1},
    {1, 8.0f, +1},
    {2, 8.0f, +1},
};

static float s_ref[3];        // reference (center) yaw, pitch, roll
static int s_have_ref = 0;

void mapping_set_gain(float pan, float tilt, float roll)
{
    s_cfg[0].gain = pan;
    s_cfg[1].gain = tilt;
    s_cfg[2].gain = roll;
}

void mapping_recenter(float yaw_deg, float pitch_deg, float roll_deg)
{
    s_ref[0] = yaw_deg;
    s_ref[1] = pitch_deg;
    s_ref[2] = roll_deg;
    s_have_ref = 1;
}

// Normalize an angle difference into [-180, 180] degrees.
static float wrap180(float a)
{
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

void mapping_update(float yaw_deg, float pitch_deg, float roll_deg)
{
    if (!s_have_ref) {
        mapping_recenter(yaw_deg, pitch_deg, roll_deg);
    }

    // v1 outputs two axes: pan (yaw) and tilt (pitch). Roll is not mapped.
    float ang[3] = {yaw_deg, pitch_deg, roll_deg};
    for (int i = 0; i < 2; i++) {
        float rel = wrap180(ang[i] - s_ref[i]);
        float us = (float)FREELOOK_PARA_CH_CENTER + s_cfg[i].invert * s_cfg[i].gain * rel;
        if (us < (float)FREELOOK_PARA_CH_MIN) us = FREELOOK_PARA_CH_MIN;
        if (us > (float)FREELOOK_PARA_CH_MAX) us = FREELOOK_PARA_CH_MAX;
        para_ble_set_channel(s_cfg[i].ch, (uint16_t)lroundf(us));
    }
}
