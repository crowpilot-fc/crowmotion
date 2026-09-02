// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar

#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CROWMOTION_OUTPUT_OFF = 0,
    CROWMOTION_OUTPUT_BLUETOOTH,
    CROWMOTION_OUTPUT_PPM,
} crowmotion_active_output_t;

esp_err_t output_manager_init(void);
void output_manager_pause(void);
void output_manager_resume(void);
bool output_manager_is_ble_connected(void);
bool output_manager_is_ready(void);
bool output_manager_is_jack_inserted(void);
crowmotion_active_output_t output_manager_active_output(void);

#ifdef __cplusplus
}
#endif
