# FLRP API

This sample demonstrates a simple FLRP workflow on Zephyr:

- initialize FLRP with a local device EUI,
- optionally run periodic listening,
- trigger unicast transmission or reception,
- control behavior from the Zephyr shell or from LoRaWAN downlink.

The device that initiates a FLRP transfer does not need periodic listening. The device that receives a transfer command (rx or tx) must be in periodic listening.

> For FLRP principles and limitations, see the [FLRP documentation](/doc/FLRP.md).
>
> For the **FLRP protocol configuration** (default values, PA ramp time, burst interframe, and
> how/where to configure FLRP), see [FLRP — Configuration](/doc/FLRP_guidelines.md#configuration).

This sample implements a **point-to-point (P2P)** exchange using the **`SMTC_FLRP_BIDIRECTIONAL`**
communication mode (with `SMTC_FLRP_LINK_ADAPTATION_CHANNEL_SELECTION_ONLY`). **`SMTC_FLRP_BIDIRECTIONAL`
is the mode that has been mainly validated** in this release; **broadcast (one-way) configurations are
still being improved.**

This sample was
- validated on STM32L4 & xiao nRF54L15,
- tested (buildable) on STM32U5.

## Overview

The sample uses:

- FLRP core and MAC over RAC.
- a shell menu to configure device IDs and launch actions.
- a user button shortcut to trigger a transmission.
- LoRaWan downlink to control and configure the FLRP.

At runtime, the main loop executes:

- smtc_rac_run_engine()
- smtc_flrp_run_engine()

and sleeps on semaphores when FLRP is idle.

## Build

Target: XIAO nRF54L15 with LoRa Plus EVK (LR20XX).

```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/flrp_api_lorawan
```

## Flash

```bash
west flash --runner pyocd
```

## Zephyr Shell Commands

Default shell prompt is:

- flrp_api:~$

The sample registers the following commands:

For `device_eui`, `target_device_eui`, and `set_key`, you can pass either the full hexadecimal value or a 2-character shorthand: the parsed byte is repeated for every byte (for example, `11` becomes `1111111111111111` for an 8-byte EUI, and `22` becomes a 16-byte key filled with `0x22`).

- flrp device_eui <8-byte device_eui in hexadecimal format> <8-byte target_device_eui in hexadecimal format>
  - Sets local and target device EUI (16 hex characters each, or 2 hex characters repeated 8 times)
  - Initializes FLRP and starts periodic listening
- flrp target_device_eui <8-byte target_device_eui in hexadecimal format>
  - Updates target device EUI (16 hex characters, or 2 hex characters repeated 8 times)
- flrp launch_tx
  - Initiates FLRP unicast transmission to target
- flrp launch_rx
  - Initiates FLRP unicast reception from target
- flrp set_key <16-byte key in hexadecimal format>
  - Sets the FLRP MIC key (32 hex characters, or 2 hex characters repeated 16 times)
- flrp set_crypto_enabled <0|1>
  - Enables (1) or disables (0) FLRP integrity / MIC (must be set before `flrp device_eui`)
- flrp set_low_frequency <0|1>
  - Enables (1) or disables (0) low-frequency mode (must be set before `flrp device_eui`)
- flrp display_config
  - Displays current FLRP configuration (device EUI, target EUI, PER, key, listening state)
- flrp display_rx_stats
  - Displays RX statistics
- flrp set_per <burst_target_per>
  - Sets burst target PER (0..100)
- flrp start_listening
  - Starts periodic listening
- flrp stop_listening
  - Stops periodic listening
- uplink (only when `CONFIG_USP_LORA_BASICS_MODEM=y`)
  - Requests a LoRaWAN keepalive uplink

## FLRP remote downlink commands (LoRa Basics Modem)

When `CONFIG_USP_LORA_BASICS_MODEM=y`, this sample can receive remote FLRP commands over LoRaWAN downlink.

Command IDs and payload structures are defined in [`src/flrp_remote.h`](src/flrp_remote.h).

- Downlink FPort for FLRP remote commands: `12` ([`FLRP_REMOTE_PORT`](src/flrp_remote.h#L44))
- Keepalive uplink uses FPort `101` (`KEEP_ALIVE_PORT`)
- Command type is identified by [`cmd_id`](src/flrp_remote.h)

Supported commands:

- [`FLRP_REMOTE_CMD_ID_RESET`](src/flrp_remote.h) (`0x00`)
  - Payload structure: [`flrp_remote_cmd_id_t`](src/flrp_remote.h)
  - Content: `cmd_id`
  - Action: resets the MCU (`smtc_modem_hal_reset_mcu()`)

- [`FLRP_REMOTE_CMD_ID_INIT`](src/flrp_remote.h) (`0x01`)
  - Payload structure: [`flrp_remote_init_cmd_t`](src/flrp_remote.h)
  - Content: `cmd_id`, `device_eui`, `key`, `init_options` (`crypto_enabled`, `is_low_frequency`, `listening`)
  - Action: calls `flrp_api_menu_init(device_eui, key, crypto_enabled, is_low_frequency, listening)`
  - Can be received only once after boot (same constraint as `flrp device_eui`). To apply a new init, reset the device first (for example with [`FLRP_REMOTE_CMD_ID_RESET`](src/flrp_remote.h))

- [`FLRP_REMOTE_CMD_ID_INIT_TRANSFER`](src/flrp_remote.h) (`0x02`)
  - Payload structure: [`flrp_remote_transmit_cmd_t`](src/flrp_remote.h)
  - Content: `cmd_id`, `target_device_eui`, `transmit`
  - Action: calls `flrp_api_menu_initiate_transfer(transmit, target_device_eui)`

- [`FLRP_REMOTE_CMD_ID_STOP_LISTENING`](src/flrp_remote.h) (`0x03`)
  - Payload structure: [`flrp_remote_cmd_id_t`](src/flrp_remote.h)
  - Content: `cmd_id`
  - Action: calls `flrp_api_menu_stop_listening()`

- [`FLRP_REMOTE_CMD_ID_START_LISTENING`](src/flrp_remote.h#L60) (`0x04`)
  - Payload structure: [`flrp_remote_cmd_id_t`](src/flrp_remote.h)
  - Content: `cmd_id`
  - Action: calls `flrp_api_menu_start_listening()`

Notes:

- Payload validation is performed before executing commands.
- Unknown command IDs are ignored and logged as warnings.

## Device EUI configuration

For this sample, source and destination `dev_eui` are configured directly from shell commands using 8-byte hexadecimal values (16 hex characters, or 2 hex characters repeated 8 times).

- Source `dev_eui` (local device / subordinate):
  - Set with `flrp device_eui <device_eui> <target_device_eui>`
  - Example full format: `0102030405060708` (16 hex characters)
  - Example shorthand: `01` (expanded to `0101010101010101`)
  - Once set after boot, `device_eui` cannot be changed again
- Destination `dev_eui` (target device for TX/RX):
  - Set at startup with `flrp device_eui ... ...`
  - Can be updated later with `flrp target_device_eui <target_device_eui>`
  - Same full or 2-character shorthand format as above
  - Can be changed multiple times during runtime

### Typical sequence

Device A:

```text
flrp device_eui A902030405060701 A902030405060702
```

Device B:

```text
flrp device_eui A902030405060702 A902030405060701
```

Optional key update (both devices must use the same key):

```text
flrp set_key 00112233445566778899AABBCCDDEEFF
```

Or using the 2-character shorthand (same key on both devices):

```text
flrp set_key 22
```

Then trigger from either side:

```text
flrp launch_tx
```

or:

```text
flrp launch_rx
```

## Notes

- device_eui can be set only once after boot (as implemented in menu logic).
- target_device_eui can be updated multiple times after boot.
- The user button also triggers launch_tx behavior.
- Data buffer size is currently large (20 kB) in sample code.
