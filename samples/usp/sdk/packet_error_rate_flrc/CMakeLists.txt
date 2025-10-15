# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(per_flrc)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/packet_error_rate_flrc_example/app_per_flrc.c
)

zephyr_include_directories(
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/packet_error_rate_flrc_example
)

zephyr_include_directories(${ZEPHYR_USP_ZEPHYR_CMAKE_DIR}/include/zephyr/usp)


# set(ROLE "" CACHE STRING "Role of the device (TRANSMITTER or RECEIVER)")
# if(NOT ROLE)
#   message(ERROR "ROLE not defined, using default value")
# else()
#   target_compile_definitions(app PRIVATE ROLE=${ROLE})
# endif()
