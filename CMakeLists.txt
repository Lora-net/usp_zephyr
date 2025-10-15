# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.12.4)

add_subdirectory(drivers)
add_subdirectory(subsys)

zephyr_include_directories(include)
