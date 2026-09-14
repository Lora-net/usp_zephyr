# Known Limitations

This document presents USP-Zephyr current known limitations and their workarounds, when available.

### This USP-Zephyr version includes the first version of FLRP

Known Limitations specific to FLRP are gathered in the [FLRP documentation](FLRP_guidelines.md#limitations).

### Not validated in this release

The following are **not validated / not fully tested** in this release:
- **geolocation**
- **rf_certification**

### Xiao-nRF54L15 & STM32-U5 are not more managed with Zephyr < 4.3

The current software is prepared to be used with Zephyr 4.4.
Zephyr 4.2 is no more directly supported for the Xiao-nRF54L15 & STM32-U5 platforms.
Zephyr 3.7 is no more directly supported for the STM32-U5 platform.

To be able to use the current software with Xiao-nRF54L15 & Zephyr 4.2, the `usp_zephyr/board` directory of USP_ZEPHYR 1.0.0 shall be back-ported to the current software.

To be able to use the current software with STM32-U5 & Zephyr 4.2 or 3.7, the `nucleo_u575zi_q.overlay` file from USP_ZEPHYR 1.0.0 shall be back-ported to the current software : To be added to `nucleo_u575zi_q.overlay` when using Zephyr 4.2 or 3.7 :
```
// Add user storage partition (not present in default nucleo_u575zi_q dts, as opposed to other boards)
&flash0 {
	partitions {
		compatible = "fixed-partitions";
		#address-cells = < 0x1 >;
		#size-cells = < 0x1 >;

		storage_partition: partition@1f8000 {
			label = "storage";
			reg = < 0x1f8000 0x8000 >;
		};
	};
};
```

### rf_certification example & FCC duty-cyle limit

FCC test application currently may not reach the required 98% channel duty-cycle limit. A fix is in progress.

### porting_tests application limitation

On xiao_nrf54l15, 3 tests are not passing :
- With LR2021 : `porting_test_get_time/Get time in millisecond` : `NOK: Time is not coherent with radio irq : expected 1966ms / get 1968ms (margin +/-1ms)`
  - This issue is under investigation, but currently, It did not prevent to pass OK through the Semtech full Validation Process
- `porting_test_config_rx_radio` : `Configuration of rx radio is too long: 9ms (margin +8ms)` :
  - This issue is under investigation, but currently, It did not prevent to pass OK through the Semtech full Validation Process
- `porting_test_config_tx_radio` : `Configuration of rx radio is too long: 10ms (margin +8ms)` :
  - This issue is under investigation, but currently, It did not prevent to pass OK through the Semtech full Validation Process

On `nucleo_l476rg`, `porting_test_get_time/Get time in millisecond` also fails with the **SX126x (SX1261)** and **LR11xx (LR1120)** shields: the measured time is not coherent with the radio IRQ (error is largest on SX1261). Under investigation (issue 267).

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
to the sample's `boards/nucleo_l476rg.conf`.

**Issue drawbacks:**
- The power consumption might increase.

**Comments:**
- The second workaround is applied for demonstration to the `lctt_certif`, `hw_modem`, `packet_error_rate_lora`, and `packet_error_rate_fsk` samples (have a look on `samples/usp/lbm/lctt_certif/boards/nucleo_l476rg.conf` & `samples/usp/lbm/lctt_certif/boards/nucleo_l476rg.overlay`.

- The clock drift was not evaluated on nucleo U5. Consider applying such a workaround if the same issue is detected. But CONFIG_PM=y in prj.conf causes deep STOP entry when idle, which breaks UART and radio (SPI/DIO) in USP samples with U5. Workaround: use CONFIG_PM=n, or add a U5 board conf with same settings as nucleo_l476rg.conf and re-enable lptime within nucleo_u575zi_q.overlay.

### hw_modem integration (#131)

hw_modem is an application embedding most of the USP platform on the tested MCU. This MCU can then be controlled by UART to test USP & LoRa Basics Modem API.
However, `modem-bridge`, a bridge application between the hw_modem MCU and the controlling computer, is not provided.
hw_modem documentation will be completed in future releases.

### US915 TX power index offset via LinkAdrRequest on LR11xx/SX126x (#300)

Network servers can modify the transmit power of devices with the LinkAdrRequest command.
On US915, with LR11xx and SX126x radios, we observed a power offset: the first two power indices produce the same transmit power, which creates an offset throughout the test for all indices

### SX126x RSSI_READ shows dBm offset (#299)

Using the RSSI_READ test command on the SX126X radios, we observe an offset negative or positive depending on usp or usp zephyr.

### SX126x on usp receives LoRaWAN at ~20 dBm too low

On USP, the SX126x radios receive LoRaWAN messages at an RSSI roughly 20 dBm lower than they should. Not seen on usp zephyr.

### Some programmed packets could be dropped (seen in Relay RX) (#130)

During validation, it was discovered that some packets could be dropped under certain circumstances. When this occurs, the following message may appear:
> task schedule aborted because in the past -1

This issue was observed during validation of Relay RX with STM32L476RG & Wio-LR2021, but may also occur occasionally with other features and radios.
If this issue occurs, try extending the `RP_MARGIN_DELAY` value from `8` to `12` in the following file: `smtc_rac_lib/radio_planner/src/radio_planner_types.h`.

### Geolocation tools

The geolocation application from Legacy LoRa Basics Modem 3_geolocation_on_lora_edge Application suite was ported to USP & USP Zephyr.
Nevertheless, the following tools are not yet available for USP:
- lr11xx_flasher

If required, It can be retrieved from [LR11xx Updater tool](https://github.com/Lora-net/SWTL001).

Note : Geolocation tools (geolocation,  full_almanac_update and wifi_region_detection) cannot be built with CONFIG_USP_MAIN_THREAD=y

### USP API: `smtc_rac_submit_radio_transaction()` with out-of-range frequency is accepted (#98)

When requested frequency is out of range, no error is returned and the software seems to run normally, but the requested frequency is not used. Instead, the previously set frequency is used.
In future releases, an error will be returned or the firmware will reset with panic for out-of-range frequency.

### USP API: `smtc_rac_submit_radio_transaction()` with LR20xx & BW 7, 10, 15, 20 causes Division by zero (#94)

When using the LR20xx radio with LoRa modulation and BW 7, 10, 15, 20, the software crashes with a Division by zero exception.
In future releases, an error will be returned or the firmware will reset with panic for out-of-range BW.

### Optimization

Memory footprint (RAM/NVM), performances, and low power are not yet fully optimized.
For example :
- RAM and flash usage are still being minimized
- Direct Memory Access may not be enabled for all peripherals on every MCU
- Low-power modes are not fully optimized for all MCU targets
- Additional Kconfig options are needed to enable fine-grained feature selection

### LBM/FUOTA non-functional (multicast issue)

FUOTA is currently non-functional due to an issue with multicast. A fix is planned for the next release.

### LBM instabilities detected in certain regions

Instabilities have been identified in the following regions: AS923_GRP1, AS923_GRP4, KR920, and AU915.
This issue may cause periodic panic reboots. A fix is planned for the next release.

### LBM/Relay WOR window misaligned (US 915 MHz)

In US 915 MHz band, the Relay feature is not operational when using different/mismatched boards (e.g. Nucleo-L476RG and Xiao-nRF54L15), due to WOR window misalignment. We recommend using the same MCU/board on both sides.

### SX1262 & SX1268 support issue

The SX1262 & SX1268 radios are currently not well supported.
Please add `-- -DEXTRA_CFLAGS="SX1262"` or `-- -DEXTRA_CFLAGS="SX1268"` in the `west build` command.
A fix is planned for the next release.
