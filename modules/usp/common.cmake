# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0


# USE_SMTC_RAC is used by the USP headers (smtc_rac.h, radio planner hook IDs enum): it must be global
zephyr_compile_definitions(USE_SMTC_RAC)

# MODEM_HAL_DBG_TRACE is used by smtc_core_hal_trace.h, included by the USP library and the application: it must be global
if(CONFIG_USP_MODEM_HAL_DBG_TRACE)
  zephyr_compile_definitions(MODEM_HAL_DBG_TRACE=1)
else()
  zephyr_compile_definitions(MODEM_HAL_DBG_TRACE=0)
endif()

# HAL_DBG_TRACE is used by smtc_hal_dbg_trace.h, included by the USP helpers and the application: it must be global
if(CONFIG_USP_HAL_DBG_TRACE)
  zephyr_compile_definitions(HAL_DBG_TRACE=1)
else()
  zephyr_compile_definitions(HAL_DBG_TRACE=0)
endif()

#-----------------------------------------------------------------------------
# FLRP (FLRC Protocol)
#-----------------------------------------------------------------------------
if(CONFIG_USP_FLRP)
  include(${CMAKE_CURRENT_LIST_DIR}/../flrc_protocol/common.cmake)

  # FLRP definitions change the radio planner hook IDs enum (radio_planner_hook_id_defs.h, included through
  # smtc_rac_api.h): they must be identical in the USP library and in the application, so they are made global.
  zephyr_link_libraries(flrp_compile_definitions)
endif()

#-----------------------------------------------------------------------------
# LBM
#-----------------------------------------------------------------------------
if(CONFIG_USP_LORA_BASICS_MODEM)
  zephyr_compile_definitions(USE_LBM)
  include(${CMAKE_CURRENT_LIST_DIR}/../lora_basics_modem/common.cmake)

  # LBM definitions (ADD_CLASS_B, ADD_CSMA, ADD_LBM_GEOLOCATION, ADD_RELAY_*, NUMBER_OF_STACKS, ...) change the radio
  # planner hook IDs enum and LBM public types: they must be identical in the USP library and in the application,
  # so they are made global instead of being linked to the USP library only.
  zephyr_link_libraries(lbm_compile_definitions)
endif()

#-----------------------------------------------------------------------------
# USP
#-----------------------------------------------------------------------------
zephyr_include_directories(
  ${RAC_LIB_DIR}/smtc_rac_api
  ${RAC_LIB_DIR}/smtc_rac
  ${RAC_LIB_DIR}/radio_planner/src
  ${RAC_LIB_DIR}/smtc_core_hal_interface
)

zephyr_library_sources(
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_lora.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_flrc.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_flrc_burst.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_fsk.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_lbt.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_lrfhss.c
 )

# RADIO_PLANNER_C_SOURCES
# zephyr_library_sources(
#   ${RAC_LIB_DIR}/radio_planner/src/radio_planner.c
#   ${RAC_LIB_DIR}/radio_planner/src/duty_cycle.c
# )
# duplicated  duty_cyle.c both in LBM and here when no LBM to be reworked
zephyr_library_sources(
  ${RAC_LIB_DIR}/radio_planner/src/radio_planner.c
)

if(NOT CONFIG_USP_LORA_BASICS_MODEM)
  zephyr_library_sources(
    ${RAC_LIB_DIR}/radio_planner/src/duty_cycle.c
  )
endif()