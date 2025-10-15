# Known limitations that will be addressed in master release

This document presents USP-Zephyr current known limitations and their workarounds, when available.

### 1. Xiao-nRF54L15 user UART RX not working as expected
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

### 2. Nucleo-L476RG default clock drift
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
- The second workaround is applied for demonstration to the `lctt_certif` & `hw_modem` samples

### 3. CAD Modulation Service not yet implemented in USP

CAD is working as usual with LoRaWAN / LBM 4.9.0. \
But the CAD service through the USP / RAC API is not yet available.

### 4. FUOTA Restriction

At the end of the FUOTA process, the integrity check (MIC) of the downloaded file fails due to a malfunction in the ZEPHYR Flash HAL layer.

### 5. Standard deviation of RTToF results with fractional bandwidths

Executing RTToF operations on bandwidths 812kHz, 406kHz, 203kHz and 101kHz (a.k.a. *fractional bandwidths*) exposes unexpectedly high standard deviation of the RTToF results.
The function `lr20xx_workarounds_rttof_results_deviation()` provides a workaround that is currently not called by USP. If called, it can reduce the standard deviation on RTToF results.
Please read the `lr20xx_driver/README.md` for more information on this limitation.

### 6. Baremetal STM32L476RG USP version is to be considered experimental

The current release focus on USP Zephyr on
- Xiao-nRF54L15
- LR2021-Wio + LoRa Plus Expansion Board
- Zephyr 4.2.0

The bare-metal STM32L476RG implementation is included but has not been fully validated and might exhibit instabilities.

### 7. Optimization

Memory footprint (RAM/NVM), performances, and low power are not yet fully optimized.
For example :
- RAM and flash usage have not been minimized
- Direct Memory Access may not be enabled for all peripherals on every MCU
- Low-power modes are not fully optimized for all MCU targets
- Additional Kconfig options are needed to enable fine-grained feature selection
