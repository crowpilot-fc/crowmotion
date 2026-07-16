// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// Firmware version and update endpoint.

#pragma once

#include "sdkconfig.h"

// Bump on each release. Compared against the update manifest's "version".
#define CROWMOTION_VERSION "0.1.3"

// Update manifest URL, one manifest per chip target so every board downloads
// an image built for it. The server serves JSON:
//   { "version": "0.2.0",
//     "url": "https://updates.crowpilot.in/crowmotion/crowmotion-0.2.0-esp32c3.bin" }
// If "version" is newer than CROWMOTION_VERSION, the device downloads "url".
// (The release workflow also keeps the legacy /crowmotion/latest.json for
// devices fielded before per-target manifests existed; those are all C3.)
#define CROWMOTION_UPDATE_URL \
    "https://updates.crowpilot.in/crowmotion/latest-" CONFIG_IDF_TARGET ".json"
