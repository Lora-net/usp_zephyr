/**
 * @file      sx126x_hal_context.h
 *
 * @brief     sx126x_hal_context HAL implementation
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2025. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SX126X_HAL_CONTEXT_H
#define SX126X_HAL_CONTEXT_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>

#include <ral_sx126x_bsp.h>
#include <sx126x.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sx126x_hal_context_tcxo_cfg_t
{
    ral_xosc_cfg_t              xosc_cfg;
    sx126x_tcxo_ctrl_voltages_t voltage;
    uint32_t                    wakeup_time_ms;
};

typedef struct sx126x_pa_pwr_cfg_s
{
    int8_t  power;
    uint8_t pa_duty_cycle;
    uint8_t pa_hp_sel;
} sx126x_pa_pwr_cfg_t;

struct sx126x_hal_context_cfg_t
{
    struct spi_dt_spec spi; /* spi peripheral */

    struct gpio_dt_spec reset; /* reset pin */
    struct gpio_dt_spec busy;  /* busy pin */

    struct gpio_dt_spec dio1; /* DIO1 pin */
    struct gpio_dt_spec dio2; /* DIO2 pin */
    struct gpio_dt_spec dio3; /* DIO3 pin */

    bool                                 dio2_as_rf_switch;
    struct sx126x_hal_context_tcxo_cfg_t tcxo_cfg; /* TCXO config, says if dio3-tcxo */
    uint8_t                              capa_xta; /* set to 0xFF if not configured*/
    uint8_t                              capa_xtb; /* set to 0xFF if not configured*/

    sx126x_reg_mod_t reg_mode;
    int8_t           tx_power_offset_db; /* Board TX power offset */
    bool             rx_boosted;         /* RXBoosted option */

    sx126x_ramp_time_t pa_ramp_time; /* PA ramp time */
};

/* This type holds the current sleep status of the radio */
typedef enum
{
    RADIO_SLEEP,
    RADIO_AWAKE
} radio_sleep_status_t;

/**
 * @brief Callback upon firing event trigger
 *
 */
typedef void ( *event_cb_t )( const struct device* dev );

struct sx126x_hal_context_data_t
{
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    const struct device* sx126x_dev;
    struct gpio_callback dio1_cb;            /* event callback structure */
    struct gpio_callback dio2_cb;            /* event callback structure */
    struct gpio_callback dio3_cb;            /* event callback structure */
    event_cb_t           event_interrupt_cb; /* event interrupt user provided callback */

#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD
    struct k_work work;
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD */
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD
    K_THREAD_STACK_MEMBER( thread_stack, CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_THREAD_STACK_SIZE );
    struct k_thread thread;
    struct k_sem    trig_sem;
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD */
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER */
    radio_sleep_status_t radio_status;
    int8_t               tx_power_offset_db_current; /* Board TX power offset at reset */
};

#ifdef __cplusplus
}
#endif

#endif /* SX126X_HAL_CONTEXT_H */
