# FreeLook main application component.
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS
        "freelook_main.c"
        "para/para_ble.c"
        "imu/mpu6500.c"
    INCLUDE_DIRS
        "."
        "para"
        "imu"
    PRIV_REQUIRES
        bt
        nvs_flash
        esp_driver_i2c
)
