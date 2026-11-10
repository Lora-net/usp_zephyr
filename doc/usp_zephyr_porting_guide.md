# ZEPHYR APPLICATION PORTING GUIDE

## Introduction

This guide explains how to port Zephyr applications to new MCUs and radios for **usp_zephyr**. For general setup, build instructions, and supported hardware, refer to the [main README](https://github.com/Lora-net/usp_zephyr#readme).




<details>
<summary><b>MCU Porting</b></summary>

### 1.1 Hardware Requirements

**MCU Memory Requirements:**

- Varies significantly by application and enabled features
- **Reference**: `periodical_uplink` sample uses ~257KB Flash + ~39KB RAM
- Check memory usage after building your specific application

**MCU Required Peripherals:**

- **SPI**: For radio communication (frequency depends on radio - check datasheet)
- **Timer**: Sufficient resolution for application timing requirements
- **GPIOs**:
  - Interrupt-capable GPIO for radio events
  - Control pins as required by radio (RESET, BUSY, DIO, etc.)
- **Non-volatile Storage**: If application requires context persistence
- **Entropy Source**: If application requires random numbers (hardware or software)

**Optional but Recommended:**

- UART (for logging/debugging)
- I2C (for sensors, displays)
- Watchdog timer
- RTC (for low-power timing)

**Notes:**
- Specific peripheral requirements depend on your application and radio
- Check [Zephyr Supported Boards](https://docs.zephyrproject.org/latest/boards/index.html) for compatible MCU architectures
- Refer to radio datasheets for radio-specific requirements (SPI speed, TCXO, timing, etc.)

### 1.2 Board Files Structure

Create in `boards/<vendor>/<board>/`:

```
<board>/
├── <board>_<soc>_<core>.dts     # Main board DTS
├── <board>_common.dtsi          # Common definitions (LEDs, buttons, peripherals)
├── <board>-pinctrl.dtsi         # Pin multiplexing
├── <connector>.dtsi             # Connector definitions (optional)
├── Kconfig.<board>              # Board Kconfig
├── Kconfig.defconfig            # Default enables
├── board.cmake                  # Flash runner config
├── board.yml                    # Metadata
└── support/                     # Optional: debug configs, udev rules
```

**File Organization Pattern** (from xiao_nrf54l15 example):
- **Main DTS**: Includes common files, defines memory partitions, chosen nodes
- **Common DTSI**: Shared peripheral configs (UART, SPI, I2C, LEDs, buttons, ADC)
- **Pinctrl DTSI**: Pin multiplexing for all peripherals
- **Connector DTSI**: Standard connector pin mappings (Arduino, Wio, etc.)

**Reference Example:**
See `boards/seeed/xiao_nrf54l15/` for a complete custom board implementation:
- Main DTS: `xiao_nrf54l15_nrf54l15_cpuapp.dts`
- Common: `xiao_nrf54l15_common.dtsi` (peripherals, LEDs, buttons, ADC)
- Pinctrl: `xiao_nrf54l15-pinctrl.dtsi`
- Connector: `seeed_xiao_connector.dtsi`

### 1.3 Essential Devicetree Elements

**Memory Partitions:**

Define flash partitions for your application (boot, image slots, storage).

**Reference**: See `boards/seeed/xiao_nrf54l15/xiao_nrf54l15_nrf54l15_cpuapp.dts` for partition examples

**Peripheral Configuration:**

Enable required peripherals, for example:

```dts
&uart20 { status = "okay"; };
&spi00 { status = "okay"; };
&gpio0 { status = "okay"; };
```

**Chosen Nodes:**

Define system defaults (console, flash, SRAM):

```dts
chosen {
    zephyr,code-partition = &slot0_partition;
    zephyr,console = &uart20;
};
```

**Reference Files:**
- Complete board: `boards/seeed/xiao_nrf54l15/xiao_nrf54l15_nrf54l15_cpuapp.dts`
- Common definitions: `boards/seeed/xiao_nrf54l15/xiao_nrf54l15_common.dtsi`
- Connector: `boards/seeed/xiao_nrf54l15/seeed_xiao_connector.dtsi`
- Shield overlays: `boards/shields/semtech_wio_lr20xx/semtech_wio_lr2021.overlay`

### 1.3 Kconfig Configuration

**`Kconfig.<board>`** - Board selection (example from `Kconfig.xiao_nrf54l15`):

```kconfig
config BOARD_YOUR_BOARD
    select SOC_<YOUR_SOC>_<CORE> if BOARD_YOUR_BOARD_<SOC>_<CORE>
```

**`Kconfig.defconfig`** - Board defaults (example from `xiao_nrf54l15`):

```kconfig
if BOARD_YOUR_BOARD_<SOC>_<CORE>

# Enable required peripherals
config SPI
    default y

config GPIO
    default y

config SERIAL
    default y if CONSOLE

# Board-specific settings
config ROM_START_OFFSET
    default 0x800 if BOOTLOADER_MCUBOOT

endif # BOARD_YOUR_BOARD_<SOC>_<CORE>
```

**Application `prj.conf`** - Application-specific configuration:

```ini
# Enable required features for your application
CONFIG_GPIO=y
CONFIG_SPI=y
CONFIG_LOG=y
# Add your application configs here
```

**Reference Files:**
- `boards/seeed/xiao_nrf54l15/Kconfig.xiao_nrf54l15`
- `boards/seeed/xiao_nrf54l15/Kconfig.defconfig`

### 1.4 Platform/Modem HAL

The MCU-specific HAL functions required by USP & LoRa Basics Modem are already implemented in USP for Zephyr in those files : `modules/smtc_modem_hal/`.
In case the targeted MCU requires some modifications, please refer to the [USP Porting Guide - HAL Implementation](https://github.com/Lora-net/usp/blob/v1.3.0/doc/usp_porting_guide.md).

</details>

<details>
<summary><b>RF Shield Porting</b></summary>
<br>

When developing a custom RF shield, it is strongly recommended to follow the [**Semtech LR2021 Reference Design**](https://www.semtech.com/products/wireless-rf/lora-plus/lr2021).

Aligning your design with the reference ensures:

- validated and predictable RF performance
- correct PA (Power Amplifier) operation
- optimized output power and efficiency

If your RF shield hardware **matches the LR2021 reference design**, you can **reuse the existing power PA table** `tx-power-cfg-lf` and `tx-power-cfg-hf` provided in shields folder :  `usp_zephyr/boards/shields/semtech_wio_lr20xx/semtech_wio_lr20xx_common.dtsi`

### 2.1 Shield Structure

**Available shields**: See `boards/shields/` directory

**For Custom Shield**, create in `boards/shields/<shield>/`:

```
<shield>/
├── <shield>.overlay         # Variant-specific configurations
├── <shield>_common.dtsi     # Shared definitions across variants (optional)
└── Kconfig.shield           # Shield selection
```

**Note**: The `_common.dtsi` file contains shared configurations (chosen nodes, aliases, LEDs, SPI, radio node) that are used across multiple shield variants.

### 2.2 Shield Overlay Structure

**Shield overlay defines:**
- Radio node with pin mappings
- DIO (Digital I/O) configuration
- Radio-specific parameters (TX power, calibration, etc.)
- Aliases and chosen nodes

**Reference existing shields as templates:**
- `boards/shields/semtech_sx126xmb2xxs/semtech_sx1261mb1bas.overlay`
- `boards/shields/semtech_lr11xxmb1xxs/semtech_lr1110mb1xxs.overlay`
- `boards/shields/semtech_wio_lr20xx/semtech_wio_lr2021.overlay`

Shield overlays can be complex with radio-specific parameters (TX power tables, power consumption, RX bandwidth configs, calibration frequencies). Use an existing similar shield as a starting point and modify pin mappings as needed.

Have a look at
- `dts/bindings/` yaml description files gathering used attributes,
- `include/zephyr/dt-bindings/usp` for radio specific definitions.

### 2.3 RSSI Calibration Gain Tables (LR20xx)

The role of the RSSI calibration gain tables, the meaning of the 27 gain steps and the fixed point encoding of the `gain_value` / `noise_figure` fields are described in the Porting Guide of USP git repository.

**With Zephyr, the table selection is done by the devicetree**: there is no table to pick in the BSP code. The RAL BSP hook `ral_lr20xx_bsp_get_rssi_calibration_table()` in `drivers/usp/lr20xx/lr20xx_ral_bsp_calibration.c` simply returns the tables built from the radio node properties, so this file does **not** need to be modified:

- `rssi-calibration-lf-table`: sub-GHz RX path (below 1.5GHz)
- `rssi-calibration-hf-table`: 2.xGHz RX path

Both properties are described in `dts/bindings/usp/semtech,lr20xx-common.yaml` and are **required**: each one expects 27 `<gain_value noise_figure>` cells, in the order `G1 ... G11`, `G12 boost0..7`, `G13 boost0..7`.

**Where to customize the table**: in the devicetree of your shield/board, on the radio node. In the reference shield, the tables are region-dependent and therefore live in the region `.dtsi` files included by the overlays:

- `boards/shields/semtech_wio_lr20xx/semtech_wio_lr20xx_region_eu_us.dtsi` - LF table characterized for 868/915MHz
- `boards/shields/semtech_wio_lr20xx/semtech_wio_lr20xx_region_cn.dtsi` - LF table characterized for 490MHz

```dts
	/* RSSI calibration gain table, LF path, 868-915MHz */
	rssi-calibration-lf-table =
	/* gain_value noise_figure */
	<(-32) 219>, /* G1 */
	<( -8) 204>, /* G2 */
	/* ... */
	<(232)   9>; /* G13 boost7 */
```

Guidelines:

- Selecting a reference table means selecting the matching shield variant (for example `semtech_wio_lr2021` for 868/915MHz or `semtech_wio_lr2021_cn` for 490MHz), or including the wanted region `.dtsi` in your own overlay.
- The reference values are only valid for a receive chain equivalent to the Semtech reference design. If your board uses a different matching network, an external LNA, a front-end module or an RF switch with a different insertion loss, characterize your own board and write the resulting values in your own devicetree, following the encoding described in the USP Porting Guide.
- Values are written as devicetree cells, so negative gains must be parenthesized (`<(-32) 219>`).

---
</details>
