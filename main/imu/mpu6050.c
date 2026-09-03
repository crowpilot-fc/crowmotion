// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// Crowmotion - MPU6050 I2C driver for the locked hardware BOM.

#include "mpu6050.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "board.h"

static const char *TAG = "mpu6050";

#define MPU_I2C_PORT I2C_NUM_0
#define MPU_I2C_HZ 100000
#define MPU_ADDR 0x68
#define MPU_IO_TIMEOUT_MS 20

#define REG_SMPLRT_DIV 0x19
#define REG_CONFIG 0x1A
#define REG_GYRO_CONFIG 0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_XOUT_H 0x3B
#define REG_PWR_MGMT_1 0x6B
#define REG_WHO_AM_I 0x75

#define WHO_AM_I_MPU6050 0x68
#define GYRO_FS_2000DPS 0x18
#define ACCEL_FS_2G 0x00
#define ACCEL_LSB_PER_G 16384.0f
#define GYRO_LSB_PER_DPS 16.4f
#define TEMP_LSB_PER_C 340.0f
#define TEMP_OFFSET_C 36.53f

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

static esp_err_t reg_write(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(s_dev, data, sizeof(data), MPU_IO_TIMEOUT_MS);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len,
                                       MPU_IO_TIMEOUT_MS);
}

static esp_err_t configure_sensor(void)
{
    esp_err_t err = reg_write(REG_PWR_MGMT_1, 0x80);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));

    err = reg_write(REG_PWR_MGMT_1, 0x01);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(10));

    if ((err = reg_write(REG_CONFIG, 0x03)) != ESP_OK) return err;
    if ((err = reg_write(REG_GYRO_CONFIG, GYRO_FS_2000DPS)) != ESP_OK) return err;
    if ((err = reg_write(REG_ACCEL_CONFIG, ACCEL_FS_2G)) != ESP_OK) return err;
    if ((err = reg_write(REG_SMPLRT_DIV, 0x04)) != ESP_OK) return err;
    return ESP_OK;
}

esp_err_t mpu6050_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = MPU_I2C_PORT,
        .scl_io_num = CROWMOTION_I2C_SCL_GPIO,
        .sda_io_num = CROWMOTION_I2C_SDA_GPIO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU_ADDR,
        .scl_speed_hz = MPU_I2C_HZ,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C device setup failed: %s", esp_err_to_name(err));
        return err;
    }

    if (i2c_master_probe(s_bus, MPU_ADDR, 100) != ESP_OK) {
        ESP_LOGE(TAG, "No MPU6050 at 0x68 on SDA=GPIO%d SCL=GPIO%d",
                 CROWMOTION_I2C_SDA_GPIO, CROWMOTION_I2C_SCL_GPIO);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t who = 0;
    err = reg_read(REG_WHO_AM_I, &who, 1);
    if (err != ESP_OK || who != WHO_AM_I_MPU6050) {
        ESP_LOGE(TAG, "Unexpected WHO_AM_I 0x%02X, expected 0x%02X",
                 who, WHO_AM_I_MPU6050);
        return ESP_ERR_NOT_FOUND;
    }

    err = configure_sensor();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sensor configuration failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "MPU6050 ready: +/-2g, +/-2000dps, DLPF 42Hz, 200Hz ODR");
    return ESP_OK;
}

esp_err_t mpu6050_read(imu_sample_t *out)
{
    if (out == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t b[14];
    esp_err_t err = reg_read(REG_ACCEL_XOUT_H, b, sizeof(b));
    if (err != ESP_OK) return err;

    int16_t ax = (int16_t)((b[0] << 8) | b[1]);
    int16_t ay = (int16_t)((b[2] << 8) | b[3]);
    int16_t az = (int16_t)((b[4] << 8) | b[5]);
    int16_t temp = (int16_t)((b[6] << 8) | b[7]);
    int16_t gx = (int16_t)((b[8] << 8) | b[9]);
    int16_t gy = (int16_t)((b[10] << 8) | b[11]);
    int16_t gz = (int16_t)((b[12] << 8) | b[13]);

    out->raw_accel[0] = ax;
    out->raw_accel[1] = ay;
    out->raw_accel[2] = az;
    out->raw_gyro[0] = gx;
    out->raw_gyro[1] = gy;
    out->raw_gyro[2] = gz;
    out->ax = ax / ACCEL_LSB_PER_G;
    out->ay = ay / ACCEL_LSB_PER_G;
    out->az = az / ACCEL_LSB_PER_G;
    out->gx = gx / GYRO_LSB_PER_DPS;
    out->gy = gy / GYRO_LSB_PER_DPS;
    out->gz = gz / GYRO_LSB_PER_DPS;
    out->temp_c = temp / TEMP_LSB_PER_C + TEMP_OFFSET_C;
    return ESP_OK;
}

esp_err_t mpu6050_health_check(void)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;
    uint8_t who = 0;
    uint8_t power = 0;
    esp_err_t err = reg_read(REG_WHO_AM_I, &who, 1);
    if (err != ESP_OK) return err;
    if (who != WHO_AM_I_MPU6050) return ESP_ERR_INVALID_RESPONSE;
    err = reg_read(REG_PWR_MGMT_1, &power, 1);
    if (err != ESP_OK) return err;
    return (power & 0x40) ? ESP_ERR_INVALID_STATE : ESP_OK;
}

esp_err_t mpu6050_recover(void)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;
    esp_err_t err = configure_sensor();
    if (err == ESP_OK) err = mpu6050_health_check();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "MPU6050 recovered and reconfigured");
    } else {
        ESP_LOGW(TAG, "MPU6050 recovery failed: %s", esp_err_to_name(err));
    }
    return err;
}
