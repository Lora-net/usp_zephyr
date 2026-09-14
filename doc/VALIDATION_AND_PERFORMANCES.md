# Validation & Performances

This page summarizes **how** the software was validated, **with which configuration**, and a few
**performance / integration constraints** (SPI clock, upcoming memory footprint).

## Validation methodology

The release goes through Semtech's **nominal (full) validation process**. Validation relies on a
set of example applications, each exercising a different layer of the stack:

- **`lbm/lctt_certif`** — run against the official **LCTT (LoRaWAN Certification Test Tool)** to
  check LoRaWAN certification behavior (Class A/B/C, regions).
- **`lbm/periodical_uplink`** — end-to-end **LoRaWAN** behavior (periodic + button-triggered
  uplinks) against a network server.
- **`lbm/porting_tests`** — validates the **HAL / porting layer** on each platform: SPI access,
  radio IRQ, `get_time`, timers, RNG, sleep, low-power.
- **`rac/hw_modem`** — host-controlled (serial) modem, exercised through its command interface.
- Other **RAC / SDK** examples (`ping_pong`, `packet_error_rate_*`, `cad`, `flrp_api`, …) exercise
  the individual radio features.

## Validation maturity levels

The same wording is used in the [root README](../README.md):

- **Validated** — passed the Semtech nominal full validation process.
- **Buildable** — can be compiled but did **not** go through the full Semtech validation process
  (to be considered experimental).
- **Might work** — compiled and tested on the `periodical_uplink` sample only, with low validation.

## Validated configuration

- **Zephyr RTOS:** Validated with **Zephyr RTOS v4.4.0 & Zephyr SDK v1.0.1**.
  - Buildable with **v4.4-branch & SDK v1.0.1** (recommended to get Zephyr fixes) and with
    **v3.7.0 LTS & SDK v0.16.9** (see limitations).
  - The delivery was **not** validated with toolchains other than the default Zephyr SDK.
- **Boards & shields / radios:** see [Supported Boards & Shields](../README.md#supported-boards--shields)
  and the [LoRa Plus™ EVK](LORA_PLUS_EVK.md).
- See [Known Limitations](KNOWN_LIMITATIONS.md) for per-configuration caveats.

## Validation coverage per domain

Validation maturity and the SPI clock at which it was performed, per functional domain:

| Domain | Full validation | Also run / lighter testing | SPI / datarate notes |
|--------|-----------------|----------------------------|----------------------|
| **FLRP** | Xiao nRF54L15 @ **8 MHz** and STM32L4 (`nucleo_l476rg`) @ **10 MHz** | Launched & lightly tested on **STM32U5** | Most features should also work on Xiao nRF54L15 @ **16 MHz**. **Below 8 MHz the datarate must be lowered / adapted.** |
| **LoRaWAN** | Xiao nRF54L15 @ **8 MHz** and STM32L4 @ **10 MHz** | — | |
| **Other example apps** | Xiao nRF54L15 @ **8 MHz** and STM32L4 @ **10 MHz** | Tested regularly @ **16 MHz** | Should also work at **much lower** SPI speeds |

## SPI clock speed

The maximum SPI clock used / validated per platform:

- **Xiao nRF54L15:** SPI speed reduced to **8 MHz**.
- **STM32L4:**
  - **10 MHz** in the **FLRP** examples
  - **5 MHz** for the other examples
- **Other MCUs:** **8 MHz or less**.

Notes:
- Saleae Logic Analyzer probes were OK at these speeds.
- **nRF54L15-DK** only worked at **4 MHz** with the adapter used.

## Memory footprint (upcoming)

**RAM / Flash footprint** figures per sample/configuration will be added in a **future release**.

---

*See also: [LoRa Plus™ EVK](LORA_PLUS_EVK.md) · [FLRP](FLRP.md) · [Known Limitations](KNOWN_LIMITATIONS.md)*
