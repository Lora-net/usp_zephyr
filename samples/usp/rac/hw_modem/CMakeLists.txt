# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hw_modem)

# Use serialization files from USP module
set(USP_HW_MODEM_DIR ${ZEPHYR_USP_MODULE_DIR}/examples/main_examples/hw_modem)

zephyr_include_directories(
    ${ZEPHYR_USP_ZEPHYR_CMAKE_DIR}/include/zephyr/usp
    ${USP_HW_MODEM_DIR}
)

# Nanopb and RAC serialization
target_include_directories(app PRIVATE
  ${USP_HW_MODEM_DIR}/serialization/nanopb
  ${USP_HW_MODEM_DIR}/serialization/generated
  ${USP_HW_MODEM_DIR}/serialization/helpers
)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/hw_modem.c
  ${USP_HW_MODEM_DIR}/cmd_parser.c
  ${USP_HW_MODEM_DIR}/git_version.c

  # Nanopb core (copied to generated/ by build.sh)
  ${USP_HW_MODEM_DIR}/serialization/generated/pb_common.c
  ${USP_HW_MODEM_DIR}/serialization/generated/pb_decode.c
  ${USP_HW_MODEM_DIR}/serialization/generated/pb_encode.c

  # Generated protobuf
  ${USP_HW_MODEM_DIR}/serialization/generated/smtc_rac_context.pb.c

  # Helpers
  ${USP_HW_MODEM_DIR}/serialization/helpers/rac_context_converter.c
)

if(CONFIG_LORA_BASICS_MODEM_GEOLOCATION)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../lbm/geolocation/src/geolocation_bsp.c
    )
endif()

# Apply the same compile definitions as LBM library
# Link against the INTERFACE library that contains all LBM compile definitions
target_link_libraries(app PRIVATE lbm_compile_definitions)

# Apply the same compile definitions as FLRP library
# Link against the INTERFACE library that contains all FLRP compile definitions
if(CONFIG_USP_FLRP)
    target_link_libraries(app PRIVATE flrp_compile_definitions)
endif()
