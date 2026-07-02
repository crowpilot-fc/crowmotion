# CrowLink bridge application component.
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS
        "bridge_main.c"
        "ppm.c"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        crowlink
        nvs_flash
        esp_wifi
        esp_event
        esp_timer
        esp_driver_gpio
        esp_driver_rmt
)
