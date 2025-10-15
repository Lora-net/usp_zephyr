# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0


if(CONFIG_USP)

  zephyr_library()

  if(CONFIG_USP_MAIN_THREAD)
    zephyr_library_sources(
      ${CMAKE_CURRENT_LIST_DIR}/zephyr_usp_thread.c
      ${CMAKE_CURRENT_LIST_DIR}/zephyr_usp_initialization.c
      ${CMAKE_CURRENT_LIST_DIR}/zephyr_usp_api_call.c
    )
  endif()

  zephyr_library_sources(
    ${CMAKE_CURRENT_LIST_DIR}/smtc_sw_platform_helper.c
  )

endif()
