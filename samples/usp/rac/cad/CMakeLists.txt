# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(cad)

zephyr_include_directories(
    ${ZEPHYR_USP_ZEPHYR_CMAKE_DIR}/include/zephyr/usp
    ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/cad_example
)

target_sources(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/cad_example/app_cad.c
)