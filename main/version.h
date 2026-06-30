// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// Firmware version and update endpoint.

#pragma once

// Bump on each release. Compared against the update manifest's "version".
#define FREELOOK_VERSION "0.1.0"

// Update manifest URL. The server (set up later) should serve JSON:
//   { "version": "0.2.0",
//     "url": "https://updates.robosutra.com/freelook/freelook-0.2.0.bin" }
// If "version" differs from FREELOOK_VERSION, the device downloads "url".
#define FREELOOK_UPDATE_URL "https://updates.robosutra.com/freelook/latest.json"
