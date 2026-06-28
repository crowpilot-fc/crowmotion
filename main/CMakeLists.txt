# FreeLook main application component.
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRCS
        "freelook_main.c"
        "para/para_ble.c"
    INCLUDE_DIRS
        "."
        "para"
    PRIV_REQUIRES
        bt
        nvs_flash
)
