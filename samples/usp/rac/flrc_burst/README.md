# FLRC Burst

This application demonstrates **FLRC burst transfer**: sending or receiving a large block of data (e.g. 20 kB) as a continuous stream of fixed-size radio frames at high data rate, with minimal gap between frames.

The FLRP Protocol implemented in flrc_burst example is Work In Progress and available for demonstration. A more Stable version will be available soon.

## What is a burst?

A **burst** is a back-to-back sequence of FLRC packets with a short, fixed inter-frame spacing (e.g. 200 µs). The radio uses a double-buffer (ping-pong) scheme so that the next packet can be loaded while the current one is still being transmitted, keeping the channel busy and maximizing throughput.

## How burst TX works

- **Double-buffer pipeline**: Packet N+1 must be loaded into the radio before packet N has finished transmitting. When the radio reports that packet N is sent (`RP_STATUS_REQUEST_NEXT_TX_PAYLOAD`), the application then loads **packet N+2** into the buffer that was just freed (packet N+1 is already in the other buffer and is being sent).
- **Lock / Unlock**: The radio is locked only for the duration of the burst. On the receiver, the **Lock/Unlock** events are used so that Rx CRC and payload verification are done **after** the radio is unlocked, reducing the time the radio is held locked.

## Key features

- Sub-GHz single-channel FLRC; ~2.08 Mbps PHY (US) / ~1.04 Mbps raw (EU), <~200 µs minimum inter-frame.
- File transfer of 20 kB in one burst (511 bytes per frame).
- Preliminary LoRa WOR sync frame with acknowledgement before the first burst.
- Single direction: initiator sends, receiver listens. No block ack, no protocol security or header.
- Parameters configurable via `apps_configuration.h`.

## Configuration

| Parameter       | Description                                      |
|-----------------|--------------------------------------------------|
| `ROLE`          | `TRANSMITTER` or `RECEIVER`                      |
| `RP_MARGIN_DELAY` | RAC margin delay (e.g. `10`)                   |

## Compilation

Target: XIAO nRF54L15 or STM32L476RG with LoRa Plus EVK (LR20XX). Build with the appropriate role.

**Transmitter:**
```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/flrc_burst -- -DEXTRA_CFLAGS="-DROLE=TRANSMITTER -DRP_MARGIN_DELAY=10"
```

**Receiver:**
```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/rac/flrc_burst -- -DEXTRA_CFLAGS="-DROLE=RECEIVER -DRP_MARGIN_DELAY=10"
```

**Flash:**
```bash
west flash --runner pyocd
```

## Usage

1. Flash the receiver firmware on one device and the transmitter firmware on another (LoRa Plus EVK).
2. Receiver listens for a WOR sync frame, then the FLRC burst.
3. Transmitter: press the user button or wait for the periodic timer to start a burst.

## Expected Output


## Limitations

- Single transfer direction; basic send API, subject to change.
