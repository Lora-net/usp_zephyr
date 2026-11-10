# Overview

This file contains general informations that are relevant to all samples.
For detailled informations about each sample, please refer to the README.md file, located in the directory of the sample.

# Available samples

* lbm
  * geolocation
    - [geolocation](lbm/geolocation/geolocation/README.md)
    - [full_almanac_update](lbm/geolocation/full_almanac_update/README.md)
    - [wifi_region_detection](lbm/geolocation/wifi_region_detection/README.md)
  - [lctt_certif](lbm/lctt_certif/README.md)
  - [periodical_uplink](lbm/periodical_uplink/README.md)
  - [porting_tests](lbm/porting_tests/README.md)
* rac
  - [immediate_radio_access](rac/immediate_radio_access/README.md)
  - [direct_driver_access](rac/direct_driver_access/README.md)
  - [hw_modem](rac/hw_modem/README.md)
  - [cad](rac/cad/README.md)
  - [multiprotocol](rac/multiprotocol/README.md)
  - [flrp_api](rac/flrp_api/README.md)
  - [flrp_api_lorawan](rac/flrp_api_lorawan/README.md)
* sdk
  - [fhss_exemple](sdk/fhss_exemple/README.md)
  - [lrfhss](sdk/lrfhss/README.md)
  - [packet_error_rate_flrc](sdk/packet_error_rate_flrc/README.md)
  - [packet_error_rate_fsk](sdk/packet_error_rate_fsk/README.md)
  - [packet_error_rate_lora](sdk/packet_error_rate_lora/README.md)
  - [ping_pong](sdk/ping_pong/README.md)
  - [ranging_demo](sdk/ranging_demo/README.md)
  - [rf_certification](sdk/rf_certification/README.md)
  - [spectral_scan](sdk/spectral_scan/README.md)
  - [tx_cw](sdk/tx_cw/README.md)

## Hardware Requirements

- **MCU Board**: cf. project README.md
- **Radio Shield**: cf. project README.md
- **Antenna**: Appropriate for selected frequency band
- **USB Connection**: For firmware flashing and debug output

# Configuration and Build

## Using CMake

Most examples can be configured at compile time by passing `-D` flags to CMake.
These flags are documented in a table.
When building using `west`, the syntax is as follow:
```bash
west build --pristine --board <board> --shield <shield> <sample> -- -DEXTRA_CFLAGS="<list of additional -D flags>"
```

## Logging configuration

Logs are configured with Kconfig options, passed in a `.conf` file or directly on the command line:
```bash
west build --pristine --board <board> --shield <shield> <sample> -- -DCONFIG_USP_LOG_LEVEL_DBG=y
```

| Kconfig option                                     | Default | Description                                                                                  |
|----------------------------------------------------|---------|----------------------------------------------------------------------------------------------|
| `CONFIG_USP_LOG_LEVEL_{OFF,ERR,WRN,INF,DBG}`       | Zephyr default | Log level of the USP modules (`usp`, `lorawan`, `lorawan_hal`). Use `_OFF` to disable (there is no `_NONE`) |
| `CONFIG_LORA_BASICS_MODEM_LOG_LEVEL_{OFF,ERR,WRN,INF,DBG}` | Zephyr default | Log level of the LoRa Basics Modem module                                             |
| `CONFIG_LORA_BASICS_MODEM_LOG_VERBOSE`             | `n`     | LoRa Basics Modem deep (debug) traces                                                        |
| `CONFIG_LORA_BASICS_MODEM_RADIO_PLANNER_LOG_VERBOSE` | `n`   | Radio planner deep traces                                                                    |
| `CONFIG_LORA_BASICS_MODEM_GEOLOCATION_LOG_VERBOSE` | `n`     | Geolocation and Wi-Fi middlewares deep traces                                                |
| `CONFIG_USP_MODEM_HAL_DBG_TRACE`                   | `y`     | `SMTC_MODEM_HAL_TRACE_*` traces (LBM, RAC, radio planner). `n` removes them from the build   |
| `CONFIG_USP_HAL_DBG_TRACE`                         | `y`     | `SMTC_HAL_TRACE_*` traces (samples and helpers). `n` removes them from the build             |

The RAC core logs are selected with C definitions, passed through `EXTRA_CFLAGS` with a `0` or `1` value (`ON`/`OFF` are silently ignored).
They are printed only when `CONFIG_USP_MODEM_HAL_DBG_TRACE=y` and the USP log level is high enough:
```bash
west build --pristine --board <board> --shield <shield> <sample> -- -DEXTRA_CFLAGS="-DRAC_CORE_LOG_INFO_ENABLE=1"
```

| C definition                 | Default | Description                  |
|------------------------------|---------|------------------------------|
| `RAC_CORE_LOG_ERROR_ENABLE`  | `1`     | Enable RAC Core ERROR logs   |
| `RAC_CORE_LOG_CONFIG_ENABLE` | `1`     | Enable RAC Core CONFIG logs  |
| `RAC_CORE_LOG_INFO_ENABLE`   | `0`     | Enable RAC Core INFO logs    |
| `RAC_CORE_LOG_WARN_ENABLE`   | `0`     | Enable RAC Core WARN logs    |
| `RAC_CORE_LOG_DEBUG_ENABLE`  | `0`     | Enable RAC Core DEBUG logs   |
| `RAC_CORE_LOG_API_ENABLE`    | `0`     | Enable RAC Core API logs     |
| `RAC_CORE_LOG_RADIO_ENABLE`  | `0`     | Enable RAC Core RADIO logs   |

To build without any log, see "Building without logs" in the [main README](../../README.md).

## Using the Device Tree

Some examples require LoRaWAN Parameters. Configure these parameters in your device tree or overlay file:

```dts
&{/zephyr,user} {
    user-lorawan-device-eui = [00 01 02 03 04 05 06 07];
    user-lorawan-join-eui = [00 01 02 03 04 05 06 07];
    user-lorawan-gen-app-key = [00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F];
    user-lorawan-app-key = [00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F];
    user-lorawan-region = "EU868";
};
```

## Flash and Run

- Flash the firmware to your MCU board equipped with the Semtech&copy radio.
```
west flash # optional --runner parameter may be given
```

# Common issues

1. **Build/compilation failure**:
   - Check that radio parameters are properly configured in CMake
   - Ensure USP environment is correctly set up
   - Ensure correct shield and board configuration

2. **Communication issues**:
   - Verify frequency, sync word, and network settings match between devices

3. **No packets received**:
   - Verify that radio parameters are identical on both devices
   - Check frequency and proximity of devices

4. **Very high PER**:
   - Increase transmission power
   - Reduce distance between devices
   - Change channel/frequency

5. **Join failures**:
   - Check LoRaWAN credentials and network server configuration

6. **No ranging results/ranging failure**:
   - Check device positioning (line of sight recommended)
   - Verify both devices use same configuration
   - Ensure adequate distance (minimum ~1 meter)

7. **Timeout errors**:
   - Check RF environment for interference
   - Verify antenna connections
   - Try different frequency or power settings

8. **Inconsistent Results**:
   - Ensure stable device positioning
   - Check for RF reflections or multipath
   - Consider environmental factors (weather, obstacles)

9. **OLED Display Issues**:
   - Verify I2C connections and address
   - Check display power supply
   - Ensure SSD1306 driver configuration

# Debug

## Debugging output

All applications provide comprehensive debug output via UART.
Use a terminal emulator like `minicom` to monitor:
```bash
minicom -D /dev/ttyACM0 [-b <baudrate>]
```
The default baudrate used is 115200. But check on your MCU applicaiton overlay file (e.g. `samples/usp/lbm/periodical_uplink/boards/xiao_nrf54l15_nrf54l15_cpuapp.overlay`

## Stepping through code

1. Open a GDB server on the chip using OpenOCD or pyOCD
   ```bash
   openocd -f interface/stlink.cfg -f target/stm32l4x.cfg [-c "gdb_port <GDB_PORT>"]
   ```
   The default GDB port is 3333.

2. Open the flashed binary with GDB (preferably with the ELF format)
   ```bash
   gdb path/to/binary
   ```

3. In GDB, connect to the device and restart its execution
   ```
   >>> target extended-remote :<GDB_PORT>
   >>> monitor reset halt
   ```

You can now proceed with the usual GDB commands, like `break main` and `continue`.
