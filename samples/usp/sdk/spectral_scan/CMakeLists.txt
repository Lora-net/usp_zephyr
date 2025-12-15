# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spectral_scan)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/spectral_scan_example/app_spectral_scan.c
)

zephyr_include_directories(
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/spectral_scan_example
)

