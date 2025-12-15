# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ping_pong)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/ping_pong_example/app_periodic_uplink.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/ping_pong_example/app_ping_pong.c
)

zephyr_include_directories(
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/ping_pong_example
)


# Activate LoRa periodic uplink
option(ENABLE_PERIODIC_UPLINK "Enable the periodic uplink" OFF)

if(ENABLE_PERIODIC_UPLINK)
  target_compile_definitions(app PRIVATE ENABLE_PERIODIC_UPLINK)
  message(STATUS "Periodic uplink enabled")
else()
  message(STATUS "Periodic uplink disabled")
endif()
