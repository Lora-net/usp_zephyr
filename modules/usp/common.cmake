# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0


#
add_compile_definitions(USE_SMTC_RAC)

#-----------------------------------------------------------------------------
# LBM
#-----------------------------------------------------------------------------
if(CONFIG_USP_LORA_BASICS_MODEM)
  include(${CMAKE_CURRENT_LIST_DIR}/../lora_basics_modem/common.cmake)
endif()

#-----------------------------------------------------------------------------
# USP
#-----------------------------------------------------------------------------
zephyr_include_directories(
  ${RAC_LIB_DIR}/smtc_rac_api
  ${RAC_LIB_DIR}/smtc_rac
  ${RAC_LIB_DIR}/radio_planner/src
)

zephyr_library_sources(
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_lora.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_flrc.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_fsk.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_lbt.c
  ${RAC_LIB_DIR}/smtc_rac/smtc_rac_lrfhss.c
 )

# RADIO_PLANNER_C_SOURCES
# zephyr_library_sources(
#   ${RAC_LIB_DIR}/radio_planner/src/radio_planner.c
#   ${RAC_LIB_DIR}/radio_planner/src/duty_cycle.c
# )
zephyr_library_sources(
  ${RAC_LIB_DIR}/radio_planner/src/radio_planner.c
)

if(NOT CONFIG_USP_LORA_BASICS_MODEM)
  zephyr_library_sources(
    ${RAC_LIB_DIR}/radio_planner/src/duty_cycle.c
  )
endif()


zephyr_include_directories(
  # NOTE: it is only used by the samples, could be cleaned up.
  ${RAC_LIB_DIR}/smtc_modem_hal
)
