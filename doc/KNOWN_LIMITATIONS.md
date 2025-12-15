# Known limitations that will be addressed in master release

This document presents USP-Zephyr current known limitations and their workarounds, when available.

### Xiao-nRF54L15 user UART RX not working as expected
There is currently a bug in the nRF54L15 IC that prevents some peripherals to work as expected when their pins are connected as "cross power-domains" (= using GPIO P2 port, see [Nordic website](https://docs.nordicsemi.com/bundle/ps_nrf54L15/page/chapters/pin.html) for additional details). In the Xiao-nRF54L15 board, the user UART pins are routed on GPIO pins P2.07 (RX) and P2.08 (TX).

**Issue description:**
- No data is received on RX pin, or UART RX interrupt does not trigger.

**Issue conditions:**
- The application has currently no debugger attached/debug session started.

**Issue workaround:**
- Add `#include <nrfx_power.h>` to your application code.
- Before starting UART character reception, call `nrfx_power_constlat_mode_request()`.
- After completing UART character reception, call `nrfx_power_constlat_mode_free()`.

**Issue drawbacks:**
- The power consumption is slightly increased when CONSTLAT mode is enabled.

**Comments:**
- When a debugger is attached, the CONSTLAT mode is enabled, hence the issue isn't present.
- The bug is present in chip revisions QFAAB0 and QFAACO. More versions may be affected.

### Nucleo-L476RG default clock drift
**Issue description:**
- On Nucleo-L476RG board, Zephyr clock source is driven by internal RC, leading to poor precision, and some drift/window misalignment when using LoRaWAN Class B.

**Issue conditions:**
- The default clock driver is used.

**Issue workaround:**
- Increase LBM `crystal-error` parameter (leading to higher average current consumption in Receive mode),
- **or** Add
```dts
&lptim1 {
	clocks = <&rcc STM32_CLOCK_BUS_APB1 0x80000000>,
		 <&rcc STM32_SRC_LSE LPTIM1_SEL(3)>;
	status = "okay";
};
```
to the sample's `boards/nucleo_l476rg.overlay`, *and* add
```kconfig
CONFIG_CORTEX_M_SYSTICK=n
CONFIG_STM32_LPTIM_TIMER=y
CONFIG_CLOCK_CONTROL=y
CONFIG_PM=y
```
to the sample's `prj.conf`.

**Issue drawbacks:**
- The power consumption might increase.

**Comments:**
- The second workaround is applied for demonstration to the `lctt_certif`, `hw_modem`, `packet_error_rate_lora`, and `packet_error_rate_fsk` samples (have a look on `samples/usp/lbm/lctt_certif/boards/nucleo_l476rg.conf` & `samples/usp/lbm/lctt_certif/boards/nucleo_l476rg.overlay`.
- The clock drift was not evaluated on nucleo U5. Consider applying such a workaround if the same issue is detected.

### hw_modem integration (#131)

hw_modem is an application embedding most of the USP platform on the tested MCU. This MCU can then be controlled by UART to test USP & LoRa Basics Modem API.
However, `modem-bridge`, a bridge application between the hw_modem MCU and the controlling computer, is not provided.
hw_modem documentation will be completed in future releases.

### hw_modem: STORE & FORWARD integration (#119)

Store & Forward service is not functional in hw_modem. The defines used in cmd_parser.c are not activated.
Store & Forward service is functional in geolocation example.

### Some programmed packets could be dropped (seen in Relay RX) (#130)

During validation, it was discovered that some packets could be dropped under certain circumstances. When this occurs, the following message may appear:
> task schedule aborted because in the past -1

This issue was observed during validation of Relay RX with STM32L476RG & Wio-LR2021, but may also occur occasionally with other features and radios.
If this issue occurs, try extending the `RP_MARGIN_DELAY` value from `8` to `12` in the following file: `smtc_rac_lib/radio_planner/src/radio_planner_types.h`.

### Geolocation Tools are missing (#129)

The geolocation application from Legacy LoRa Basics Modem 3_geolocation_on_lora_edge Application suite was ported to USP.
Nevertheless, the following tools are not yet available for USP:
- full_almanac_update
- lr11xx_flasher
- wifi_region_detection

If required, they can be retrieved from [Legacy LBM](https://github.com/Lora-net/SWL2001/tree/master/lbm_applications/3_geolocation_on_lora_edge).

### USP API: `smtc_rac_submit_radio_transaction()` with out-of-range frequency is accepted (#98)

When requested frequency is out of range, no error is returned and the software seems to run normally, but the requested frequency is not used. Instead, the previously set frequency is used.
In future releases, an error will be returned or the firmware will reset with panic for out-of-range frequency.

### USP API: `smtc_rac_submit_radio_transaction()` with LR20xx & BW 7, 10, 15, 20 causes Division by zero (#94)

When using the LR20xx radio with LoRa modulation and BW 7, 10, 15, 20, the software crashes with a Division by zero exception.
In future releases, an error will be returned or the firmware will reset with panic for out-of-range BW.

### USP API: Max value for `symb_nb_timeout` is limited to `uint8_t` (#102)

In RAC API, `smtc_rac_radio_lora_params_t`/`symb_nb_timeout` is stored in a `uint8_t` type. This restricts the max length for a LoRa preamble.
LoRaWAN Relay TX/RX are not affected by this issue.

### The `rf_certification` is not available

The  `rf_certification`sample is temporary not available. Please contact the Semtech support for more details.

### The RAL_LORA_CAD_LBT CAD mode of `cad`example is not functional (#125)

The `cad`example exposes 3 modes (TYPE_OF_CAD) :
- RAL_LORA_CAD_LBT  // perform CAD and then TX
- RAL_LORA_CAD_RX   // perform CAD and then RX
- RAL_LORA_CAD_ONLY // perform CAD and stop the radio

The `RAL_LORA_CAD_LBT` mode is currently not functional (both for USP Zephyr & USP baremetal).

### Logging & new applications (#28)

In the current version, the application using the USP for Zephyr module is forced to use:

``` c
LOG_MODULE_REGISTER( usp, LOG_LEVEL_INF );
```

If this is an issue for you, copy/paste this line in `subsys/usp/smtc_sw_platform_helper.c`.
Then you will be able to use the logging configuration suitable to your application :

``` c
LOG_MODULE_REGISTER( my_application, LOG_LEVEL_INF );
```

### Optimization

Memory footprint (RAM/NVM), performances, and low power are not yet fully optimized.
For example :
- RAM and flash usage have not been minimized
- Direct Memory Access may not be enabled for all peripherals on every MCU
- Low-power modes are not fully optimized for all MCU targets
- Additional Kconfig options are needed to enable fine-grained feature selection
