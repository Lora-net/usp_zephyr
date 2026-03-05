/**
 * @file      sx126x_ral_bsp_stm32wl.c
 *
 * @brief     RAL BSP implementation for the STM32WL internal SX126x radio.
 *
 * This file replaces sx126x_ral_bsp.c when CONFIG_SEMTECH_SX126X_STM32WL is
 * set. The main difference from the generic version is the TX config function,
 * which uses the LP PA parameters required by the STM32WL LP path (same PA
 * config as SX1261).
 *
 * Copyright (c) 2026 Semtech Corporation
 * SPDX-License-Identifier: Apache-2.0
 * Created by Charles-Henri Hallard (charles@evolyenergy.com)
 */

#include <zephyr/kernel.h>

#include <zephyr/usp/lora_lbm_transceiver.h>

#include <sx126x.h>
#include <ral_sx126x_bsp.h>
#include "sx126x_stm32wl_hal_context.h"

/* ---------------------------------------------------------------------------
 * Helper: access the device config
 * ---------------------------------------------------------------------------
 */

static inline const struct sx126x_stm32wl_hal_context_cfg_t* get_cfg( const void* context )
{
    const struct device* dev = context;
    return dev->config;
}

static inline struct sx126x_stm32wl_hal_context_data_t* get_data( const void* context )
{
    const struct device* dev = context;
    return dev->data;
}

/* ---------------------------------------------------------------------------
 * RAL BSP API
 * ---------------------------------------------------------------------------
 */

void ral_sx126x_bsp_get_reg_mode( const void* context, sx126x_reg_mod_t* reg_mode )
{
    *reg_mode = get_cfg( context )->reg_mode;
}

void ral_sx126x_bsp_get_rf_switch_cfg( const void* context, bool* dio2_is_set_as_rf_switch )
{
    *dio2_is_set_as_rf_switch = get_cfg( context )->dio2_tx_enable;
}

void ral_sx126x_bsp_get_tx_cfg( const void* context, const ral_sx126x_bsp_tx_cfg_input_params_t* input_params,
                                ral_sx126x_bsp_tx_cfg_output_params_t* output_params )
{
    const struct sx126x_stm32wl_hal_context_cfg_t* config = get_cfg( context );

    int8_t board_tx_pwr_offset_db = radio_utilities_get_tx_power_offset( context );
    int16_t power = ( int16_t ) input_params->system_output_pwr_in_dbm + board_tx_pwr_offset_db;

    output_params->pa_ramp_time = config->pa_ramp_time;
    /* reserved value, same for all SX126x variants */
    output_params->pa_cfg.pa_lut = 0x01;

    if( config->pa_is_lp )
    {
        /* STM32WL LP PA — same register layout as SX1261 LP path.
         * Maximum output power is board-configured (typically 14 dBm).
         * PA settings: pa_duty_cycle=0x04, hp_max=0x00, device_sel=0x01. */
        const int8_t max_power = config->pa_lp_max_power_dbm;

        if( power > max_power )
        {
            power = max_power;
        }
        if( power < -17 )
        {
            power = -17;
        }

        output_params->pa_cfg.device_sel    = 0x01; /* LP PA */
        output_params->pa_cfg.hp_max        = 0x00; /* not used on LP path */
        output_params->pa_cfg.pa_duty_cycle = 0x04;
        output_params->chip_output_pwr_in_dbm_configured = ( int8_t ) power;
        output_params->chip_output_pwr_in_dbm_expected   = ( int8_t ) power;
    }
    else
    {
        /* STM32WL HP PA — register layout mirrors SX1262 HP path.
         * Maximum output power is board-configured (rfo-hp-max-power). */
        const int8_t max_power = config->pa_hp_max_power_dbm;

        if( power > max_power )
        {
            power = max_power;
        }
        if( power < -9 )
        {
            power = -9;
        }

        /* Apply the STM32WL erratum workaround for RFO_HP antenna mismatch:
         * set REG_TX_CLAMP_CFG bits [4:1] before any HP transmission.
         * This is handled at the SX126x register level by the LBM — nothing
         * extra is needed here in the BSP. */

        output_params->pa_cfg.device_sel    = 0x00; /* HP PA */
        output_params->pa_cfg.hp_max        = 0x07; /* max HP setting */
        output_params->pa_cfg.pa_duty_cycle = 0x04;
        output_params->chip_output_pwr_in_dbm_configured = ( int8_t ) power;
        output_params->chip_output_pwr_in_dbm_expected   = ( int8_t ) power;
    }
}

void ral_sx126x_bsp_get_xosc_cfg( const void* context, ral_xosc_cfg_t* xosc_cfg,
                                  sx126x_tcxo_ctrl_voltages_t* supply_voltage,
                                  uint32_t* startup_time_in_tick )
{
    const struct sx126x_hal_context_tcxo_cfg_t tcxo = get_cfg( context )->tcxo_cfg;

    *xosc_cfg             = tcxo.xosc_cfg;
    *supply_voltage       = tcxo.voltage;
    *startup_time_in_tick = sx126x_convert_timeout_in_ms_to_rtc_step( tcxo.wakeup_time_ms );
}

void ral_sx126x_bsp_get_trim_cap( const void* context, uint8_t* trimming_cap_xta, uint8_t* trimming_cap_xtb )
{
    const struct sx126x_stm32wl_hal_context_cfg_t* config = get_cfg( context );

    if( config->capa_xta != 0xFF )
    {
        *trimming_cap_xta = config->capa_xta;
    }
    if( config->capa_xtb != 0xFF )
    {
        *trimming_cap_xtb = config->capa_xtb;
    }
}

void ral_sx126x_bsp_get_rx_boost_cfg( const void* context, bool* rx_boost_is_activated )
{
    *rx_boost_is_activated = get_cfg( context )->rx_boosted;
}

void ral_sx126x_bsp_get_ocp_value( const void* context, uint8_t* ocp_in_step_of_2_5_ma )
{
    /* Let the driver choose default values.
     * LP PA default OCP (0x18 = 60 mA) is set by the LBM internally. */
}

void ral_sx126x_bsp_get_lora_cad_det_peak( const void* context, ral_lora_sf_t sf, ral_lora_bw_t bw,
                                           ral_lora_cad_symbs_t nb_symbol, uint8_t* in_out_cad_det_peak )
{
    /* Fine-tune the CAD detection peak if needed */
}

/* ---------------------------------------------------------------------------
 * TX power offset helpers
 * ---------------------------------------------------------------------------
 */

void radio_utilities_set_tx_power_offset( const void* context, uint8_t tx_pwr_offset_db )
{
    get_data( context )->tx_power_offset_db_current = ( int8_t ) tx_pwr_offset_db;
}

uint8_t radio_utilities_get_tx_power_offset( const void* context )
{
    return ( uint8_t ) get_data( context )->tx_power_offset_db_current;
}

/* ---------------------------------------------------------------------------
 * Power consumption tables (LP PA, LDO regulator mode — STM32WL typical)
 * ---------------------------------------------------------------------------
 */

/* Values from SX1261 datasheet at 3.3 V / LDO */
#define SX126X_LP_MIN_OUTPUT_POWER          -17
#define SX126X_LP_MAX_OUTPUT_POWER           15
#define SX126X_LP_CONVERT_TABLE_INDEX_OFFSET 17

#define SX126X_GFSK_RX_CONSUMPTION_LDO         8000
#define SX126X_GFSK_RX_BOOSTED_CONSUMPTION_LDO 9300
#define SX126X_LORA_RX_CONSUMPTION_LDO         8880
#define SX126X_LORA_RX_BOOSTED_CONSUMPTION_LDO 10100

static const uint32_t ral_sx126x_convert_tx_dbm_to_ua_ldo_lp[] = {
    9800,  /* -17 dBm */
    10300, /* -16 dBm */
    10500, /* -15 dBm */
    10800, /* -14 dBm */
    11100, /* -13 dBm */
    11300, /* -12 dBm */
    11600, /* -11 dBm */
    11900, /* -10 dBm */
    12400, /*  -9 dBm */
    12900, /*  -8 dBm */
    13400, /*  -7 dBm */
    13900, /*  -6 dBm */
    14500, /*  -5 dBm */
    15300, /*  -4 dBm */
    16000, /*  -3 dBm */
    17000, /*  -2 dBm */
    18000, /*  -1 dBm */
    19000, /*   0 dBm */
    20600, /*   1 dBm */
    22000, /*   2 dBm */
    23500, /*   3 dBm */
    24900, /*   4 dBm */
    26600, /*   5 dBm */
    28400, /*   6 dBm */
    30200, /*   7 dBm */
    32000, /*   8 dBm */
    34300, /*   9 dBm */
    36600, /*  10 dBm */
    39200, /*  11 dBm */
    41700, /*  12 dBm */
    44700, /*  13 dBm */
    48200, /*  14 dBm */
    52200, /*  15 dBm */
};

__weak ral_status_t ral_sx126x_bsp_get_instantaneous_tx_power_consumption(
    const void* context, const ral_sx126x_bsp_tx_cfg_output_params_t* tx_cfg_output_params,
    sx126x_reg_mod_t radio_reg_mode, uint32_t* pwr_consumption_in_ua )
{
    uint8_t index = 0;
    int8_t  pwr   = tx_cfg_output_params->chip_output_pwr_in_dbm_expected;

    if( pwr > SX126X_LP_MAX_OUTPUT_POWER )
    {
        index = SX126X_LP_MAX_OUTPUT_POWER + SX126X_LP_CONVERT_TABLE_INDEX_OFFSET;
    }
    else if( pwr < SX126X_LP_MIN_OUTPUT_POWER )
    {
        index = 0;
    }
    else
    {
        index = ( uint8_t )( pwr + SX126X_LP_CONVERT_TABLE_INDEX_OFFSET );
    }

    *pwr_consumption_in_ua = ral_sx126x_convert_tx_dbm_to_ua_ldo_lp[index];
    return RAL_STATUS_OK;
}

__weak ral_status_t ral_sx126x_bsp_get_instantaneous_gfsk_rx_power_consumption(
    const void* context, sx126x_reg_mod_t radio_reg_mode, bool rx_boosted,
    uint32_t* pwr_consumption_in_ua )
{
    *pwr_consumption_in_ua = rx_boosted
        ? SX126X_GFSK_RX_BOOSTED_CONSUMPTION_LDO
        : SX126X_GFSK_RX_CONSUMPTION_LDO;
    return RAL_STATUS_OK;
}

__weak ral_status_t ral_sx126x_bsp_get_instantaneous_lora_rx_power_consumption(
    const void* context, sx126x_reg_mod_t radio_reg_mode, bool rx_boosted,
    uint32_t* pwr_consumption_in_ua )
{
    *pwr_consumption_in_ua = rx_boosted
        ? SX126X_LORA_RX_BOOSTED_CONSUMPTION_LDO
        : SX126X_LORA_RX_CONSUMPTION_LDO;
    return RAL_STATUS_OK;
}
