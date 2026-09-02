// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar

#include "channel_state.h"

#include <string.h>

#include "freertos/FreeRTOS.h"

static uint16_t s_channels[CROWMOTION_CHANNEL_COUNT];
static uint32_t s_inhibit_mask;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static uint16_t clamp_us(uint16_t value)
{
    if (value < CROWMOTION_CHANNEL_MIN_US) return CROWMOTION_CHANNEL_MIN_US;
    if (value > CROWMOTION_CHANNEL_MAX_US) return CROWMOTION_CHANNEL_MAX_US;
    return value;
}

void channel_state_init(void)
{
    taskENTER_CRITICAL(&s_mux);
    for (int i = 0; i < CROWMOTION_CHANNEL_COUNT; i++) {
        s_channels[i] = CROWMOTION_CHANNEL_CENTER_US;
    }
    s_inhibit_mask = 0;
    taskEXIT_CRITICAL(&s_mux);
}

void channel_state_replace(const uint16_t values[CROWMOTION_CHANNEL_COUNT])
{
    taskENTER_CRITICAL(&s_mux);
    for (int i = 0; i < CROWMOTION_CHANNEL_COUNT; i++) {
        s_channels[i] = clamp_us(values[i]);
    }
    taskEXIT_CRITICAL(&s_mux);
}

uint16_t channel_state_get(uint8_t channel)
{
    if (channel >= CROWMOTION_CHANNEL_COUNT) return CROWMOTION_CHANNEL_CENTER_US;
    taskENTER_CRITICAL(&s_mux);
    uint16_t value = s_inhibit_mask ? CROWMOTION_CHANNEL_CENTER_US : s_channels[channel];
    taskEXIT_CRITICAL(&s_mux);
    return value;
}

void channel_state_copy(uint16_t out[CROWMOTION_CHANNEL_COUNT])
{
    taskENTER_CRITICAL(&s_mux);
    if (s_inhibit_mask) {
        for (int i = 0; i < CROWMOTION_CHANNEL_COUNT; i++) {
            out[i] = CROWMOTION_CHANNEL_CENTER_US;
        }
    } else {
        memcpy(out, s_channels, sizeof(s_channels));
    }
    taskEXIT_CRITICAL(&s_mux);
}

void channel_state_set_inhibit(channel_inhibit_reason_t reason, bool enabled)
{
    taskENTER_CRITICAL(&s_mux);
    if (enabled) {
        s_inhibit_mask |= (uint32_t)reason;
    } else {
        s_inhibit_mask &= ~(uint32_t)reason;
    }
    taskEXIT_CRITICAL(&s_mux);
}

bool channel_state_is_inhibited(void)
{
    taskENTER_CRITICAL(&s_mux);
    bool inhibited = s_inhibit_mask != 0;
    taskEXIT_CRITICAL(&s_mux);
    return inhibited;
}
