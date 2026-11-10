# USP for Zephyr changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v1.3.0] 2026-11-09

### Added

- New sample `samples/usp/sdk/fhss_exemple`: LoRa intra-packet frequency hopping PING/PONG between two LR20XX radios
- LR20XX RSSI calibration gain tables configured from the devicetree
  - New required properties `rssi-calibration-lf-table` (RX path below 1.5GHz) & `rssi-calibration-hf-table` (RX path above 1.5GHz) in `dts/bindings/usp/semtech,lr20xx-common.yaml` (27 `<gain_value noise_figure>` cells)
  - `ral_lr20xx_bsp_get_rssi_calibration_table()` implemented in `drivers/usp/lr20xx/lr20xx_ral_bsp_calibration.c`
  - Reference tables added for the WIO LR20XX shields (`semtech_wio_lr20xx_region_eu_us.dtsi`: 868/915MHz, `semtech_wio_lr20xx_region_cn.dtsi`: 490MHz) and the LR2021MB1xxS shield
  - Documentation in the [USP Zephyr porting guide](doc/usp_zephyr_porting_guide.md) (§2.3)
- hw_modem
  - Single-UART transport without external bridge (`src/unified_uart.c`, `include/zephyr/usp/unified_uart.h`): modem data & log traces are multiplexed on the same UART with a framed protocol, through a dedicated log backend (`log_backend_unified_uart`). Host side: `python_bridge.py` tool provided by USP
  - The host UART is automatically the board console UART (chosen `zephyr,console`, i.e. the USB / VCP link of the board); the `smtc-hal-uart` alias can still be defined in an overlay to force another UART
  - New sample `Kconfig`: `USE_FLRP` option (FLRP commands, requires a LR20XX radio) and log configuration required by the single-UART transport
  - NUCLEO-U575ZI-Q support (overlay & conf, LPTIM1 on LSE used as system timer like on NUCLEO-L476RG)
  - Commands ported from modem-e (USP): `CMD_WIFI_SCAN_MODE`, `CMD_GNSS_ALM_DEMOD_STOP`, `CMD_WIFI_GET_SCAN_DONE_SCAN_DATA_RESULT`, `CMD_GET/SET_NO_RX_PACKET_THRESHOLD`, `CMD_GET/SET_CHANNEL_MASK`
- FLRP (from USP)
  - TX statistics given to the TX done callback (`smtc_flrp_tx_stats_t`: frequency, frequency offset, payload & burst bit rates)
  - Frequency, frequency offset, payload & burst bit rates in RX burst statistics (printed by the `flrp_api_lorawan` sample)
  - `smtc_flrp_is_transaction_active()` API
  - `frequency_offset_enabled` advanced FLRC parameter to disable the frequency offset application
- LBM (from USP) : see below
- `CONFIG_USP_MODEM_HAL_DBG_TRACE` Kconfig option: when disabled, the LBM, RAC and radio planner traces (`SMTC_MODEM_HAL_TRACE_*`) are removed from the build
- `CONFIG_USP_HAL_DBG_TRACE` Kconfig option: when disabled, the samples and helpers traces (`SMTC_HAL_TRACE_*`) are removed from the build

### Changed

- Zephyr updated from v4.4.0 to v4.4.2
- USP updated from v1.2.1 to v1.3.0, see USP changelog
- Zephyr module CMake adapted to the new USP architecture (radio drivers, RAL & RALF from `smtc_radio_interface`, `smtc_core_hal_interface`, LBM decoupled from the modem HAL & storage)
- FLRP (from USP)
  - `smtc_flrp_tx_done_f` TX callback has a new `smtc_flrp_tx_stats_t flrp_stats` parameter: `flrp_api` & `flrp_api_lorawan` samples updated
  - `smtc_flrp_initiate_transmission()` / `smtc_flrp_initiate_reception()` return `SMTC_FLRP_RC_BUSY` if a transaction is already in progress
  - Sub-GHz defines renamed (`WOR_SUBGIG_RF_FREQ_IN_HZ`, `DATA_FLRC_SUBGIG_RF_FREQ_IN_HZ`, `DATA_FLRC_CHANNEL_STEP_SUBGIG_HZ`) and default sub-GHz frequency moved from 865.1MHz to 865.7MHz
- LBM (from USP) : see below
- Radio drivers & RAL (from USP)
  - LR20xx driver upgraded from v2.0.2 to v2.1.0 (FLRC FEI, BT 0.3 & double rate sampling workaround, CCA with gain, LR-FHSS hopping table, ...)
  - smtc_ral: LR20XX RSSI calibration applied at RAL init, LR2021 PRAM version checked at init, new LoRa LDRO computation for all radios, missing LR20XX IRQs mapped
- SX126X devicetree: `tcxo-voltage` & `tcxo-wakeup-time` are now required (same approach as LR11XX), `tcxo-wakeup-time = <0>` selects the XTAL (default wake-up time is now 0 instead of 8ms); `semtech_sx126xmbxxxs` shields updated accordingly
- WIO LR20XX PA tables (`semtech_wio_lr20xx_region_eu_us.dtsi` & `semtech_wio_lr20xx_region_cn.dtsi`) regenerated from the smtc-shields reference tables
- hw_modem
  - Bridge GPIOs (`hw-modem-command-gpios`, `hw-modem-busy-gpios`, `hw-modem-event-gpios`) removed from all board overlays
  - Host UART: `smtc-hal-uart` alias removed on NUCLEO & XIAO boards, the console UART is used: NUCLEO-L476RG (all variants) `usart2` (ST-Link VCP) instead of `usart3`, NUCLEO-U575ZI-Q `usart1` (ST-Link VCP), XIAO nRF54L15 (all variants) `uart20` (USB serial) with `uart21` disabled. nRF52840-DK (`uart1`) & nRF54L15-DK (`uart22`) keep an explicit alias
  - Log configuration forced by the sample Kconfig (`LOG_BACKEND_UART=n`, `UART_CONSOLE=n`, `LOG_OUTPUT=y`, `LOG_PRINTK=y`) and `CONFIG_LOG_MODE_IMMEDIATE=y` in `prj.conf`
  - `python_test` & `serialization` directories (and the nanopb submodule) removed: the USP ones are used
  - `CMD_FLRP_START_PERIODIC_LISTENING` & `CMD_FLRP_INITIATE_RECEPTION` now take the expected payload size (USP)
- multiprotocol: periodical uplink delay set to 60s (instead of 600s)
- LR20XX devicetree: the RSSI calibration `offset`/`tune` properties are replaced by the RSSI calibration gain tables
- Display (Zephyr 4.4): SSD1306 compatible `solomon,ssd1306fb` replaced by `solomon,ssd1306` in the `semtech_loraplus_expansion_board` overlay
- HAL API (USP new architecture): `smtc_modem_hal.h` split into `smtc_core_hal.h` (core HAL, always used) and the LBM specific `smtc_modem_hal.h`, `smtc_modem_hal_dbg_trace.h` renamed `smtc_core_hal_trace.h`. `smtc_modem_hal_interruptible_msleep()` now takes a `uint32_t` timeout in ms (instead of a `k_timeout_t`) and, like `smtc_modem_hal_wake_up()`, is declared in `smtc_core_hal.h` (no more in `lorawan_hal_init.h`): applications must include `smtc_core_hal.h` and pass milliseconds

### Fixed

- LR2021MB1xxS shield overlay: missing RSSI calibration table (build error)
- SSD1306 build warning
- TCXO: XTAL / TCXO selection from `tcxo-wakeup-time` for LR20XX & LR11XX, LR20XX TCXO startup time
- LR20XX HAL: direct FIFO read was sending the RX buffer content on MOSI (0 is now sent)
- Storage: flash padding for store & forward (`smtc_modem_hal_storage.c`)
- Low power (`prj_lowpower.conf`, NUCLEO-L476RG): `CONFIG_RESET` re-enabled for the LPTIM when the debug UART is disabled
- CMake: include directory for `CONFIG_LORA_BASICS_MODEM_BEACON_TX`
- lctt_certif: `SMTC_MODEM_EVENT_RESET_REQUEST` (reset requested by the certification) managed
- FLRP (from USP): channel index boundary & default channel checks, frequencies `memcpy()` size, busy guards & IDLE transitions, division by zero & bit rate overflow, FLRC burst TX IRQ clear
- LBM (from USP): see below
- hw_modem: circular buffer issue, missing return checks (Coverity)

### Bugfixing

- Unused I2C / SSD1306 configuration removed from `direct_driver_access`, `immediate_radio_access` & `ranging_demo` `prj.conf`
- Documentation: no more cross links between USP & USP for Zephyr

### Deprecated

- hw_modem FLRC protocol commands 0xB0 to 0xB2 removed (USP)
- `CONFIG_USP_LOG_VERBOSE` Kconfig option removed (unused)

### LoRa Basics Modem v4.9.3 (from [USP](https://github.com/Lora-net/usp))

USP v1.3.0 embeds LoRa Basics Modem v4.9.3 (`protocols/lbm_lib`).

`protocols/lbm_lib` is common to USP and SWL2001, its detailed changelogs are:
- [Modem API changelog](protocols/lbm_lib/smtc_modem_api/CHANGELOG.md)
- [Modem HAL changelog](protocols/lbm_lib/smtc_modem_hal/CHANGELOG.md)

#### Added

- Regional Parameters RP002-1.0.4 and RP002-1.0.5 support (CFList type 1 for dynamic plans, up to 80 channels on EU868, new US915 channel plan, Relay channel 1 SF update, AS923 group 4), selected with the CMake option `RP_VERSION` (`RP2_101`, `RP2_103`, `RP2_104`, `RP2_105`; default remains `RP2_103`)
- LR2022 added to the list of WW_2G4-compatible radios
- CMake: `LBM_CMAKE_CONFIG_AUTO` option
- Relay Tx: `number_of_miss_wor_ack_to_switch_in_nosync_mode` can now be configured by the user

#### Changed

- The radio planner and RALF are provided externally to LBM (RAC dependency removed), the LBM HAL is split in the LBM specific `smtc_modem_hal.h` and the core HAL `smtc_core_hal.h` (see the Modem HAL changelog), simplified CMake architecture, LBM specific compilation definitions are now PUBLIC
- Class B beacon DPLL: `BEACON_PLL_PHASE_GAIN_MUL/DIV` set to 1/2 for a faster convergence and accept a bigger crystal error
- Multicast Class B/C: the session now starts 2 s before its session time
- Duty-cycle frequency list now takes the 80 channels into account

#### Fixed

- NbTrans > 1: the duty-cycle time off is now updated on retransmission, all retransmissions are sent before the duty-cycle delay is enforced
- Wrong index boundary check when decrementing the datarate (AU915 / LR-FHSS)
- `lr1_stack_mac_cmd_ans_cut()` could block the modem because DeviceTimeAns and Class B MAC commands were missing in the array used for length check
- Class B/C Multicast v1/v2 ping slot hopping when frequency is 0
- Modem was not able to retain sleep when `lorawan_dwn_ack_management` variable was less than or equal to 96 bytes (context added)
- Missing check of `SMTC_MULTICAST` definition (Class B or C only)
- `smtc_modem_get_suspend_radio_communications()` can no longer be called in test mode
- Porting guide: `CONTEXT_SECURE_ELEMENT` needs 484 bytes
- Issue [#141](https://github.com/Lora-net/SWL2001/issues/141) Missing semicolons when `SMTC_TRACE` is compiled
- IN865, AU915 and AS923_GRP4 region typos/out-of-bound fixes
- MAC commands answer size table: `LINK_CHECK_ANS` replaced by `LINK_CHECK_REQ` (command sent by the end-device)
- SX127x (SX1272/SX1276) build and management

#### Security

Robustness fixes found by static analysis and code audit (out-of-bound access, unvalidated lengths and indexes):

- FOpts extraction read past the valid payload before MIC verification
- FUOTA: fragment payload size validated against the received RF payload; `FragDecoderProcess()` rejects `fragCounter == 0` (row index underflow); `frag_index` bounds checked before use; `frag_req_cmd_size` table initialisation; unicast DataFragment refused on multicast-only sessions; `fmp_test()` duplicated declaration
- Remote Multicast Setup: `MC_GROUP_STATUS_REQ` answer buffer bound check
- Multi Package Access: `MULTI_PACK_BUFFER_REQ` read past the payload
- Certification: `TX_PERIODICITY_CHANGE_REQ` and `EchoPlayReq` size validation
- `link_adr_parser()` size check performed before dereference
- Function pointers are checked (`SMTC_LBM_IS_VALID_FN_PTR`) before being called; `smtc_lbt_get_obj()` panics on out-of-bound instead of returning NULL
- Coverity findings: uninitialised variables, integer overflow, `MIN/MAX` macros brackets, `CRYPTO_MAXMESSAGE_SIZE` limited to 255 in the LR11xx crypto engine

## [v1.2.1] 2026-06-26

### Added

- Zephyr 4.4 support (validated with SDK 1.0.1)
- FLRP protocol & examples (for more details cf the example README.md & FLRP API / FLRP API LORAWAN README.md)
- More parameter checking in LoRa modulation of USP/RAC submit API
- SX126X support : PA tables, TCXO, cmake files
- hw_modem updated with FLRP feature
- Porting Tests SPI integrity tests (SPI endurance)
- LoRa Plus LR20XX EVK Support : New SPI speed configured as follow :
  - STM32L4 :
    - FLRP examples/applications : 10MHz
    - Other examples : 5MHz
  - xiao nRF54L15 : 8MHz
  - nRF54L15-DK : 4MHz
  - Other MCU : 8MHz maximum

### Changed

- LR20XX Drivers updated to 2.0.2
- Default ALC SYNC & FUOTA versions set from 1 to 2
- Documentation updates (FLRC, README)
- LoRa Plus LR20XX EVK PA Table updated (all regions)
- Factorized common examples functions
- With Zephyr 4.4, FLASH management was updated in modules/smtc_modem_hal/smtc_modem_hal_storage.c (kvss/nvs.h support + nRF54L15 fixes)
- LIBC is not mode NEWLIB but the Zephyr default LIBC
- All STM32L4 examples are now configured to make zephyr use the LPTimer as internal clock
- semtech_sx1261mb2bas.overlay shield renamed to semtech_sx1261mb2bas_usp.overlay

### Fixed

- Examples & associated documentation fixes (porting tests, full almanac update, tx_cw, geolocation, hw_modem, cad, direct_driver_access, per_flrc, per_fsk, per_lora, ranging_demo, rf_certification)
- Add calibrate after configuring tcxo on LR20XX
- Porting Tests is now OK with LR2021 & STM32L4 & xiao_nRF54L15
- Issues with SX126X support
- LRFHSS/AU : Fix index wrongly calculated when decrementing DR
- LBM : compilation fixes related to lr11xx crypto
- RTToF : configure the responder PA / TX power (auto-transmitted response was sent at default power, leading to a weak signal / RSSI at the initiator)

### Deprecated

- NA

## [v1.1.2] 2026-04-16

### Added

- LR2012 & LR2022 support
- New USP/RAC API
  - to get the current radio ID in post_callbacks (`smtc_rac_get_callback_radio_id()`)
  - to get the version of API
  - to get signal rssi result in smtc_rac_data_result_t
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
