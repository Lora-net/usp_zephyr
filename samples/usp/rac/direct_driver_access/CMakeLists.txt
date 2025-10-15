# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ranging_demo)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/direct_driver_access_example/app_direct_driver_access.c
)

zephyr_include_directories(
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/direct_driver_access_example
)
zephyr_include_directories(${ZEPHYR_USP_ZEPHYR_CMAKE_DIR}/include/zephyr/usp)
