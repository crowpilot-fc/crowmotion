# FreeLook main application component.
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS
        "freelook_main.c"
        "para/para_ble.c"
        "imu/mpu6500.c"
        "fusion/madgwick.c"
        "mapping/mapping.c"
        "config/config.c"
        "web/webconfig.c"
    INCLUDE_DIRS
        "."
        "para"
        "imu"
        "fusion"
        "mapping"
        "config"
        "web"
    EMBED_FILES
        "web/index.html"
    PRIV_REQUIRES
        bt
        nvs_flash
        esp_driver_i2c
        esp_timer
        json
        esp_wifi
        esp_netif
        esp_event
        esp_http_server
)
