// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// CrowMotion - DIY wireless FPV head tracker
// MPU6500 6-axis IMU driver (I2C), Milestone 2.
//
// Original implementation from the InvenSense MPU-6500 register map. Uses the
// ESP-IDF new-style I2C master driver (driver/i2c_master.h).

#include "mpu6500.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board.h"

static const char *TAG = "mpu6500";

// --- Wiring / bus config ---
// I2C pins come from the per-board profile (board.h), chosen so all wiring
// stays on one side of the selected board, next to its 3V3/GND pads.
#define MPU_I2C_PORT I2C_NUM_0
#define MPU_PIN_SDA CROWMOTION_I2C_SDA_GPIO
#define MPU_PIN_SCL CROWMOTION_I2C_SCL_GPIO
#define MPU_I2C_HZ 100000   // 100 kHz, tolerant of weak / internal-only pull-ups
#define MPU_ADDR 0x68   // AD0 = GND
#define MPU_IO_TIMEOUT_MS 100

// --- Register map ---
#define REG_SMPLRT_DIV 0x19
#define REG_CONFIG 0x1A
#define REG_GYRO_CONFIG 0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_ACCEL_CONFIG2 0x1D
#define REG_ACCEL_XOUT_H 0x3B
#define REG_PWR_MGMT_1 0x6B
#define REG_WHO_AM_I 0x75

#define WHO_AM_I_MPU6500 0x70

// Full-scale selections used below.
#define GYRO_FS_2000DPS 0x18   // GYRO_CONFIG[4:3] = 11
#define ACCEL_FS_2G 0x00       // ACCEL_CONFIG[4:3] = 00

// Sensitivity for the ranges selected above.
#define ACCEL_LSB_PER_G 16384.0f   // +/- 2 g
#define GYRO_LSB_PER_DPS 16.4f     // +/- 2000 dps
#define TEMP_SENSITIVITY 333.87f
#define TEMP_OFFSET_C 21.0f

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), MPU_IO_TIMEOUT_MS);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t len)
{
    // Separate write-then-read (not a repeated-start transmit_receive), which is
    // more tolerant of marginal pull-ups on this board.
    esp_err_t e = i2c_master_transmit(s_dev, &reg, 1, MPU_IO_TIMEOUT_MS);
    if (e != ESP_OK) {
        return e;
    }
    return i2c_master_receive(s_dev, buf, len, MPU_IO_TIMEOUT_MS);
}

esp_err_t mpu6500_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = MPU_I2C_PORT,
        .scl_io_num = MPU_PIN_SCL,
        .sda_io_num = MPU_PIN_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU_ADDR,
        .scl_speed_hz = MPU_I2C_HZ,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c add device failed: %s", esp_err_to_name(err));
        return err;
    }

    // Detect with a probe (robust even when a plain transaction reports the bus
    // busy). Check both 0x68 and 0x69 so a floating AD0 is distinguishable from
    // a dead bus.
    uint8_t found = 0;
    for (int attempt = 0; attempt < 10 && found == 0; attempt++) {
        if (i2c_master_probe(s_bus, 0x68, 50) == ESP_OK) {
            found = 0x68;
        } else if (i2c_master_probe(s_bus, 0x69, 50) == ESP_OK) {
            found = 0x69;
        } else {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "No MPU on SDA=GPIO%d SCL=GPIO%d at 0x68 or 0x69. "
                      "Check 3V3, GND, and the SDA/SCL solder joints.",
                 MPU_PIN_SDA, MPU_PIN_SCL);
        return ESP_ERR_NOT_FOUND;
    }
    if (found != MPU_ADDR) {
        ESP_LOGW(TAG, "MPU answers at 0x%02X, not 0x%02X: AD0 is floating, "
                      "tie AD0 to GND.", found, MPU_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t who = 0;
    for (int attempt = 0; attempt < 10; attempt++) {
        err = reg_read(REG_WHO_AM_I, &who, 1);
        if (err == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "probe OK but WHO_AM_I read failed (%s) at 0x%02X",
                 esp_err_to_name(err), MPU_ADDR);
        return ESP_ERR_NOT_FOUND;
    }
    if (who != WHO_AM_I_MPU6500) {
        // Many low-cost modules are MPU6500 clones with a different id. Warn but
        // continue so bring-up is not blocked by a cosmetic mismatch.
        ESP_LOGW(TAG, "WHO_AM_I = 0x%02X (expected 0x%02X); continuing anyway",
                 who, WHO_AM_I_MPU6500);
    } else {
        ESP_LOGI(TAG, "MPU6500 detected (WHO_AM_I = 0x%02X)", who);
    }

    // Reset, then wake with the best available clock (auto-select PLL).
    reg_write(REG_PWR_MGMT_1, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));
    reg_write(REG_PWR_MGMT_1, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));

    // DLPF ~41 Hz on gyro and accel; 200 Hz sample rate (1 kHz / (1 + 4)).
    reg_write(REG_CONFIG, 0x03);
    reg_write(REG_GYRO_CONFIG, GYRO_FS_2000DPS);
    reg_write(REG_ACCEL_CONFIG, ACCEL_FS_2G);
    reg_write(REG_ACCEL_CONFIG2, 0x03);
    reg_write(REG_SMPLRT_DIV, 0x04);

    ESP_LOGI(TAG, "MPU6500 configured: +/-2g, +/-2000dps, DLPF 41Hz, 200Hz ODR");
    return ESP_OK;
}

esp_err_t mpu6500_read(imu_sample_t *out)
{
    uint8_t b[14];
    esp_err_t err = reg_read(REG_ACCEL_XOUT_H, b, sizeof(b));
    if (err != ESP_OK) {
        return err;
    }

    int16_t ax = (int16_t)((b[0] << 8) | b[1]);
    int16_t ay = (int16_t)((b[2] << 8) | b[3]);
    int16_t az = (int16_t)((b[4] << 8) | b[5]);
    int16_t t  = (int16_t)((b[6] << 8) | b[7]);
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
    out->temp_c = t / TEMP_SENSITIVITY + TEMP_OFFSET_C;
    return ESP_OK;
}
