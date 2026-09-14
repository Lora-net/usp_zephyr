# FLRP API

This sample demonstrates a simple FLRP workflow on Zephyr:

- initialize FLRP with a local device EUI,
- run periodic listening as subordinate,
- trigger unicast transmission or reception as initiator,
- trigger transfers from the user button.

This sample was
- validated on STM32L4 & xiao nRF54L15,
- tested (buildable) on STM32U5.

> For FLRP principles and limitations, see the [FLRP documentation](/doc/FLRP.md).

## Overview

The sample uses:

- FLRP core and MAC over RAC,
- a user button shortcut to trigger a transfer,
- compile-time options to select role and FLRP behavior.

At runtime, the main loop executes FLRP/RAC engines and waits on semaphores when idle.

Default behavior:

- `FLRP_API_ROLE=FLRP_API_ROLE_SLAVE` (subordinate role),
- `FLRP_API_CRYPTO_ENABLED=true`,
- `FLRP_API_IS_LOW_FREQUENCY=true`,
- `FLRP_API_IS_LISTENING=true`,
- `FLRP_API_DATA_SIZE=(20 * 1024)`.

## Configuration

> For the **FLRP protocol configuration** (default values, PA ramp time, burst interframe, and
> how/where to configure FLRP), see [FLRP — Configuration](/doc/FLRP_guidelines.md#configuration).

The sample behavior is selected at **compile time**. Pass options via `EXTRA_CFLAGS` (see [Build](#build)).

| Macro | Default | Description |
|-------|---------|-------------|
| `FLRP_API_ROLE` | `FLRP_API_ROLE_SLAVE` (`0`) | `FLRP_API_ROLE_INITIATOR` (`1`) or `FLRP_API_ROLE_SLAVE` (`0`) |
| `FLRP_API_CRYPTO_ENABLED` | `true` | Enable per-packet AES-CMAC integrity (MIC); the payload is **not** encrypted |
| `FLRP_API_IS_LOW_FREQUENCY` | `true` | Use sub-GHz (865 MHz) instead of 2.4 GHz |
| `FLRP_API_IS_LISTENING` | `true` | Enable periodic listening (required for the slave role) |
| `FLRP_API_DATA_SIZE` | `20 * 1024` | Payload size to transfer, in bytes |

**TX power, FLRC data rate, frequency plan and channels** are taken from the FLRP configuration
defaults (`flrp_configuration.h`) and the FLRP init configuration. **For any precise / detailed FLRP
configuration, refer to the [FLRP — Guidelines](/doc/FLRP_guidelines.md) documentation.**

This sample implements a **point-to-point (P2P)** exchange using the **`SMTC_FLRP_BIDIRECTIONAL`**
communication mode (with `SMTC_FLRP_LINK_ADAPTATION_CHANNEL_SELECTION_ONLY`). **`SMTC_FLRP_BIDIRECTIONAL`
is the mode that has been mainly validated** in this release; **broadcast (one-way) configurations are
still being improved.**

Device EUIs and the MIC key are configured in code — see
[Device EUI and key configuration](#device-eui-and-key-configuration) below.

## Build

Target: XIAO nRF54L15 with LoRa Plus EVK (LR20XX).

```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/flrp_api/
```

Low-power configuration:

```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/flrp_api/ -- -DEXTRA_CONF_FILE=prj_lowpower.conf
```

Build as initiator (example):

```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/flrp_api/ -- -DEXTRA_CFLAGS="-DFLRP_API_ROLE=FLRP_API_ROLE_INITIATOR"
```

## Flash

```bash
west flash --runner pyocd
```

## Expected Output

Trace captured on the Zephyr sample. Both devices (initiator and slave) produce the same kind of output (alternating RX/TX transfers).

```
INFO: [TX #2 OK] counter=1
[TX] data preview - (64 bytes):
 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F
 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F
 30 31 32 33 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F

INFO: [RX #3 OK] size=20482 ok/err/nok=74/0/0 rssi=-27 dBm wor_rssi=-27 dBm wor_snr=15 dB
[RX] data preview - (64 bytes):
 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F
 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F
 30 31 32 33 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F
```

## Runtime behavior

- The user button triggers a FLRP action.
- Actions alternate between TX and RX (`flrp_api_initiate_transfer`).
- In initiator role (`FLRP_API_ROLE=1`), the device **automatically triggers transfers on a periodic
  timer** (`FLRP_API_INTIATOR_PERIODIC_TRANSFER`, default 20 s), **alternating** transmit and receive:
  transmit → receive → transmit → … and so on.
- In subordinate role (`FLRP_API_ROLE=0`), periodic listening is started at initialization.

## Device EUI and key configuration

In this sample, source and destination `dev_eui` are defined in code according to `FLRP_API_ROLE`.

- Initiator defaults:
  - local `dev_eui`: `0001020304050607`
  - target `dev_eui`: `1001020304050607`
- Subordinate defaults:
  - local `dev_eui`: `1001020304050607`
  - target `dev_eui`: `0001020304050607`

Default FLRP key (16 bytes):

`000102030405060708090A0B0C0D0E0F`

For custom values, update `src/main.c` or pass compile-time flags in `EXTRA_CFLAGS`.

## Notes

- This sample does not expose FLRP shell commands (unlike `flrp_api_lorawan`).
- The user button is required (`smtc-user-button` devicetree alias).
- Data buffer size is large by default (20 kB).
