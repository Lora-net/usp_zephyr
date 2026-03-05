# STM32WLE5 Internal Radio vs External SX1262 — Porting Notes

This document explains the architectural differences between the STM32WLE5 embedded
sub-GHz radio and a classic external SX1262, and why the STM32WL-specific driver
(`sx126x_hal_stm32wl.c`, `sx126x_board_stm32wl.c`) differs from the standard one
(`sx126x_hal.c`, `sx126x_board.c`).

---

## 1. SPI Bus: Internal Peripheral vs External GPIO

| Signal | External SX1262 | STM32WLE5 |
|--------|-----------------|-----------|
| SPI bus | Any SPI peripheral | `subghzspi` — dedicated internal peripheral |
| NSS (chip select) | GPIO pin, driven manually | Internal (managed by `subghzspi` hardware) |
| BUSY signal | `BUSY` GPIO pin (HIGH = radio busy) | `PWR_SR2.RFBUSYS` register bit |

### RFBUSYS busy-wait ordering

On an external SX1262, the host reads the `BUSY` pin **before** asserting NSS to ensure
the radio is ready. On STM32WL, `RFBUSYS` reflects whether NSS is currently asserted —
polling it **before** starting a transaction causes a deadlock:

> We wait for NSS to deassert, but NSS only deasserts when the transaction we have not
> yet started completes.

The correct sequence on STM32WL:
1. Start the SPI transaction (`spi_write_dt` / `spi_transceive_dt`).
2. Wait for `RFBUSYS = 0` (NSS deasserted, wakeup complete).

Special case: after `SetSleep`, `RFBUSYS` stays HIGH during sleep. Do not poll it;
wait the mandatory 500 µs calibration time instead.

---

## 2. Reset: External GPIO vs Internal RCC

| | External SX1262 | STM32WLE5 |
|--|-----------------|-----------|
| Reset mechanism | `NRESET` GPIO pin, active low | `LL_RCC_RF_EnableReset()` / `LL_RCC_RF_DisableReset()` |

`sx126x_hal_reset()` uses STM32 LL RCC calls instead of toggling a GPIO:

```c
LL_RCC_RF_EnableReset();
k_msleep(20);
LL_RCC_RF_DisableReset();
k_msleep(10);
```

---

## 3. DIO1 Radio Interrupt: GPIO EXTI vs NVIC IRQ 50 (level-triggered)

This is the most impactful difference and required two separate fixes.

| | External SX1262 | STM32WLE5 |
|--|-----------------|-----------|
| DIO1 signal | GPIO pin → EXTI, edge-triggered | EXTI line 44 → **NVIC IRQ 50** (`SUBGHZ_Radio_IRQn`) |
| Trigger type | Rising edge (one pulse per event) | **Level-triggered** — stays HIGH until `ClearIrqStatus` sent |

### Problem: IRQ storm

With a level-triggered source, if the ISR returns while DIO1 is still HIGH (i.e., before
`ClearIrqStatus` is sent over SPI by the USP thread), the NVIC immediately re-enters the
ISR. This creates an infinite loop that starves every thread.

**Fix — disable in ISR:**

```c
static void sx126x_stm32wl_radio_isr(const struct device *dev)
{
    irq_disable(DT_IRQ_BY_IDX(..., 0, irq));   /* prevent re-entry storm */
    sx126x_stm32wl_event_callback(data);        /* wake the USP thread */
}
```

### Problem: IRQ never re-enabled after first event

`irq_enable(50)` is called once at init via `smtc_modem_hal_irq_config_radio_irq()`.
The Radio Planner (RP) does **not** call `smtc_modem_hal_enable_modem_irq()` after
processing radio events — that function is only used by `fifo_ctrl.c` for its own
critical sections.

Result: after TX_DONE fires and the ISR disables IRQ 50, the interrupt stays disabled.
`SetRx` is sent later but the `RX_TIMEOUT` / `RX_DONE` event never reaches the CPU.
The RP watchdog (`RP_FAILSAFE`) fires after 128 s.

**Fix — re-enable in `sx126x_hal_write()`:**

```c
/* At the end of sx126x_hal_write(), after every successful SPI write: */
lora_transceiver_board_enable_interrupt(dev);   /* = irq_enable(50) */
```

After each SPI write completes and `wait_on_busy()` confirms the radio is ready,
DIO1 is either already LOW or will deassert on the imminent `ClearIrqStatus` write.
Re-arming NVIC IRQ 50 at this point is always safe and ensures no event is missed.

### ISR chain (NO_THREAD mode)

```
NVIC IRQ 50
  └─ sx126x_stm32wl_radio_isr()      irq_disable(50)
       └─ prv_transceiver_event_cb()
            └─ rp_radio_irq_callback()   radio_irq_flag = true
                 └─ smtc_modem_hal_wake_up()   k_sem_give()
                      └─ (main loop wakes, calls smtc_modem_run_engine / smtc_rac_run_engine)
                           └─ rp_callback() → rp_irq_get_status() → ClearIrqStatus (SPI)
                                └─ sx126x_hal_write() → lora_transceiver_board_enable_interrupt()
                                                         irq_enable(50)  ← re-armed here
```

---

## 4. RF Switch: DIO2 vs External MCU GPIOs

`dio2-as-rf-switch` is an SX1262 hardware feature: setting it tells the chip to
autonomously toggle DIO2 during TX. On STM32WL boards, DIO2 is not wired to the RF
switch; the MCU must drive GPIOs manually before each `SetRx` / `SetTx` SPI command.

| | Typical SX1262 board | RAK3172_SIP / RAK3172LP_SIP | NUCLEO-WL55JC |
|--|----------------------|-----------------------------|----------------|
| RF switch control | `dio2-as-rf-switch` (chip-driven) | PA1 = RF_TX (ACTIVE_HIGH), PA0 = RF_RX (ACTIVE_HIGH) | PC4 = FE_CTRL1 TX (ACTIVE_LOW), PC5 = FE_CTRL2 RX (ACTIVE_LOW) |

> **Note:** plain RAK3172 uses an external SX1262 over SPI (standard sx126x driver).
> It does NOT use this STM32WL driver and has its own GPIO wiring.

`sx126x_stm32wl_set_rf_switch()` in `sx126x_hal_stm32wl.c` is called at the start of
every `sx126x_hal_write()`, keyed on the command opcode:

| Opcode | tx_rf_switch | rx_rf_switch |
|--------|--------------|--------------|
| `0x83` SetTx | asserted | deasserted |
| `0x82` SetRx | deasserted | asserted |
| anything else | deasserted | deasserted |

Active level (HIGH or LOW) is taken from the DTS `tx-enable-gpios` / `rx-enable-gpios`
`GPIO_ACTIVE_HIGH` / `GPIO_ACTIVE_LOW` flags — no hard-coded polarity in the driver.

---

## 5. TCXO: Same Mechanism

Both use `SetDio3AsTcxoCtrl` to let the SX126x supply the regulated voltage to the
external TCXO via DIO3. No implementation difference.
- RAK3172_SIP / RAK3172LP_SIP : 1.7 V, 5 ms startup delay
- NUCLEO-WL55JC               : 1.7 V, 5 ms startup delay

---

## 6. PA: LP vs HP depending on board variant

| | External SX1262 | RAK3172_SIP / NUCLEO-WL55JC | RAK3172LP_SIP |
|--|-----------------|----------------------------|---------------|
| PA type | HP, up to +22 dBm | HP (`rfo-hp`), up to +22 dBm | LP (`rfo-lp`), up to +14 dBm |
| Compile define | `SX1262` | `SX1261` (USP library) | `SX1261` (USP library) |

The `power-amplifier-output` DTS property maps to `pa_is_lp` in the driver config struct:
- `"rfo-hp"` → `pa_is_lp = false` → HP PA path, calibration for +22 dBm
- `"rfo-lp"` → `pa_is_lp = true`  → LP PA path, calibration tables matching SX1261

In both cases `SX1261` must be passed to the USP library CMake (`-DSX1261=1`) because
the STM32WL PA config registers match the SX1261 LP PA, even on the HP variant.

---

## Summary

The external SX1262 exposes every control signal as a GPIO pin with well-defined
edge-triggered behaviour. The STM32WLE5 embeds the same radio core but replaces every
external signal with an internal MCU register or peripheral:

| External SX1262 signal | STM32WLE5 equivalent | Driver change |
|------------------------|----------------------|---------------|
| `BUSY` GPIO | `PWR_SR2.RFBUSYS` | Poll after SPI, not before |
| `NRESET` GPIO | RCC SubGHz reset | Use LL RCC API |
| DIO1 GPIO (edge) | NVIC IRQ 50 (level) | `irq_disable` in ISR + `irq_enable` after each SPI write |
| SPI + NSS GPIO | Internal `subghzspi` | Use Zephyr `subghzspi` device node |
| DIO2 RF switch | Not wired → MCU GPIOs | Drive tx/rx GPIOs before SetRx/SetTx (polarity from DTS) |
| SX1262 HP PA | HP or LP depending on board | `SX1261` define; `rfo-hp` (RAK3172_SIP, NUCLEO) or `rfo-lp` (RAK3172LP_SIP) |

### Board summary

| Board | PA | RF switch | Regulator | Driver |
|-------|----|-----------|-----------|--------|
| RAK3172 (non-SiP) | HP +22 dBm | External SX1262 DIO2 | — | **standard sx126x (SPI)** |
| RAK3172_SIP | HP +22 dBm | PA1/PA0 ACTIVE_HIGH | LDO | **this driver** |
| RAK3172LP_SIP | LP +14 dBm | PA1/PA0 ACTIVE_HIGH | LDO | **this driver** |
| NUCLEO-WL55JC | HP +22 dBm | PC4/PC5 ACTIVE_LOW | DC-DC | **this driver** |
