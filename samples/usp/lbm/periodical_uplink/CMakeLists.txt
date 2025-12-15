# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lorawan_periodical_uplink)

target_sources(app PRIVATE
  src/main.c
)

# Apply the same compile definitions as LBM library
# Link against the INTERFACE library that contains all LBM compile definitions
target_link_libraries(app PRIVATE lbm_compile_definitions)
