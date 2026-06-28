# FreeLook main application component.
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS
        "freelook_main.c"
        "para/para_ble.c"
        "imu/mpu6500.c"
        "fusion/madgwick.c"
    INCLUDE_DIRS
        "."
        "para"
        "imu"
        "fusion"
    PRIV_REQUIRES
        bt
        nvs_flash
        esp_driver_i2c
        esp_timer
)
