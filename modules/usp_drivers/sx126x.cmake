# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

# Used by LBM
zephyr_library_compile_definitions(SX126X)
zephyr_library_compile_definitions(SX126X_TRANSCEIVER)
zephyr_library_compile_definitions(SX126X_DISABLE_WARNINGS)
zephyr_library_compile_definitions_ifdef(CONFIG_DT_HAS_SEMTECH_SX1261_NEW_ENABLED SX1261)
zephyr_library_compile_definitions_ifdef(CONFIG_DT_HAS_SEMTECH_SX1262_NEW_ENABLED SX1262)
zephyr_library_compile_definitions_ifdef(CONFIG_DT_HAS_SEMTECH_SX1268_NEW_ENABLED SX1268)

# Allow modem options
set(ALLOW_CSMA_BUILD true)

set(LBM_SX126X_LIB_DIR ${SMTC_RADIO_DRIVERS_DIR}/sx126x_driver/src)
zephyr_include_directories(${LBM_SX126X_LIB_DIR})

#-----------------------------------------------------------------------------
# Radio specific sources
#-----------------------------------------------------------------------------
set(SX126X_ENABLE_LR_FHSS true)
set(LR_FHSS_SRC_PATH ${LBM_SX126X_LIB_DIR} CACHE PATH "Path to folder containing LR-FHSS driver")
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/dev_env.cmake")
  include("${CMAKE_CURRENT_LIST_DIR}/dev_env.cmake")
endif()
zephyr_include_directories(${LR_FHSS_SRC_PATH})

zephyr_library_sources(
  ${LR_FHSS_SRC_PATH}/lr_fhss_mac.c
  ${LBM_SX126X_LIB_DIR}/sx126x.c
  ${LBM_SX126X_LIB_DIR}/sx126x_driver_version.c
  ${LBM_SX126X_LIB_DIR}/sx126x_lr_fhss.c
)

# Used later
set(SMTC_RAL_SOURCES ${SMTC_RAL_DIR}/src/ral_sx126x.c)
set(SMTC_RALF_SOURCES ${SMTC_RALF_DIR}/src/ralf_sx126x.c)

#-----------------------------------------------------------------------------
# Radio specific compilation flags
#-----------------------------------------------------------------------------

zephyr_compile_definitions(
  SX126X
)
