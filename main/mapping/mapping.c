// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Axis-to-channel mapping (Milestone 5). Reads gain/limits/channels/invert and
// deadband from the live config; keeps only the recenter reference as state.

#include "mapping.h"

#include <math.h>

#include "config.h"
#include "para_ble.h"

static float s_ref[3];        // reference (center) yaw, pitch, roll
static int s_have_ref = 0;

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

    const freelook_config_t *c = config_get();
    float ang[2] = {yaw_deg, pitch_deg};  // 0 = pan, 1 = tilt

    struct {
        uint8_t ch, en;
        float gain;
        int8_t inv;
        uint16_t center, mn, mx;
    } ax[2] = {
        {c->pan_ch, c->pan_enabled, c->pan_gain, c->pan_invert,
         c->pan_center, c->pan_min, c->pan_max},
        {c->tilt_ch, c->tilt_enabled, c->tilt_gain, c->tilt_invert,
         c->tilt_center, c->tilt_min, c->tilt_max},
    };

    for (int i = 0; i < 2; i++) {
        if (!ax[i].en) {
            continue;
        }
        float rel = wrap180(ang[i] - s_ref[i]);

        // Dead zone around center, shifted so there is no jump at the edge.
        if (rel > c->deadband_deg) {
            rel -= c->deadband_deg;
        } else if (rel < -c->deadband_deg) {
            rel += c->deadband_deg;
        } else {
            rel = 0.0f;
        }

        float us = (float)ax[i].center + ax[i].inv * ax[i].gain * rel;
        if (us < ax[i].mn) us = ax[i].mn;
        if (us > ax[i].mx) us = ax[i].mx;
        para_ble_set_channel(ax[i].ch, (uint16_t)lroundf(us));
    }
}
