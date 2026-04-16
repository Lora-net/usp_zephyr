# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.12.4)

add_subdirectory(drivers)
add_subdirectory(subsys)

zephyr_include_directories(include)

# Allow to use west flash --runner pyocd on nucleo_l476rg and xiao_nrf54l15
if(BOARD STREQUAL "nucleo_l476rg")
  board_runner_args(pyocd "--target=stm32l476rgtx")
  include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
endif()
if(BOARD STREQUAL "xiao_nrf54l15")
  board_runner_args(pyocd "--target=nrf54l")
  include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
endif()
