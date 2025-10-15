# Overview

This file contains general informations that are relevant to all samples.
For detailled informations about each sample, please refer to the README.md file, located in the directory of the sample.

# Available samples

```
.
├── lbm
│  ├── `geolocation`
│  ├── `lctt_certif`
│  ├── `periodical_uplink`
│  └── `porting_tests`
├── rac
│  ├── `direct_driver_access`
│  ├── `hw_modem`
│  └── `multiprotocol`
└── sdk
   ├── `lrfhss`
   ├── `packet_error_rate_flrc`
   ├── `packet_error_rate_fsk`
   ├── `packet_error_rate_lora`
   ├── `ping_pong`
   ├── `ranging_demo`
   ├── `rf_certification`
   ├── `spectral_scan`
   └── `tx_cw`

```

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
west build --pristine --board <board> --shield <shield> <sample> -- -DCMAKE_C_FLAGS="<list of additional -D flags>"
```

All examples use the unified logging system, which is controlled with the following flags:

| Parameter                    | Default   | Description                                         |
|------------------------------|-----------|-----------------------------------------------------|
| `RAC_LOGGING_ENABLE`         | `ON`      | Enable logging system                               |
| `RAC_LOG_PROFILE`            | `DEFAULT` | logging profile (MINIMAL;DEFAULT;VERBOSE;DEBUG;ALL) |
|------------------------------|-----------|-----------------------------------------------------|
| `RAC_CORE_LOG_ERROR_ENABLE`  | `ON`      | Enable RAC Core ERROR logs                          |
| `RAC_CORE_LOG_CONFIG_ENABLE` | `ON`      | Enable RAC Core CONFIG logs                         |
| `RAC_CORE_LOG_INFO_ENABLE`   | `OFF`     | Enable RAC Core INFO logs                           |
| `RAC_CORE_LOG_WARN_ENABLE`   | `OFF`     | Enable RAC Core WARN logs                           |
| `RAC_CORE_LOG_DEBUG_ENABLE`  | `OFF`     | Enable RAC Core DEBUG logs                          |
| `RAC_CORE_LOG_API_ENABLE`    | `OFF`     | Enable RAC Core API logs                            |
| `RAC_CORE_LOG_RADIO_ENABLE`  | `OFF`     | Enable RAC Core RADIO logs                          |
|------------------------------|-----------|-----------------------------------------------------|
| `RAC_LORA_LOG_TX_ENABLE`     | `ON`      | Enable RAC LoRa TX logs                             |
| `RAC_LORA_LOG_RX_ENABLE`     | `ON`      | Enable RAC LoRa RX logs                             |
| `RAC_LORA_LOG_CONFIG_ENABLE` | `ON`      | Enable RAC LoRa CONFIG logs                         |
| `RAC_LORA_LOG_ERROR_ENABLE`  | `ON`      | Enable RAC LoRa ERROR logs                          |
| `RAC_LORA_LOG_INFO_ENABLE`   | `OFF`     | Enable RAC LoRa INFO logs                           |
| `RAC_LORA_LOG_WARN_ENABLE`   | `OFF`     | Enable RAC LoRa WARN logs                           |
| `RAC_LORA_LOG_DEBUG_ENABLE`  | `OFF`     | Enable RAC LoRa DEBUG logs                          |

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
