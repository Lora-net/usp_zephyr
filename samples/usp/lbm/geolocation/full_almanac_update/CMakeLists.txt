# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(full_almanac_update)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../geolocation/src/geolocation_bsp.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/geolocation/full_almanac_update/app_full_almanac_update.c
)

zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/geolocation/full_almanac_update/
)

# Apply the same compile definitions as LBM library
# Link against the INTERFACE library that contains all LBM compile definitions
target_link_libraries(app PRIVATE lbm_compile_definitions)