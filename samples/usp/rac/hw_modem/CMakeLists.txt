# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hw_modem)

add_subdirectory(src)

zephyr_include_directories(${ZEPHYR_USP_ZEPHYR_CMAKE_DIR}/include/zephyr/usp)

# Nanopb and RAC serialization
target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/
  ${CMAKE_CURRENT_SOURCE_DIR}/serialization/nanopb
  ${CMAKE_CURRENT_SOURCE_DIR}/serialization/generated
  ${CMAKE_CURRENT_SOURCE_DIR}/serialization/helpers
)

target_sources(app PRIVATE
  # Nanopb core (copied to generated/ by build.sh)
  ${CMAKE_CURRENT_SOURCE_DIR}/serialization/generated/pb_common.c
  ${CMAKE_CURRENT_SOURCE_DIR}/serialization/generated/pb_decode.c
  ${CMAKE_CURRENT_SOURCE_DIR}/serialization/generated/pb_encode.c

  # Generated protobuf
  ${CMAKE_CURRENT_SOURCE_DIR}/serialization/generated/smtc_rac_context.pb.c

  # Helpers
  ${CMAKE_CURRENT_SOURCE_DIR}/serialization/helpers/rac_context_converter.c
)
