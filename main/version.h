// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Firmware version and update endpoint.

#pragma once

// Bump on each release. Compared against the update manifest's "version".
#define CROWMOTION_VERSION "0.1.3"

// Update manifest URL. The server (set up later) should serve JSON:
//   { "version": "0.2.0",
//     "url": "https://updates.crowpilot.in/crowmotion/crowmotion-0.2.0.bin" }
// If "version" differs from CROWMOTION_VERSION, the device downloads "url".
#define CROWMOTION_UPDATE_URL "https://updates.crowpilot.in/crowmotion/latest.json"
