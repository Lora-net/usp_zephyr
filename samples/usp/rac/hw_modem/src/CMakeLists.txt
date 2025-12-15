# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)
target_sources(app PRIVATE
  main.c cmd_parser.c git_version.c hw_modem.c
)

if(CONFIG_LORA_BASICS_MODEM_GEOLOCATION)
  target_sources(app PRIVATE geoloc_bsp.c)
endif()
