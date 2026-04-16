# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_region_detection)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../geolocation/src/geolocation_bsp.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/geolocation/wifi_region_detection/wifi_region_finder.c
)

zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/geolocation/wifi_region_detection
)
