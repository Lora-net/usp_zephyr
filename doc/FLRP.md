# FLRP (Fast LoRa communication Protocol)

This page gathers the Zephyr samples related to FLRP (Semtech FLRC based protocol).
FLRP will bring multiple services to ease the use of FLRC communication across different contexts and use case. Its first release provides the file-transfer feature: FLRP-BURST.

## Principles

See [FLRP-BURST — Principles](FLRP_principles.md) — overview, roles & directions, transfer phases (WOR / Adaptive Link / Burst / Retry), low-power wake-up, default configuration, and FLRP-BURST vs FLRC.

## Guidelines

See [FLRP-BURST — Guidelines](FLRP_guidelines.md) — regulatory compliance, recommendations, configuration & default values, and known limitations.

## Getting Started on FLRP API

See the [USP documentation](https://github.com/Lora-net/usp) for the FLRP API reference.

## Samples

See [flrp_api](../samples/usp/rac/flrp_api/README.md) — simple FLRP workflow on Zephyr (periodic listening, unicast TX/RX, transfers triggered from the user button).

See [flrp_api_lorawan](../samples/usp/rac/flrp_api_lorawan/README.md) — FLRP workflow on Zephyr controlled from the Zephyr shell.
