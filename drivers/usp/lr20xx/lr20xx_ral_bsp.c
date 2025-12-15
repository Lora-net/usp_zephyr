/**
 * @file      lr20xx_ral_bsp.c
 *
 * @brief     lr20xx_ral_bsp BSP implementation
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

#include <ral_lr20xx_bsp.h>
#include <lr20xx_radio_common.h>
#include "lr20xx_hal_context.h"

LOG_MODULE_DECLARE( lora_lr20xx, CONFIG_LORA_BASICS_MODEM_DRIVERS_LOG_LEVEL );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/**
 * @brief Returns the config of the requested DIO
 *
 */
static lr20xx_dio_cfg_t* lr20xx_get_dio_cfg( const struct lr20xx_hal_context_cfg_t* config, lr20xx_system_dio_t dio )
{
    for( int i = 0; i < config->dios_config_num; i++ )
    {
        if( config->dios_config[i].dio == dio )
        {
            return &config->dios_config[i];
        }
    }
    return NULL;
}

void ral_lr20xx_bsp_get_rx_cfg( const void* context, const uint32_t freq_in_hz, lr20xx_radio_common_rx_path_t* rx_path,
                                lr20xx_radio_common_rx_path_boost_mode_t* boost_mode )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;

    if( freq_in_hz >= 1600000000 )  // 1.6GHz
    {
        *rx_path = LR20XX_RADIO_COMMON_RX_PATH_HF;
    }
    else
    {
        *rx_path = LR20XX_RADIO_COMMON_RX_PATH_LF;
    }

    *boost_mode = config->rx_boosted_cfg;
}

void ral_lr20xx_bsp_get_dio_function( const void* context, lr20xx_system_dio_t dio, lr20xx_system_dio_func_t* function )
{
    const struct device*                   dev        = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config     = dev->config;
    const lr20xx_dio_cfg_t*                dio_config = lr20xx_get_dio_cfg( config, dio );

    if( dio_config )
    {
        *function = dio_config->function;
    }
}

void ral_lr20xx_bsp_get_dio_sleep_drive( const void* context, lr20xx_system_dio_t dio,
                                         lr20xx_system_dio_drive_t* drive )
{
    const struct device*                   dev        = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config     = dev->config;
    const lr20xx_dio_cfg_t*                dio_config = lr20xx_get_dio_cfg( config, dio );

    if( dio_config )
    {
        *drive = dio_config->sleep_drive;
    }
}

void ral_lr20xx_bsp_get_dio_irq_mask( const void* context, lr20xx_system_dio_t dio, lr20xx_system_irq_mask_t* irq_mask )
{
    const struct device*                   dev        = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config     = dev->config;
    const lr20xx_dio_cfg_t*                dio_config = lr20xx_get_dio_cfg( config, dio );

    if( dio_config )
    {
        *irq_mask = dio_config->irq_mask;
    }
}

void ral_lr20xx_bsp_get_dio_rf_switch_cfg( const void* context, lr20xx_system_dio_t dio,
                                           lr20xx_system_dio_rf_switch_cfg_t* rf_switch_cfg )
{
    const struct device*                   dev        = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config     = dev->config;
    const lr20xx_dio_cfg_t*                dio_config = lr20xx_get_dio_cfg( config, dio );

    if( dio_config )
    {
        *rf_switch_cfg = dio_config->rf_switch_cfg;
    }
}

void ral_lr20xx_bsp_get_reg_mode( const void* context, lr20xx_system_reg_mode_t* reg_mode )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;

    *reg_mode = config->reg_mode;
}

void ral_lr20xx_bsp_get_dio_hf_clk_scaling_cfg( const void* context, lr20xx_system_hf_clk_scaling_t* hf_clk_scaling )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;

    *hf_clk_scaling = config->hf_clk_out_scaling;
}

void ral_bsp_lr20xx_get_lfclk_cfg( const void* context, lr20xx_system_lfclk_cfg_t* lfclk_cfg )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;

    *lfclk_cfg = config->lf_clck_cfg.lf_clk_cfg;
}

void ral_lr20xx_bsp_get_xosc_cfg( const void* context, ral_xosc_cfg_t* xosc_cfg,
                                  lr20xx_system_tcxo_supply_voltage_t* supply_voltage, uint32_t* startup_time_in_tick )
{
    const struct device*                       transceiver = context;
    const struct lr20xx_hal_context_cfg_t*     config      = transceiver->config;
    const struct lr20xx_hal_context_tcxo_cfg_t tcxo_cfg    = config->tcxo_cfg;

    *xosc_cfg             = tcxo_cfg.xosc_cfg;
    *supply_voltage       = tcxo_cfg.voltage;
    *startup_time_in_tick = lr20xx_radio_common_convert_time_in_ms_to_rtc_step( tcxo_cfg.wakeup_time_ms );
}

void radio_utilities_set_tx_power_offset( const void* context, uint8_t tx_pwr_offset_db )
{
    const struct device*              dev  = ( const struct device* ) context;
    struct lr20xx_hal_context_data_t* data = dev->data;
    data->tx_power_offset_db_current       = tx_pwr_offset_db;
}

uint8_t radio_utilities_get_tx_power_offset( const void* context )
{
    const struct device*              dev  = ( const struct device* ) context;
    struct lr20xx_hal_context_data_t* data = dev->data;
    return data->tx_power_offset_db_current;
}

void ral_lr20xx_bsp_get_lora_cad_det_peak( const void* context, ral_lora_sf_t sf, ral_lora_cad_symbs_t nb_symbol,
                                           uint8_t* in_out_cad_det_peak )
{
    // Function used to fine tune the cad detection peak, update if needed
}