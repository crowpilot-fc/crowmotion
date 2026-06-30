// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// FrSky PARA Bluetooth wireless trainer link.
//
// Original implementation. The PARA link is a wire protocol; it is
// reimplemented from its observable behavior, not copied from any project.
//
// Protocol summary (target radio: FrSky X20S / Tandem on EthOS):
//   - BLE GATT service UUID 0xFFF0.
//   - Channel data is delivered to the radio via GATT notify on the
//     channel characteristic (0xFFF6).
//   - On connect, the radio needs a "\r\n" nudge to switch into
//     trainer-receive mode.
//   - Channels are sent centered until the IMU pipeline is online.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Number of trainer channels CrowMotion reports.
#define CROWMOTION_PARA_NUM_CHANNELS 8

// Channel value range, in microseconds (RC convention). 1500 = centered.
#define CROWMOTION_PARA_CH_MIN 988
#define CROWMOTION_PARA_CH_CENTER 1500
#define CROWMOTION_PARA_CH_MAX 2012

// Start BLE, advertise the PARA service, and begin streaming channels.
// Until the IMU pipeline is wired (M2+), all channels are held at center.
// Returns 0 on success, negative on error.
int para_ble_start(void);

// Update the channel that the next PARA notify will send.
// ch is 0-based; value_us is clamped to [CH_MIN, CH_MAX].
// Safe to call from the tracker task once fusion/mapping is online (M5).
void para_ble_set_channel(uint8_t ch, uint16_t value_us);

// Read back the current value (microseconds) of a channel.
uint16_t para_ble_get_channel(uint8_t ch);

// True while a radio (central) is connected.
bool para_ble_is_connected(void);

// Pause the radio link (stop advertising, drop any connection) so WiFi config
// mode can run, and resume it afterwards.
void para_ble_pause(void);
void para_ble_resume(void);

#ifdef __cplusplus
}
#endif
