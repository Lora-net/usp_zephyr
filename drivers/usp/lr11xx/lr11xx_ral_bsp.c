/**
 * @file      lr11xx_ral_bsp.c
 *
 * @brief     lr11xx_ral_bsp BSP implementation
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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/usp/lora_lbm_transceiver.h>

#include <ral_lr11xx_bsp.h>
#include <lr11xx_radio.h>
#include "lr11xx_hal_context.h"

LOG_MODULE_DECLARE( lora_lr11xx, CONFIG_LORA_BASICS_MODEM_DRIVERS_LOG_LEVEL );

void ral_lr11xx_bsp_get_rf_switch_cfg( const void* context, lr11xx_system_rfswitch_cfg_t* rf_switch_cfg )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr11xx_hal_context_cfg_t* config = dev->config;

    rf_switch_cfg->enable  = config->rf_switch_cfg.enable;
    rf_switch_cfg->standby = 0;
    rf_switch_cfg->rx      = config->rf_switch_cfg.rx;
    rf_switch_cfg->tx      = config->rf_switch_cfg.tx;
    rf_switch_cfg->tx_hp   = config->rf_switch_cfg.tx_hp;
    rf_switch_cfg->tx_hf   = config->rf_switch_cfg.tx_hf;
    rf_switch_cfg->gnss    = config->rf_switch_cfg.gnss;
    rf_switch_cfg->wifi    = config->rf_switch_cfg.wifi;
}

void ral_lr11xx_bsp_get_reg_mode( const void* context, lr11xx_system_reg_mode_t* reg_mode )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr11xx_hal_context_cfg_t* config = dev->config;
    *reg_mode                                     = config->reg_mode;
}

void ral_lr11xx_bsp_get_xosc_cfg( const void* context, ral_xosc_cfg_t* xosc_cfg,
                                  lr11xx_system_tcxo_supply_voltage_t* supply_voltage, uint32_t* startup_time_in_tick )
{
    const struct device*                       transceiver = context;
    const struct lr11xx_hal_context_cfg_t*     config      = transceiver->config;
    const struct lr11xx_hal_context_tcxo_cfg_t tcxo_cfg    = config->tcxo_cfg;

    *xosc_cfg             = tcxo_cfg.xosc_cfg;
    *supply_voltage       = tcxo_cfg.voltage;
    *startup_time_in_tick = lr11xx_radio_convert_time_in_ms_to_rtc_step( tcxo_cfg.wakeup_time_ms );
}

void ral_lr11xx_bsp_get_crc_state( const void* context, bool* crc_is_activated )
{
#if defined( CONFIG_LR11XX_USE_CRC_OVER_SPI )
    LOG_DBG( "LR11XX CRC over spi is activated" );
    *crc_is_activated = true;
#else
    *crc_is_activated = false;
#endif
}

void ral_lr11xx_bsp_get_lora_cad_det_peak( const void* context, ral_lora_sf_t sf, ral_lora_bw_t bw,
                                           ral_lora_cad_symbs_t nb_symbol, uint8_t* in_out_cad_det_peak )
{
    /* Function used to fine tune the cad detection peak, update if needed */
}

void ral_lr11xx_bsp_get_rx_boost_cfg( const void* context, bool* rx_boost_is_activated )
{
    const struct device*                   transceiver = context;
    const struct lr11xx_hal_context_cfg_t* config      = transceiver->config;
    *rx_boost_is_activated                             = config->rx_boosted;
}

void ral_lr11xx_bsp_get_lfclk_cfg_in_sleep( const void* context, bool* lfclk_is_running )
{
#if defined( CONFIG_LORA_BASICS_MODEM_GEOLOCATION )
    *lfclk_is_running = true;
#else
    *lfclk_is_running = false;
#endif
}

void radio_utilities_set_tx_power_offset( const void* context, uint8_t tx_pwr_offset_db )
{
    const struct device*              dev  = ( const struct device* ) context;
    struct lr11xx_hal_context_data_t* data = dev->data;

    data->tx_power_offset_db_current = tx_pwr_offset_db;
}

uint8_t radio_utilities_get_tx_power_offset( const void* context )
{
    const struct device*              dev  = ( const struct device* ) context;
    struct lr11xx_hal_context_data_t* data = dev->data;

    return data->tx_power_offset_db_current;
}