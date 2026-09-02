// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "channel_state.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CROWMOTION_PPM_FRAME_US 22500
#define CROWMOTION_PPM_PULSE_US 300

esp_err_t ppm_output_init(int gpio_num);
esp_err_t ppm_output_set_enabled(bool enabled, bool inverted);
esp_err_t ppm_output_send_frame(
    const uint16_t channels[CROWMOTION_CHANNEL_COUNT], bool inverted);

#ifdef __cplusplus
}
#endif
