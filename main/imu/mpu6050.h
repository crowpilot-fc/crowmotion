// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// Crowmotion - MPU6050 6-axis IMU driver.

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float ax, ay, az;
    float gx, gy, gz;
    float temp_c;
    int16_t raw_accel[3];
    int16_t raw_gyro[3];
} imu_sample_t;

esp_err_t mpu6050_init(void);
esp_err_t mpu6050_read(imu_sample_t *out);
esp_err_t mpu6050_health_check(void);
esp_err_t mpu6050_recover(void);

#ifdef __cplusplus
}
#endif
