// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// Crowmotion - shared trainer-channel state.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CROWMOTION_CHANNEL_COUNT 8
#define CROWMOTION_CHANNEL_MIN_US 988
#define CROWMOTION_CHANNEL_CENTER_US 1500
#define CROWMOTION_CHANNEL_MAX_US 2012

typedef enum {
    CHANNEL_INHIBIT_OUTPUT_SWITCH = 1u << 0,
    CHANNEL_INHIBIT_IMU_FAULT = 1u << 1,
} channel_inhibit_reason_t;

void channel_state_init(void);
void channel_state_replace(const uint16_t values[CROWMOTION_CHANNEL_COUNT]);
uint16_t channel_state_get(uint8_t channel);
void channel_state_copy(uint16_t out[CROWMOTION_CHANNEL_COUNT]);
void channel_state_set_inhibit(channel_inhibit_reason_t reason, bool enabled);
bool channel_state_is_inhibited(void);

#ifdef __cplusplus
}
#endif
