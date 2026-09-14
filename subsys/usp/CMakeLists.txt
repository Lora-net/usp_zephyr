# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0


if(CONFIG_USP)

  zephyr_library()

  # Add include directory for smtc_hal_linux headers
  zephyr_include_directories(${ZEPHYR_USP_MODULE_DIR}/examples/smtc_hal_linux)

  if(CONFIG_USP_MAIN_THREAD)
    zephyr_library_sources(
      ${CMAKE_CURRENT_LIST_DIR}/zephyr_usp_thread.c
      ${CMAKE_CURRENT_LIST_DIR}/zephyr_usp_initialization.c
    )
  endif()

  zephyr_library_sources(
    ${CMAKE_CURRENT_LIST_DIR}/smtc_sw_platform_helper.c
    ${CMAKE_CURRENT_LIST_DIR}/smtc_zephyr_log_custom.c
    ${CMAKE_CURRENT_LIST_DIR}/smtc_modem_helper.c
    ${CMAKE_CURRENT_LIST_DIR}/smtc_hal_led.c
  )

  if(CONFIG_USP_LORA_BASICS_MODEM)
    zephyr_library_link_libraries(lbm_compile_definitions)
  endif()

endif()
