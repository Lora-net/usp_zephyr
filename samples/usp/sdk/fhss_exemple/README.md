# FHSS Example

This application demonstrates LoRa intra-packet frequency hopping between two LR20xx radios through the USP/RAC framework.

## Features

- Intra-packet frequency hopping using the LR20xx LoRa hopping engine
- LR20xx-to-LR20xx PING/PONG exchange
- Deterministic payload validation with retry handling
- TX/RX LED feedback

## Configuration

The defaults in `main_fhss_example.h` target LR2021-class radios:

| Parameter | Default | Description |
|---|---:|---|
| `FHSS_HOP_FREQ_0_IN_HZ` | `490000000` | Initial carrier frequency |
| `FHSS_HOP_PERIOD_IN_SYMBOLS` | `2` | Hop period in LoRa symbols |
| `RF_FREQ_IN_HZ` | `FHSS_HOP_FREQ_0_IN_HZ` | Initial LoRa frequency |
| `TX_OUTPUT_POWER_DBM` | `14` | TX output power |
| `LORA_SPREADING_FACTOR` | `RAL_LORA_SF10` | LoRa spreading factor |
| `LORA_BANDWIDTH` | `RAL_LORA_BW_125_KHZ` | LoRa bandwidth |
| `LORA_CODING_RATE` | `RAL_LORA_CR_4_5` | LoRa coding rate |
| `LORA_PREAMBLE_LENGTH` | `12` | Preamble length |
| `LORA_PKT_LEN_MODE` | `RAL_LORA_PKT_EXPLICIT` | Packet length mode |
| `LORA_IQ` | `false` | IQ inversion |
| `LORA_CRC` | `true` | CRC enable |

## Compilation

**Build LR2021 to LR2021 ping-pong firmware:**

```bash
west build --pristine --board xiao_nrf54l15/nrf54l15/cpuapp --shield semtech_loraplus_expansion_board --shield semtech_wio_lr2021 usp_zephyr/samples/usp/sdk/fhss_exemple
```

**Flash the firmware:**

```bash
west flash --runner pyocd
```

If multiple NUCLEO/ST-LINK boards are connected, always pass the adapter serial number to avoid flashing the wrong board.

## Usage

### LR2021 to LR2021

1. Flash the same LR2021-to-LR2021 firmware to both LR20xx boards.
2. Start both boards. Each device starts as subordinate and waits in RX.
3. Press the user button on one board to make it manager and start the exchange.
4. Monitor UART output and TX/RX LEDs.


## Technical Notes

- This example requires an LR20xx radio. `CMakeLists.txt` rejects non-LR20xx `RAC_RADIO` selections.
- In LR2021-to-LR2021 mode, the example uses PING/PONG payloads and retries failed exchanges.
