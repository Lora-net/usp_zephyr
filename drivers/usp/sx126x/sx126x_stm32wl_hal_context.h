/**
 * @file      sx126x_stm32wl_hal_context.h
 *
 * @brief     Device driver context for the STM32WL internal SX126x radio.
 *
 * Unlike the external SPI radio (sx126x_hal_context.h), the STM32WL variant
 * has no external GPIO pins for reset, busy, or DIOs. Those signals are
 * accessed through CPU-internal registers (RCC, PWR, EXTI).
 *
 * Copyright (c) 2026 Semtech Corporation
 * SPDX-License-Identifier: Apache-2.0
 * Created by Charles-Henri Hallard (charles@evolyenergy.com)
 */

#ifndef SX126X_STM32WL_HAL_CONTEXT_H
#define SX126X_STM32WL_HAL_CONTEXT_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

#include <ral_sx126x_bsp.h>
#include <sx126x.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reuse the TCXO config struct from the standard context */
struct sx126x_hal_context_tcxo_cfg_t
{
    ral_xosc_cfg_t              xosc_cfg;
    sx126x_tcxo_ctrl_voltages_t voltage;
    uint32_t                    wakeup_time_ms;
};

/**
 * @brief Compile-time configuration for the STM32WL internal radio.
 *
 * Populated by the DTS macro SX126X_STM32WL_CONFIG(node_id).
 */
struct sx126x_stm32wl_hal_context_cfg_t
{
    struct spi_dt_spec spi; /* subghzspi bus spec */

    /* External RF switch GPIOs (optional — absent when dio2-tx-enable is used).
     * RAK3172_SIP / RAK3172LP_SIP : PA1 = RF_TX (ACTIVE_HIGH), PA0 = RF_RX (ACTIVE_HIGH)
     * NUCLEO-WL55JC               : PC4 = FE_CTRL1 TX (ACTIVE_LOW), PC5 = FE_CTRL2 RX (ACTIVE_LOW)
     * Note: plain RAK3172 uses an external SX1262 over SPI — it does NOT use this driver. */
    struct gpio_dt_spec tx_rf_switch;
    struct gpio_dt_spec rx_rf_switch;

    bool                                 dio2_tx_enable;
    struct sx126x_hal_context_tcxo_cfg_t tcxo_cfg;
    uint8_t                              capa_xta; /* 0xFF = not set */
    uint8_t                              capa_xtb; /* 0xFF = not set */

    sx126x_reg_mod_t   reg_mode;
    int8_t             tx_power_offset_db;
    bool               rx_boosted;
    sx126x_ramp_time_t pa_ramp_time;

    /* PA output type and per-variant max power */
    bool               pa_is_lp;
    int8_t             pa_lp_max_power_dbm; /* max TX power for LP PA in dBm */
    int8_t             pa_hp_max_power_dbm; /* max TX power for HP PA in dBm */
};

/* Current sleep state of the internal radio */
typedef enum
{
    RADIO_SLEEP,
    RADIO_AWAKE
} radio_sleep_status_t;

/* Callback type used by the board layer to notify the USP stack of a DIO event */
typedef void ( *event_cb_t )( const struct device* dev );

/**
 * @brief Runtime data for the STM32WL internal radio device.
 */
struct sx126x_stm32wl_hal_context_data_t
{
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    const struct device* sx126x_dev;
    event_cb_t           event_interrupt_cb;
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD
    struct k_work work;
#endif
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD
    K_THREAD_STACK_MEMBER( thread_stack, CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_THREAD_STACK_SIZE );
    struct k_thread thread;
    struct k_sem    gpio_sem;
#endif
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER */

    radio_sleep_status_t radio_status;
    int8_t               tx_power_offset_db_current;
};

#ifdef __cplusplus
}
#endif

#endif /* SX126X_STM32WL_HAL_CONTEXT_H */
