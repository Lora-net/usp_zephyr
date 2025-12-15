# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(usp_multiprocol)

target_sources(app PRIVATE
  src/main.c
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/ranging_demo/app_ranging_hopping.c
)

zephyr_include_directories(
  ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/ranging_demo
)

# Apply the same compile definitions as LBM library
# Link against the INTERFACE library that contains all LBM compile definitions
target_link_libraries(app PRIVATE lbm_compile_definitions)
