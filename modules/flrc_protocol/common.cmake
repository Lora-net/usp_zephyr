# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

# Create an INTERFACE library that contains all FLRP compile definitions.
# This library can be linked by applications that need the same definitions as FLRP.
# Applications can use: target_link_libraries(app PRIVATE flrp_compile_definitions)

#-----------------------------------------------------------------------------
# Compile Definitions Interface
#-----------------------------------------------------------------------------

add_library(flrp_compile_definitions INTERFACE)

target_compile_definitions(flrp_compile_definitions INTERFACE
  USE_FLRC_PROTOCOL
  NUMBER_OF_STACKS=1
)

zephyr_include_directories(
  ${FLRP_DIR}
  ${FLRP_DIR}/smtc_flrp_api
  ${FLRP_DIR}/smtc_flrp_core
  ${FLRP_DIR}/smtc_flrp_crypto
  ${FLRP_DIR}/smtc_flrp_crypto/soft_secure_element
  ${FLRP_DIR}/smtc_flrp_mac_layer/src
  ${FLRP_DIR}/smtc_flrp_wor/src
)

# FLRP adds *_fast AES/CMAC/soft_se for the FLRP stack; LoRaWAN still uses LBM
# software crypto when CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_SOFT=y.

# Create an alias with namespace (best practice)
add_library(flrp::definitions ALIAS flrp_compile_definitions)

#-----------------------------------------------------------------------------
# Common
#-----------------------------------------------------------------------------

zephyr_library_sources(
  ${FLRP_DIR}/smtc_flrp_core/smtc_flrp_core.c
  ${FLRP_DIR}/smtc_flrp_crypto/smtc_flrp_crypto.c
  ${FLRP_DIR}/smtc_flrp_crypto/soft_secure_element/aes_fast.c
  ${FLRP_DIR}/smtc_flrp_crypto/soft_secure_element/cmac_fast.c
  ${FLRP_DIR}/smtc_flrp_crypto/soft_secure_element/soft_se_fast.c
  ${FLRP_DIR}/smtc_flrp_mac_layer/src/smtc_flrp_mac_layer.c
  ${FLRP_DIR}/smtc_flrp_mac_layer/src/smtc_flrp_mac_serde.c
  ${FLRP_DIR}/smtc_flrp_mac_layer/src/smtc_flrp_mac_adaptive_link.c
  ${FLRP_DIR}/smtc_flrp_mac_layer/src/smtc_flrp_mac_config.c
  ${FLRP_DIR}/smtc_flrp_wor/src/smtc_flrp_wor.c
  ${FLRP_DIR}/smtc_flrp_wor/src/smtc_wor_rx.c
  ${FLRP_DIR}/smtc_flrp_wor/src/smtc_wor_tx.c
  ${FLRP_DIR}/smtc_flrp_wor/src/smtc_wor.c
  ${FLRP_DIR}/smtc_flrp_utils.c
)