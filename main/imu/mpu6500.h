// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// FreeLook - DIY wireless FPV head tracker
// MPU6500 6-axis IMU driver (I2C).
//
// Original implementation from the InvenSense MPU-6500 register map / datasheet.
// Wiring (ESP32-C3 Super Mini or XIAO ESP32-C3): SDA = GPIO10, SCL = GPIO20,
// AD0 = GND (address 0x68), VCC = 3V3. On the XIAO these are pads D10 and D7.

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// One IMU reading, already converted to physical units.
typedef struct {
    float ax, ay, az;  // acceleration, g
    float gx, gy, gz;  // angular rate, degrees/second
    float temp_c;      // die temperature, Celsius
    int16_t raw_accel[3];
    int16_t raw_gyro[3];
} imu_sample_t;

// Probe and configure the MPU6500 over I2C.
// Returns:
//   ESP_OK              - found and configured
//   ESP_ERR_NOT_FOUND   - no I2C response / WHO_AM_I unreadable (IMU not wired)
//   other esp_err_t     - I2C bus setup failure
esp_err_t mpu6500_init(void);

// Read one accel + gyro + temperature sample. Must call mpu6500_init() first.
esp_err_t mpu6500_read(imu_sample_t *out);

#ifdef __cplusplus
}
#endif
