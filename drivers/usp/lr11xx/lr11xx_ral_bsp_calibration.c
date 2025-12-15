/**
 * @file      lr11xx_ral_bsp_calibration.c
 *
 * @brief     lr11xx_ral_bsp_calibration implementation
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

#include <ral_lr11xx_bsp.h>
#include "lr11xx_hal_context.h"

typedef enum lr11xx_pa_type_s
{
    LR11XX_WITH_LF_LP_PA,
    LR11XX_WITH_LF_HP_PA,
    LR11XX_WITH_LF_LP_HP_PA,
    LR11XX_WITH_HF_PA,
} lr11xx_pa_type_t;

#define LR11XX_GFSK_RX_CONSUMPTION_DCDC 5400
#define LR11XX_GFSK_RX_BOOSTED_CONSUMPTION_DCDC 7500

#define LR11XX_GFSK_RX_CONSUMPTION_LDO 5400
#define LR11XX_GFSK_RX_BOOSTED_CONSUMPTION_LDO 7500

#define LR11XX_LORA_RX_CONSUMPTION_DCDC 5700
#define LR11XX_LORA_RX_BOOSTED_CONSUMPTION_DCDC 7800

#define LR11XX_LORA_RX_CONSUMPTION_LDO 5700
#define LR11XX_LORA_RX_BOOSTED_CONSUMPTION_LDO 7800

#define LR11XX_LF_LP_MIN_OUTPUT_POWER -17
#define LR11XX_LF_LP_MAX_OUTPUT_POWER 15

#define LR11XX_LF_HP_MIN_OUTPUT_POWER -9
#define LR11XX_LF_HP_MAX_OUTPUT_POWER 22

#define LR11XX_HF_MIN_OUTPUT_POWER -18
#define LR11XX_HF_MAX_OUTPUT_POWER 13

#define LR11XX_LP_CONVERT_TABLE_INDEX_OFFSET 17
#define LR11XX_HP_CONVERT_TABLE_INDEX_OFFSET 9
#define LR11XX_HF_CONVERT_TABLE_INDEX_OFFSET 18

#define LR11XX_PWR_VREG_VBAT_SWITCH 8

#define LR11XX_RSSI_CALIBRATION_TUNE_LENGTH 17

static void lr11xx_get_tx_cfg( const void* context, lr11xx_pa_type_t pa_type, int8_t expected_output_pwr_in_dbm,
                               ral_lr11xx_bsp_tx_cfg_output_params_t* output_params )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr11xx_hal_context_cfg_t* config = dev->config;

    int8_t power = expected_output_pwr_in_dbm;

    /* Ramp time is the same for any config */
    output_params->pa_ramp_time = config->pa_ramp_time;

    switch( pa_type )
    {
    case LR11XX_WITH_LF_LP_PA:
    {
        /* Check power boundaries for LP LF PA:
         * The output power must be in range [ -17 , +15 ] dBm
         */
        power                        = CLAMP( power, LR11XX_LF_LP_MIN_OUTPUT_POWER, LR11XX_LF_LP_MAX_OUTPUT_POWER );
        lr11xx_pa_pwr_cfg_t* pwr_cfg = &config->pa_lf_lp_cfg_table[power - LR11XX_LF_LP_MIN_OUTPUT_POWER];

        output_params->pa_cfg.pa_sel                     = LR11XX_RADIO_PA_SEL_LP;
        output_params->pa_cfg.pa_reg_supply              = LR11XX_RADIO_PA_REG_SUPPLY_VREG;
        output_params->pa_cfg.pa_duty_cycle              = pwr_cfg->pa_duty_cycle;
        output_params->pa_cfg.pa_hp_sel                  = pwr_cfg->pa_hp_sel;
        output_params->chip_output_pwr_in_dbm_configured = pwr_cfg->power;
        output_params->chip_output_pwr_in_dbm_expected   = power;
        break;
    }
    case LR11XX_WITH_LF_HP_PA:
    {
        /* Check power boundaries for HP LF PA:
         * The output power must be in range [ -9 , +22 ] dBm
         */
        power                        = CLAMP( power, LR11XX_LF_HP_MIN_OUTPUT_POWER, LR11XX_LF_HP_MAX_OUTPUT_POWER );
        lr11xx_pa_pwr_cfg_t* pwr_cfg = &config->pa_lf_hp_cfg_table[power - LR11XX_LF_HP_MIN_OUTPUT_POWER];

        output_params->pa_cfg.pa_sel = LR11XX_RADIO_PA_SEL_HP;

        /* For powers below 8dBm use regulated supply for
         * HP PA for a better efficiency.
         */
        output_params->pa_cfg.pa_reg_supply              = ( power <= LR11XX_PWR_VREG_VBAT_SWITCH )
                                                               ? LR11XX_RADIO_PA_REG_SUPPLY_VREG
                                                               : LR11XX_RADIO_PA_REG_SUPPLY_VBAT;
        output_params->pa_cfg.pa_duty_cycle              = pwr_cfg->pa_duty_cycle;
        output_params->pa_cfg.pa_hp_sel                  = pwr_cfg->pa_hp_sel;
        output_params->chip_output_pwr_in_dbm_configured = pwr_cfg->power;
        output_params->chip_output_pwr_in_dbm_expected   = power;
        break;
    }
    case LR11XX_WITH_LF_LP_HP_PA:
    {
        /* Check power boundaries for LP/HP LF PA:
         * The output power must be in range [ -17 , +22 ] dBm
         */
        power = CLAMP( power, LR11XX_LF_LP_MIN_OUTPUT_POWER, LR11XX_LF_HP_MAX_OUTPUT_POWER );

        if( power <= LR11XX_LF_LP_MAX_OUTPUT_POWER )
        {
            lr11xx_pa_pwr_cfg_t* pwr_cfg = &config->pa_lf_lp_cfg_table[power - LR11XX_LF_LP_MIN_OUTPUT_POWER];

            output_params->chip_output_pwr_in_dbm_expected   = power;
            output_params->pa_cfg.pa_sel                     = LR11XX_RADIO_PA_SEL_LP;
            output_params->pa_cfg.pa_reg_supply              = LR11XX_RADIO_PA_REG_SUPPLY_VREG;
            output_params->pa_cfg.pa_duty_cycle              = pwr_cfg->pa_duty_cycle;
            output_params->pa_cfg.pa_hp_sel                  = pwr_cfg->pa_hp_sel;
            output_params->chip_output_pwr_in_dbm_configured = pwr_cfg->power;
        }
        else
        {
            lr11xx_pa_pwr_cfg_t* pwr_cfg = &config->pa_lf_hp_cfg_table[power - LR11XX_LF_HP_MIN_OUTPUT_POWER];

            output_params->chip_output_pwr_in_dbm_expected   = power;
            output_params->pa_cfg.pa_sel                     = LR11XX_RADIO_PA_SEL_HP;
            output_params->pa_cfg.pa_reg_supply              = LR11XX_RADIO_PA_REG_SUPPLY_VBAT;
            output_params->pa_cfg.pa_duty_cycle              = pwr_cfg->pa_duty_cycle;
            output_params->pa_cfg.pa_hp_sel                  = pwr_cfg->pa_hp_sel;
            output_params->chip_output_pwr_in_dbm_configured = pwr_cfg->power;
        }
        break;
    }
    case LR11XX_WITH_HF_PA:
    {
        /* Check power boundaries for HF PA:
         * The output power must be in range [ -18 , +13 ] dBm
         */
        power                        = CLAMP( power, LR11XX_HF_MIN_OUTPUT_POWER, LR11XX_HF_MAX_OUTPUT_POWER );
        lr11xx_pa_pwr_cfg_t* pwr_cfg = &config->pa_hf_cfg_table[power - LR11XX_HF_MIN_OUTPUT_POWER];

        output_params->pa_cfg.pa_sel                     = LR11XX_RADIO_PA_SEL_HF;
        output_params->pa_cfg.pa_reg_supply              = LR11XX_RADIO_PA_REG_SUPPLY_VREG;
        output_params->pa_cfg.pa_duty_cycle              = pwr_cfg->pa_duty_cycle;
        output_params->pa_cfg.pa_hp_sel                  = pwr_cfg->pa_hp_sel;
        output_params->chip_output_pwr_in_dbm_configured = pwr_cfg->power;
        output_params->chip_output_pwr_in_dbm_expected   = power;
        break;
    }
    }
}

void ral_lr11xx_bsp_get_tx_cfg( const void* context, const ral_lr11xx_bsp_tx_cfg_input_params_t* input_params,
                                ral_lr11xx_bsp_tx_cfg_output_params_t* output_params )
{
    /* get board tx power offset */
    int8_t board_tx_pwr_offset_db = radio_utilities_get_tx_power_offset( context );

    int16_t power = input_params->system_output_pwr_in_dbm + board_tx_pwr_offset_db;

    lr11xx_pa_type_t pa_type;

    /* check frequency band first to choose LF of HF PA */
    if( input_params->freq_in_hz >= 1600000000 )
    {
        pa_type = LR11XX_WITH_HF_PA;
    }
    else
    {
        /* Modem is acting in subgig band: use LP/HP PA
         * (both LP and HP are connected on lr11xx evk board)
         */
        pa_type = LR11XX_WITH_LF_LP_HP_PA;
    }

    /* call the configuration function */
    lr11xx_get_tx_cfg( context, pa_type, power, output_params );
}

void ral_lr11xx_bsp_get_rssi_calibration_table( const void* context, const uint32_t freq_in_hz,
                                                lr11xx_radio_rssi_calibration_table_t* rssi_calibration_table )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr11xx_hal_context_cfg_t* config = dev->config;

    if( freq_in_hz <= 600000000 )
    {
        *rssi_calibration_table = config->rssi_calibration_table_below_600mhz;
    }
    else if( ( 600000000 <= freq_in_hz ) && ( freq_in_hz <= 2000000000 ) )
    {
        *rssi_calibration_table = config->rssi_calibration_table_from_600mhz_to_2ghz;
    }
    else
    {
        /* freq_in_hz > 2000000000 */
        *rssi_calibration_table = config->rssi_calibration_table_above_2ghz;
    }
}

ral_status_t ral_lr11xx_bsp_get_instantaneous_tx_power_consumption( const void* context,
                                                                    const ral_lr11xx_bsp_tx_cfg_output_params_t* tx_cfg,
                                                                    lr11xx_system_reg_mode_t radio_reg_mode,
                                                                    uint32_t*                pwr_consumption_in_ua )
{
    // Get Zephyr object from Device Tree
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr11xx_hal_context_cfg_t* config = dev->config;

    if( tx_cfg->pa_cfg.pa_sel == LR11XX_RADIO_PA_SEL_LP )
    {
        if( tx_cfg->pa_cfg.pa_reg_supply == LR11XX_RADIO_PA_REG_SUPPLY_VREG )
        {
            uint8_t index = 0;

            if( tx_cfg->chip_output_pwr_in_dbm_expected > LR11XX_LF_LP_MAX_OUTPUT_POWER )
            {
                index = LR11XX_LF_LP_MAX_OUTPUT_POWER + LR11XX_LP_CONVERT_TABLE_INDEX_OFFSET;
            }
            else if( tx_cfg->chip_output_pwr_in_dbm_expected < LR11XX_LF_LP_MIN_OUTPUT_POWER )
            {
                index = LR11XX_LF_LP_MIN_OUTPUT_POWER + LR11XX_LP_CONVERT_TABLE_INDEX_OFFSET;
            }
            else
            {
                index = tx_cfg->chip_output_pwr_in_dbm_expected + LR11XX_LP_CONVERT_TABLE_INDEX_OFFSET;
            }

            if( radio_reg_mode == LR11XX_SYSTEM_REG_MODE_DCDC )
            {
                // *pwr_consumption_in_ua = ral_lr11xx_convert_tx_dbm_to_ua_reg_mode_dcdc_lp_vreg[index];
                *pwr_consumption_in_ua = config->tx_dbm_to_ua_reg_mode_dcdc_lf_lp_vreg[index];
            }
            else
            {
                // *pwr_consumption_in_ua = ral_lr11xx_convert_tx_dbm_to_ua_reg_mode_ldo_lp_vreg[index];
                *pwr_consumption_in_ua = config->tx_dbm_to_ua_reg_mode_ldo_lf_lp_vreg[index];
            }
        }
        else
        {
            return RAL_STATUS_UNSUPPORTED_FEATURE;
        }
    }
    else if( tx_cfg->pa_cfg.pa_sel == LR11XX_RADIO_PA_SEL_HP )
    {
        if( tx_cfg->pa_cfg.pa_reg_supply == LR11XX_RADIO_PA_REG_SUPPLY_VBAT )
        {
            uint8_t index = 0;

            if( tx_cfg->chip_output_pwr_in_dbm_expected > LR11XX_LF_HP_MAX_OUTPUT_POWER )
            {
                index = LR11XX_LF_HP_MAX_OUTPUT_POWER + LR11XX_HP_CONVERT_TABLE_INDEX_OFFSET;
            }
            else if( tx_cfg->chip_output_pwr_in_dbm_expected < LR11XX_LF_HP_MIN_OUTPUT_POWER )
            {
                index = LR11XX_LF_HP_MIN_OUTPUT_POWER + LR11XX_HP_CONVERT_TABLE_INDEX_OFFSET;
            }
            else
            {
                index = tx_cfg->chip_output_pwr_in_dbm_expected + LR11XX_HP_CONVERT_TABLE_INDEX_OFFSET;
            }

            if( radio_reg_mode == LR11XX_SYSTEM_REG_MODE_DCDC )
            {
                // *pwr_consumption_in_ua = ral_lr11xx_convert_tx_dbm_to_ua_reg_mode_dcdc_hp_vbat[index];
                *pwr_consumption_in_ua = config->tx_dbm_to_ua_reg_mode_dcdc_lf_hp_vbat[index];
            }
            else
            {
                // *pwr_consumption_in_ua = ral_lr11xx_convert_tx_dbm_to_ua_reg_mode_ldo_hp_vbat[index];
                *pwr_consumption_in_ua = config->tx_dbm_to_ua_reg_mode_ldo_lf_hp_vbat[index];
            }
        }
        else
        {
            return RAL_STATUS_UNSUPPORTED_FEATURE;
        }
    }
    else if( tx_cfg->pa_cfg.pa_sel == LR11XX_RADIO_PA_SEL_HF )
    {
        if( tx_cfg->pa_cfg.pa_reg_supply == LR11XX_RADIO_PA_REG_SUPPLY_VREG )
        {
            uint8_t index = 0;

            if( tx_cfg->chip_output_pwr_in_dbm_expected > LR11XX_HF_MAX_OUTPUT_POWER )
            {
                index = LR11XX_HF_MAX_OUTPUT_POWER + LR11XX_HF_CONVERT_TABLE_INDEX_OFFSET;
            }
            else if( tx_cfg->chip_output_pwr_in_dbm_expected < LR11XX_HF_MIN_OUTPUT_POWER )
            {
                index = LR11XX_HF_MIN_OUTPUT_POWER + LR11XX_HF_CONVERT_TABLE_INDEX_OFFSET;
            }
            else
            {
                index = tx_cfg->chip_output_pwr_in_dbm_expected + LR11XX_HF_CONVERT_TABLE_INDEX_OFFSET;
            }

            if( radio_reg_mode == LR11XX_SYSTEM_REG_MODE_DCDC )
            {
                // *pwr_consumption_in_ua = ral_lr11xx_convert_tx_dbm_to_ua_reg_mode_dcdc_hf_vreg[index];
                *pwr_consumption_in_ua = config->tx_dbm_to_ua_reg_mode_dcdc_hf_vreg[index];
            }
            else
            {
                return RAL_STATUS_UNSUPPORTED_FEATURE;
            }
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

ral_status_t ral_lr11xx_bsp_get_instantaneous_gfsk_rx_power_consumption( const void*              context,
                                                                         lr11xx_system_reg_mode_t radio_reg_mode,
                                                                         bool                     rx_boosted,
                                                                         uint32_t* pwr_consumption_in_ua )
{
    if( radio_reg_mode == LR11XX_SYSTEM_REG_MODE_DCDC )
    {
        *pwr_consumption_in_ua = rx_boosted ? LR11XX_GFSK_RX_BOOSTED_CONSUMPTION_DCDC : LR11XX_GFSK_RX_CONSUMPTION_DCDC;
    }
    else
    {
        *pwr_consumption_in_ua = rx_boosted ? LR11XX_GFSK_RX_BOOSTED_CONSUMPTION_LDO : LR11XX_GFSK_RX_CONSUMPTION_LDO;
    }

    return RAL_STATUS_OK;
}

ral_status_t ral_lr11xx_bsp_get_instantaneous_lora_rx_power_consumption( const void*              context,
                                                                         lr11xx_system_reg_mode_t radio_reg_mode,
                                                                         bool                     rx_boosted,
                                                                         uint32_t* pwr_consumption_in_ua )
{
    if( radio_reg_mode == LR11XX_SYSTEM_REG_MODE_DCDC )
    {
        *pwr_consumption_in_ua = rx_boosted ? LR11XX_LORA_RX_BOOSTED_CONSUMPTION_DCDC : LR11XX_LORA_RX_CONSUMPTION_DCDC;
    }
    else
    {
        *pwr_consumption_in_ua = rx_boosted ? LR11XX_LORA_RX_BOOSTED_CONSUMPTION_LDO : LR11XX_LORA_RX_CONSUMPTION_LDO;
    }

    return RAL_STATUS_OK;
}
