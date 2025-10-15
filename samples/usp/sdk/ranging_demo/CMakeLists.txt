# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ranging_demo)

target_compile_definitions(app PRIVATE DISABLE_RANGING_LOG)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app_ranging_result_output.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/oled_display.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/ranging_demo/app_ranging_hopping.c
)

zephyr_include_directories(${ZEPHYR_USP_ZEPHYR_CMAKE_DIR}/include/zephyr/usp)

zephyr_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/ranging_demo
)
