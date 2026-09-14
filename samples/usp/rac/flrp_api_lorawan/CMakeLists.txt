# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(flrp_api_lorawan)

zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/ranging_demo/src
)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/flrp_api.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/flrp_api_menu.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/flrp_remote.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/flrp_display.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/ranging_demo/src/oled_display.c
)
