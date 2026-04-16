# USP for Zephyr changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v1.1.2] 2026-04-16

### Added

- LR2012 & LR2022 support
- New USP/RAC API
  - to get the current radio ID in post_callbacks (`smtc_rac_get_callback_radio_id()`)
  - to get the version of API (`smtc_rac_get_version()`)
  - to get signal rssi result in smtc_rac_data_result_t (`int16_t  signal_rssi_result;`)
- Specific 470MHz band overlays allowing PA config & frequency calibFE calibration at start with suffix `_cn` for LR2021, LR2022, LR2012

### Changed

- v2.0.2 LR20xx Drivers support (including patch ram level2)
- v3.0.0 LR11xx Drivers support (including security patches)
- geolocation example & Tools in a dedicated directory
- hw_modem updated with last features & fixes
- Optimized spi management in lr20_hal.c for STM32L476RG
- NRF RRAM WRITE BUFFER is deactivated in LBM based examples in order to prevent issues with nRF54L15 when saving context to FLASH

### Fixed

- FLRC BURST fixes
- Examples & associated documentation fixes (lctt_certif, porting tests, tx_cw, per_lora, per_fsk, spectral scan, rf_certification, cad, direct_driver_access, immediate_radio_access, multiprotocol, lrfhss, flrc_burst)
- STM32U5 overlay fixes
- CAD TO RX (CAD_LBT) fixed
- Comments in USP/RAC API

### Deprecated

- NA

## [v1.1.1] 2026-02-27

### Added

- Support for Zephyr 4.3
- Experimental FLRP Examples (flrc_burst) & Protocol API are still in progress.
- New API to allow Immediate Access to the radio : `smtc_rac_immediate_radio_access()` / `smtc_rac_release_immediate_radio_access()` with immediat_access example
- Geolocation Tools : full_almanac_update & wifi_region_detection examples ported from LoRa Basics Modem
- New HAL GPIO/LED management API

### Changed

- Update LR20XX radio drivers
- Radio planner performances improvement
- `symb_nb_timeout` type changed from `uint8_t` to `uint16_t` in RALF & USP/RAC
- Documentation updates (FLRC, LINUX, README)
- Use of `CONFIG_NEWLIB_LIBC_NANO` to improve memory footprint


### Fixed

- Any application can now use LOG_MODULE_REGISTER(xxx) with xxx being a specific user tag. The default LOG_MODULE_REGISTER(usp) is now automatically called.
- HF RX PATH starts from 1.5 GHz instead of 1.6 GHz

### Deprecated

- NA

## [v1.0.0] 2025-12-15

This version is based on branch v0.5.1-alpha of USP for Zephyr.

### Added

- CAD example
- VSCode Integration helper
- New RAC API : smtc_rac_lock_radio_access()/smtc_rac_unlock_radio_access()
- Support of STM32U575ZIQ as buildable
- Support of xiao ESP32S3 as experimental
- LR2022 Support

### Changed

- Upgrade of LBM to v4.9
- Documentation (getting started, API, LBM porting guide, MCU & Radio porting guide, samples)
- geolocation example fixed
- hw_modem example fixed
- lctt certif : default configuration updated
- SPI Speed of Semtech Radios set to 16MHz
- Wio LR2021 shield renamed
- Support of semtech_loraplus_expansion_board  for other MCU than xiao
- rf_certification example temporary removed
- Radio drivers updates:
  - LR20XX to version [v1.3.4](smtc_rac_lib/radio_drivers/lr20xx_driver/README.md)

### Fixed

- Workaround for bad standard deviation of RTToF results with fractional bandwidths
- PER FLRC crashes
- FUOTA storage in flash fix
- Removed not required include path from Examples CMakeLists.txt
- Rework of cmake management in order to propagate LBM cmake symbols to LBM applications
- periodical_uplink documentation update for Regions
- Fixes on all samples
- Issue [#2](https://github.com/Lora-net/usp_zephyr/issues/2) modem_set_radio_ctx() was not called (required for wifi & gnss) + LR1121 support
- Issue [#1](https://github.com/Lora-net/usp_zephyr/issues/1) Incorrect pin definition in documentation

### Deprecated

- NA

## [v0.5.1] - 2025-10-15

### Added

- Initial version
