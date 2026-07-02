// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// CrowLink transmitter: broadcasts the trainer channels over ESP-NOW so a
// CrowLink bridge at the radio can turn them into wired PPM. Runs alongside
// the BLE PARA link (software coexistence), gated by config (bridge_en).

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Create the transmit task. Call once at boot, after webconfig_init() (needs
// the default event loop) and after para_ble_start() (reads its channels).
void crowlink_tx_init(void);

// Tear down WiFi so the config hotspot can own the radio, and hand it back
// afterwards. Called by webconfig around config mode, mirroring para_ble.
void crowlink_tx_pause(void);
void crowlink_tx_resume(void);

#ifdef __cplusplus
}
#endif
