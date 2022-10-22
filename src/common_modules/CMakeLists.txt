#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_include_directories(app PRIVATE .)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common_modules.c)
target_sources_ifdef(CONFIG_GNSS_MODULE app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gnss_module.c)
target_sources_ifdef(CONFIG_LED_CONTROL app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/led_module.c)
