# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0
if(CONFIG_ZEPHYR_USP_MODULE)

  if(CONFIG_USP)

    set(USP_DIR ${ZEPHYR_CURRENT_MODULE_DIR})
    set(RAC_LIB_DIR ${USP_DIR}/smtc_rac_lib)
    set(SMTC_RADIO_DRIVERS_DIR ${RAC_LIB_DIR}/radio_drivers)
    set(SMTC_RAL_DIR ${RAC_LIB_DIR}/smtc_ral)
    set(SMTC_RALF_DIR ${RAC_LIB_DIR}/smtc_ralf)

    if(CONFIG_USP_LORA_BASICS_MODEM)
      set(LBM_LIB_DIR ${USP_DIR}/protocols/lbm_lib)
      set(LBM_SMTC_MODEM_CORE_DIR ${LBM_LIB_DIR}/smtc_modem_core)
    endif()

    zephyr_library()

    if(CONFIG_SEMTECH_LR11XX)
      include(${CMAKE_CURRENT_LIST_DIR}/../usp_drivers/lr11xx.cmake)
    endif()

    if(CONFIG_SEMTECH_LR20XX)
      include(${CMAKE_CURRENT_LIST_DIR}/../usp_drivers/lr20xx.cmake)
    endif()

    if(CONFIG_SEMTECH_SX126X)
      include(${CMAKE_CURRENT_LIST_DIR}/../usp_drivers/sx126x.cmake)
    endif()

    if(CONFIG_LORA_BASICS_MODEM_DRIVERS_RAL_RALF)
      zephyr_include_directories(
        ${RAC_LIB_DIR}/smtc_ral/src
        ${RAC_LIB_DIR}/smtc_ralf/src
      )
      zephyr_library_sources(
        ${SMTC_RAL_SOURCES}
        ${SMTC_RALF_SOURCES}
      )
    endif()

    include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

    zephyr_library_include_directories(
      ${CMAKE_CURRENT_LIST_DIR}/../smtc_modem_hal
    )

    zephyr_library_sources(
      ${CMAKE_CURRENT_LIST_DIR}/../smtc_modem_hal/smtc_modem_hal.c
      ${CMAKE_CURRENT_LIST_DIR}/../smtc_modem_hal/smtc_modem_hal_storage.c
      ${CMAKE_CURRENT_LIST_DIR}/../smtc_modem_hal/smtc_modem_hal_dbg_trace.c
    )

  endif()

endif()
