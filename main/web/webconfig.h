// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Config web UI controller: a WiFi access point serving a configuration page,
// brought up on demand (quad-tap) and torn down to return to the radio link.
// WiFi and the BLE radio link never carry traffic at the same time.

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Set up networking and the mode-control task. Call once at boot.
void webconfig_init(void);

// Toggle config mode. Entering is refused while a radio (BLE) link is active.
// Called from the quad-tap gesture and the UI "exit" button.
void webconfig_request_toggle(void);

// True while the config hotspot is up.
bool webconfig_is_active(void);

#ifdef __cplusplus
}
#endif
