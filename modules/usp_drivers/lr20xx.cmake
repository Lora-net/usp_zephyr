# Copyright (c) 2024 Semtech Corporation
# SPDX-License-Identifier: Apache-2.0

set(LBM_LR20XX_DIR ${SMTC_RADIO_DRIVERS_DIR}/lr20xx_driver)

#-----------------------------------------------------------------------------
# Radio specific sources
#-----------------------------------------------------------------------------

zephyr_library_sources(
  ${LBM_LR20XX_DIR}/src/lr20xx_driver_version.c
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_bpsk.c
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_common.c
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_fifo.c
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_flrc.c
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_fsk.c
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_lora.c
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_lr_fhss.c
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_ook.c
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_oqpsk_15_4.c
  ${LBM_LR20XX_DIR}/src/lr20xx_regmem.c
  ${LBM_LR20XX_DIR}/src/lr20xx_system.c
  ${LBM_LR20XX_DIR}/src/lr20xx_workarounds.c
  ${LBM_LR20XX_DIR}/src/lr20xx_rttof.c # protect with define ?
)

zephyr_library_sources_ifdef(CONFIG_SEMTECH_LR20XX_BLE
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_bluetooth_le.c
)
zephyr_library_sources_ifdef(CONFIG_SEMTECH_LR20XX_WI_SUN
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_wi_sun.c
)
zephyr_library_sources_ifdef(CONFIG_SEMTECH_LR20XX_WM_BUS
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_wm_bus.c
)
zephyr_library_sources_ifdef(CONFIG_SEMTECH_LR20XX_Z_WAVE
  ${LBM_LR20XX_DIR}/src/lr20xx_radio_z_wave.c
)

set(SMTC_RAL_SOURCES ${SMTC_RAL_DIR}/src/ral_lr20xx.c)
set(SMTC_RALF_SOURCES ${SMTC_RALF_DIR}/src/ralf_lr20xx.c)

# zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX
#   ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/lr11xx_crypto_engine/lr11xx_ce.c
# )
# zephyr_library_sources_ifdef(CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS
#   ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/lr11xx_crypto_engine/lr11xx_ce.c
# )

#-----------------------------------------------------------------------------
# Includes
#-----------------------------------------------------------------------------
zephyr_include_directories(${LBM_LR20XX_DIR}/inc)

# zephyr_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX
#   ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/lr11xx_crypto_engine
# )
# zephyr_include_directories_ifdef(CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS
#   ${LBM_SMTC_MODEM_CORE_DIR}/smtc_modem_crypto/lr11xx_crypto_engine
# )

#-----------------------------------------------------------------------------
# Radio specific compilation flags
#-----------------------------------------------------------------------------

zephyr_compile_definitions(
  LR20XX
)
