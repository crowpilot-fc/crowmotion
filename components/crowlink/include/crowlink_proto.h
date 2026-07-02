// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// CrowLink wire protocol: trainer channels over ESP-NOW, broadcast by the
// tracker and turned into wired PPM by the CrowLink bridge at the radio.
//
// v1 keeps it deliberately simple: broadcast frames on a fixed WiFi channel,
// identified by magic + version and integrity-checked with an XOR sum. A
// bind-by-MAC unicast mode can be layered on later without changing the
// frame format.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CROWLINK_MAGIC 0x4C575243u  // "CRWL" (little-endian)
#define CROWLINK_PROTO_VER 1
#define CROWLINK_NUM_CH 8
#define CROWLINK_RATE_HZ 50          // tracker send cadence
#define CROWLINK_WIFI_CHANNEL 1      // fixed 2.4 GHz channel for ESP-NOW
#define CROWLINK_FAILSAFE_MS 250     // bridge centers channels after this

typedef struct __attribute__((packed)) {
    uint32_t magic;                  // CROWLINK_MAGIC
    uint8_t ver;                     // CROWLINK_PROTO_VER
    uint8_t seq;                     // rolling counter (loss diagnostics)
    uint16_t ch[CROWLINK_NUM_CH];    // microseconds, 988..2012, 1500 = center
    uint8_t xsum;                    // XOR of every preceding byte
} crowlink_frame_t;

// XOR checksum over all bytes before the xsum field.
static inline uint8_t crowlink_xsum(const crowlink_frame_t *f)
{
    const uint8_t *p = (const uint8_t *)f;
    uint8_t x = 0;
    for (unsigned i = 0; i + 1 < sizeof(*f); i++) {
        x ^= p[i];
    }
    return x;
}

#ifdef __cplusplus
}
#endif
