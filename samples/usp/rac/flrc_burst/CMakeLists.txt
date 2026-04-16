# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(flrc_burst)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/flrc_burst_example/flrc_burst_rx.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/flrc_burst_example/flrc_burst_tx.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/flrc_burst_example/flrc_burst_utils.c
  )

zephyr_include_directories(
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/flrc_burst_example
)
