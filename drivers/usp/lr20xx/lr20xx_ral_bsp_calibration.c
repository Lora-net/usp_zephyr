/**
 * @file      lr20xx_ral_bsp_calibration.c
 *
 * @brief     lr20xx_ral_bsp_calibration implementation
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
#include <zephyr/usp/lora_lbm_transceiver.h>

#include <ral_lr20xx_bsp.h>
#include "lr20xx_hal_context.h"
#include "lr20xx_radio_common_types.h"

#define LR20XX_LF_MIN_OUTPUT_POWER -10
#define LR20XX_LF_MAX_OUTPUT_POWER 22

#define LR20XX_HF_MIN_OUTPUT_POWER -17
#define LR20XX_HF_MAX_OUTPUT_POWER 12

#define LR20XX_GFSK_RX_CONSUMPTION_DCDC 5410
#define LR20XX_GFSK_RX_BOOSTED_CONSUMPTION_DCDC 6970

#define LR20XX_GFSK_RX_CONSUMPTION_LDO 9500
#define LR20XX_GFSK_RX_BOOSTED_CONSUMPTION_LDO 11730

static void lr20xx_get_tx_cfg( const void* context, lr20xx_radio_common_pa_selection_t pa_type,
                               int8_t expected_output_pwr_in_dbm, ral_lr20xx_bsp_tx_cfg_output_params_t* output_params )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;

    int8_t power = expected_output_pwr_in_dbm;

    // Ramp time
    output_params->pa_ramp_time = config->pa_ramp_time;

    switch( pa_type )
    {
    case LR20XX_RADIO_COMMON_PA_SEL_LF:
    {
        // Check power boundaries for LP LF PA: The output power must be in range [ -17 , +15 ] dBm
        if( power < LR20XX_LF_MIN_OUTPUT_POWER )
        {
            power = LR20XX_LF_MIN_OUTPUT_POWER;
        }
        else if( power > LR20XX_LF_MAX_OUTPUT_POWER )
        {
            power = LR20XX_LF_MAX_OUTPUT_POWER;
        }
        lr20xx_pa_pwr_cfg_t* pwr_cfg = &config->pa_lf_cfg_table[power - LR20XX_LF_MIN_OUTPUT_POWER];

        output_params->pa_cfg.pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_LF;
        output_params->pa_cfg.pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM,
        output_params->pa_cfg.pa_lf_slices     = pwr_cfg->pa_lf_slices;
        output_params->pa_cfg.pa_lf_duty_cycle = pwr_cfg->pa_duty_cycle;
        output_params->pa_cfg.pa_hf_duty_cycle = 16;

        output_params->chip_output_half_pwr_in_dbm_configured = pwr_cfg->half_power;
        output_params->chip_output_pwr_in_dbm_expected        = power;
        break;
    }

    case LR20XX_RADIO_COMMON_PA_SEL_HF:
    {
        // Check power boundaries for HF PA: The output power must be in range [ -18 , +13 ] dBm
        if( power < LR20XX_HF_MIN_OUTPUT_POWER )
        {
            power = LR20XX_HF_MIN_OUTPUT_POWER;
        }
        else if( power > LR20XX_HF_MAX_OUTPUT_POWER )
        {
            power = LR20XX_HF_MAX_OUTPUT_POWER;
        }
        lr20xx_pa_pwr_cfg_t* pwr_cfg = &config->pa_hf_cfg_table[power - LR20XX_HF_MIN_OUTPUT_POWER];

        output_params->pa_cfg.pa_sel           = LR20XX_RADIO_COMMON_PA_SEL_HF;
        output_params->pa_cfg.pa_lf_mode       = LR20XX_RADIO_COMMON_PA_LF_MODE_FSM;
        output_params->pa_cfg.pa_lf_slices     = 7;
        output_params->pa_cfg.pa_lf_duty_cycle = 0;
        output_params->pa_cfg.pa_hf_duty_cycle = pwr_cfg->pa_duty_cycle;

        output_params->chip_output_half_pwr_in_dbm_configured = pwr_cfg->half_power;
        output_params->chip_output_pwr_in_dbm_expected        = power;
        break;
    }
    }
}

void ral_lr20xx_bsp_get_tx_cfg( const void* context, const ral_lr20xx_bsp_tx_cfg_input_params_t* input_params,
                                ral_lr20xx_bsp_tx_cfg_output_params_t* output_params )
{
    // get board tx power offset
    int8_t board_tx_pwr_offset_db = radio_utilities_get_tx_power_offset( context );

    int16_t power = input_params->system_output_pwr_in_dbm + board_tx_pwr_offset_db;

    lr20xx_radio_common_pa_selection_t pa_type;

    // check frequency band first to choose Low Frequency of High Frequency Power Amplifier
    if( input_params->freq_in_hz >= 1600000000 )  // 1.6GHz
    {
        pa_type = LR20XX_RADIO_COMMON_PA_SEL_HF;
    }
    else
    {
        pa_type = LR20XX_RADIO_COMMON_PA_SEL_LF;
    }

    // call the configuration function
    lr20xx_get_tx_cfg( context, pa_type, power, output_params );
}

void ral_lr20xx_bsp_get_front_end_calibration_cfg(
    const void* context, lr20xx_radio_common_front_end_calibration_value_t front_end_calibration_structures[3] )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;

    lr20xx_radio_common_rx_path_t rx_path = LR20XX_RADIO_COMMON_RX_PATH_LF;

    lr20xx_radio_common_rx_path_boost_mode_t boost_mode = LR20XX_RADIO_COMMON_RX_PATH_BOOST_MODE_NONE;

    uint32_t freq_in_hz[3] = {
        config->calibration_freqs[0],  // Frequency 0 (range from 430MHz to 510MHz)
        config->calibration_freqs[1],  // Frequency 1 (range from 867MHz to 928MHz)
        config->calibration_freqs[2],  // Frequency 2 (range from 2.403GHz to 2.479GHz)
    };

    for( uint8_t i = 0; i < 3; i++ )

    {
        ral_lr20xx_bsp_get_rx_cfg( context, freq_in_hz[i], &rx_path, &boost_mode );

        front_end_calibration_structures[i].rx_path = rx_path;

        front_end_calibration_structures[i].frequency_in_hertz = freq_in_hz[i];
    };
}

ral_status_t ral_lr20xx_bsp_get_instantaneous_tx_power_consumption( const void* context,
                                                                    const ral_lr20xx_bsp_tx_cfg_output_params_t* tx_cfg,
                                                                    lr20xx_system_reg_mode_t radio_reg_mode,
                                                                    uint32_t*                pwr_consumption_in_ua )
{
    // Get Zephyr object from Device Tree
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;

    if( tx_cfg->pa_cfg.pa_sel == LR20XX_RADIO_COMMON_PA_SEL_LF )
    {
        uint8_t index = 0;

        if( tx_cfg->chip_output_pwr_in_dbm_expected > LR20XX_LF_MAX_OUTPUT_POWER )
        {
            index = LR20XX_LF_MAX_OUTPUT_POWER - LR20XX_LF_MIN_OUTPUT_POWER;
        }
        else if( tx_cfg->chip_output_pwr_in_dbm_expected < LR20XX_LF_MIN_OUTPUT_POWER )
        {
            index = 0;
        }
        else
        {
            index = tx_cfg->chip_output_pwr_in_dbm_expected - LR20XX_LF_MIN_OUTPUT_POWER;
        }

        if( radio_reg_mode == LR20XX_SYSTEM_REG_MODE_DCDC )
        {
            *pwr_consumption_in_ua = config->tx_dbm_to_ua_reg_mode_dcdc_lf_vreg[index];
        }
        else
        {
            *pwr_consumption_in_ua = config->tx_dbm_to_ua_reg_mode_ldo_lf_vreg[index];
        }
    }
    else if( tx_cfg->pa_cfg.pa_sel == LR20XX_RADIO_COMMON_PA_SEL_HF )
    {
        uint8_t index = 0;

        if( tx_cfg->chip_output_pwr_in_dbm_expected > LR20XX_HF_MAX_OUTPUT_POWER )
        {
            index = LR20XX_HF_MAX_OUTPUT_POWER - LR20XX_HF_MIN_OUTPUT_POWER;
        }
        else if( tx_cfg->chip_output_pwr_in_dbm_expected < LR20XX_HF_MIN_OUTPUT_POWER )
        {
            index = 0;
        }
        else
        {
            index = tx_cfg->chip_output_pwr_in_dbm_expected - LR20XX_HF_MIN_OUTPUT_POWER;
        }

        if( radio_reg_mode == LR20XX_SYSTEM_REG_MODE_DCDC )
        {
            *pwr_consumption_in_ua = config->tx_dbm_to_ua_reg_mode_dcdc_hf_vreg[index];
        }
        else
        {
            return RAL_STATUS_UNSUPPORTED_FEATURE;
        }
    }
    else
    {
        return RAL_STATUS_UNKNOWN_VALUE;
    }

    return RAL_STATUS_OK;
}

ral_status_t ral_lr20xx_bsp_get_instantaneous_gfsk_rx_power_consumption( const void*              context,
                                                                         lr20xx_system_reg_mode_t radio_reg_mode,
                                                                         bool                     rx_boosted,
                                                                         uint32_t* pwr_consumption_in_ua )
{
    if( radio_reg_mode == LR20XX_SYSTEM_REG_MODE_DCDC )
    {
        *pwr_consumption_in_ua =
            ( rx_boosted ) ? LR20XX_GFSK_RX_BOOSTED_CONSUMPTION_DCDC : LR20XX_GFSK_RX_CONSUMPTION_DCDC;
    }
    else
    {
        *pwr_consumption_in_ua =
            ( rx_boosted ) ? LR20XX_GFSK_RX_BOOSTED_CONSUMPTION_LDO : LR20XX_GFSK_RX_CONSUMPTION_LDO;
    }

    return RAL_STATUS_OK;
}

ral_status_t ral_lr20xx_bsp_get_instantaneous_lora_rx_power_consumption( const void*              context,
                                                                         lr20xx_system_reg_mode_t radio_reg_mode,
                                                                         const ral_lora_bw_t bw, const bool rx_boosted,
                                                                         uint32_t* pwr_consumption_in_ua )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;

    lr20xx_radio_common_rx_path_t rfi   = LR20XX_RADIO_COMMON_RX_PATH_LF;
    uint8_t                       index = 0;

    if( bw <= RAL_LORA_BW_125_KHZ )
    {
        rfi   = LR20XX_RADIO_COMMON_RX_PATH_LF;
        index = 0;
    }
    else if( bw == RAL_LORA_BW_250_KHZ )
    {
        rfi   = LR20XX_RADIO_COMMON_RX_PATH_LF;
        index = 1;
    }
    else if( bw == RAL_LORA_BW_400_KHZ )
    {
        rfi   = LR20XX_RADIO_COMMON_RX_PATH_LF;
        index = 2;
    }
    else if( bw == RAL_LORA_BW_500_KHZ )
    {
        rfi   = LR20XX_RADIO_COMMON_RX_PATH_LF;
        index = 3;
    }
    else if( bw == RAL_LORA_BW_800_KHZ )
    {  // Select Rx HF path
        rfi   = LR20XX_RADIO_COMMON_RX_PATH_HF;
        index = 4;
    }
    else if( bw >= RAL_LORA_BW_1000_KHZ )
    {  // Select Rx HF path
        rfi   = LR20XX_RADIO_COMMON_RX_PATH_HF;
        index = 5;
    }

    if( radio_reg_mode == LR20XX_SYSTEM_REG_MODE_DCDC )
    {
        if( rfi == LR20XX_RADIO_COMMON_RX_PATH_LF )
        {
            *pwr_consumption_in_ua = ( rx_boosted ) ? config->rx_bw_to_ua_reg_mode_dcdc_lf_vreg[index]
                                                    : config->rx_bw_to_ua_reg_mode_dcdc_lf_vreg_boosted[index];
        }
        else  // HF path
        {
            *pwr_consumption_in_ua = ( rx_boosted ) ? config->rx_bw_to_ua_reg_mode_dcdc_hf_vreg[index]
                                                    : config->rx_bw_to_ua_reg_mode_dcdc_hf_vreg_boosted[index];
        }
    }
    else  // LDO mode
    {
        if( rfi == LR20XX_RADIO_COMMON_RX_PATH_LF )
        {
            *pwr_consumption_in_ua = ( rx_boosted ) ? config->rx_bw_to_ua_reg_mode_ldo_lf_vreg[index]
                                                    : config->rx_bw_to_ua_reg_mode_ldo_lf_vreg_boosted[index];
        }
        else  // HF path
        {
            *pwr_consumption_in_ua = ( rx_boosted ) ? config->rx_bw_to_ua_reg_mode_ldo_hf_vreg[index]
                                                    : config->rx_bw_to_ua_reg_mode_ldo_hf_vreg_boosted[index];
        }
    }

    return RAL_STATUS_OK;
}
