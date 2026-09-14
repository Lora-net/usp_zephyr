# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(flrp_api)

target_sources(app PRIVATE
  src/main.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/flrp_api_example/app_flrp_api.c
)

zephyr_include_directories(
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/flrp_api_example
)