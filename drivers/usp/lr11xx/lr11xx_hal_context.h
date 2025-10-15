/**
 * @file      lr11xx_hal_context.h
 *
 * @brief     lr11xx_hal_context HAL implementation
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

#ifndef LR11XX_HAL_CONTEXT_H
#define LR11XX_HAL_CONTEXT_H

#include <stdint.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

#include <ral_lr11xx_bsp.h>
#include <lr11xx_system_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback upon firing event trigger
 *
 */
typedef void ( *event_cb_t )( const struct device* dev );

struct lr11xx_hal_context_tcxo_cfg_t
{
    ral_xosc_cfg_t                      xosc_cfg;
    lr11xx_system_tcxo_supply_voltage_t voltage;
    uint32_t                            wakeup_time_ms;
};

struct lr11xx_hal_context_lf_clck_cfg_t
{
    lr11xx_system_lfclk_cfg_t lf_clk_cfg;
    bool                      wait_32k_ready;
};

typedef struct lr11xx_pa_pwr_cfg_s
{
    int8_t  power;
    uint8_t pa_duty_cycle;
    uint8_t pa_hp_sel;
} lr11xx_pa_pwr_cfg_t;

/**
 * @brief lr11xx context device config structure
 *
 */
struct lr11xx_hal_context_cfg_t
{
    struct spi_dt_spec spi; /* spi peripheral */

    struct gpio_dt_spec reset; /* reset pin */
    struct gpio_dt_spec busy;  /* busy pin */
    struct gpio_dt_spec event; /* event pin */

    lr11xx_system_version_type_t chip_type; /* Which configured chip type in device tree */

    uint8_t lf_tx_path_options; /* LF tx path options */

    struct lr11xx_hal_context_tcxo_cfg_t    tcxo_cfg;      /* TCXO/XTAL options*/
    struct lr11xx_hal_context_lf_clck_cfg_t lf_clck_cfg;   /* LF Clock options */
    lr11xx_system_rfswitch_cfg_t            rf_switch_cfg; /* RF Switches options*/
    lr11xx_system_reg_mode_t                reg_mode;      /* Regulator mode */

    int8_t                   tx_power_offset_db; /* Board TX power offset */
    bool                     rx_boosted;         /* RX Boosted option */
    lr11xx_radio_ramp_time_t pa_ramp_time;       /* PA ramp time */

    /* Calibration tables are stored as uint8_t because
     * device tree provides them as flat arrays
     */
    /* Power amplifier configuration for Low frequency / Low power */
    lr11xx_pa_pwr_cfg_t* pa_lf_lp_cfg_table;
    /* Power amplifier configuration for Low frequency / High power */
    lr11xx_pa_pwr_cfg_t* pa_lf_hp_cfg_table;
    /* Power amplifier configuration for High frequency */
    lr11xx_pa_pwr_cfg_t*                  pa_hf_cfg_table;
    lr11xx_radio_rssi_calibration_table_t rssi_calibration_table_below_600mhz;
    lr11xx_radio_rssi_calibration_table_t rssi_calibration_table_from_600mhz_to_2ghz;
    lr11xx_radio_rssi_calibration_table_t rssi_calibration_table_above_2ghz;
    /* TX power to microamperes for Low Frequency, DC-DC regulator, Low Power output, VReg as supply */
    uint32_t* tx_dbm_to_ua_reg_mode_dcdc_lf_lp_vreg;
    /* TX power to microamperes for Low Frequency, LDO regulator, Low Power output, VReg as supply */
    uint32_t* tx_dbm_to_ua_reg_mode_ldo_lf_lp_vreg;
    /* TX power to microamperes for Low Frequency, DC-DC regulator, High Power output, VReg as supply */
    uint32_t* tx_dbm_to_ua_reg_mode_dcdc_lf_hp_vbat;
    /* TX power to microamperes for Low Frequency, LDO regulator, High Power output, VReg as supply */
    uint32_t* tx_dbm_to_ua_reg_mode_ldo_lf_hp_vbat;
    /* TX power to microamperes for High Frequency, DC-DC regulator, VReg as supply */
    uint32_t* tx_dbm_to_ua_reg_mode_dcdc_hf_vreg;
};

/* This type holds the current sleep status of the radio */
typedef enum
{
    RADIO_SLEEP,
    RADIO_AWAKE
} radio_sleep_status_t;

struct lr11xx_hal_context_data_t
{
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    const struct device* lr11xx_dev;
    struct gpio_callback event_cb;           /* event callback structure */
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
    int8_t               tx_power_offset_db_current; /* Board TX power offset */
};

#ifdef __cplusplus
}
#endif

#endif /* LR11XX_HAL_CONTEXT_H */
