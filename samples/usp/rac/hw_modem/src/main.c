/**
 * @file      main.c
 *
 * @brief     Application main
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

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/logging/log.h>

#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include <smtc_modem_utilities.h>

#include <smtc_rac_api.h>

#include "hw_modem.h"
#include "cmd_parser.h"
#include "git_version.h"

LOG_MODULE_REGISTER( hw_modem, 3 );

/**
 * @brief Watchdog counter reload value during sleep (The period must be lower than MCU watchdog
 * period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS 20000

/* lr11xx radio context and its use in the ralf layer */
static const struct device* transceiver = DEVICE_DT_GET( DT_ALIAS( lora_transceiver ) );

/**
 * @brief Callback for modem hal
 */
static uint8_t prv_get_battery_level_cb( void )
{
    return 98;
}

/**
 * @brief Callback for modem hal
 */
static uint16_t prv_get_battery_voltage_cb( void )
{
    return 3300;
}

/**
 * @brief Callback for modem hal
 */
static int8_t prv_get_temperature_cb( void )
{
    return 25;
}

#ifdef CONFIG_LORA_BASICS_MODEM_FUOTA
static uint32_t prv_get_hw_version_for_fuota( void )
{
    return 1;
}

static uint32_t prv_get_fw_version_for_fuota( void )
{
    return 1;
}

static uint8_t prv_get_fw_status_available_for_fuota( void )
{
    return 1;
}

static uint32_t prv_get_next_fw_version_for_fuota( void )
{
    return 1;
}

static uint8_t prv_get_fw_delete_status_for_fuota( uint32_t version )
{
    return 0;
}

/* Callbacks for HAL implementation */
static struct lorawan_fuota_cb prv_fuota_cb = {
    .get_hw_version          = prv_get_hw_version_for_fuota,
    .get_fw_version          = prv_get_fw_version_for_fuota,
    .get_fw_status_available = prv_get_fw_status_available_for_fuota,
    .get_next_fw_version     = prv_get_next_fw_version_for_fuota,
    .get_fw_delete_status    = prv_get_fw_delete_status_for_fuota,
};

#endif /* CONFIG_LORA_BASICS_MODEM_FUOTA */

void main_hw_modem( void );

int main( void )
{
    unsigned int irq_lock_key;
    uint32_t     sleep_time_ms = 0;
    bool         is_sleep_ok;

    lorawan_smtc_modem_hal_init( transceiver );
    lorawan_register_battery_level_callback( prv_get_battery_level_cb );
    lorawan_register_battery_voltage_callback( prv_get_battery_voltage_cb );
    lorawan_register_temperature_callback( prv_get_temperature_cb );
#ifdef CONFIG_LORA_BASICS_MODEM_FUOTA
    lorawan_register_fuota_callbacks( &prv_fuota_cb );
#endif

    cmd_parser_set_transceiver_context( ( void* ) transceiver );

    /* NOTE: watchdogs aren't fully implemented in those samples. */
    /* hal_watchdog_init(); */

    // Initialize RAC
    smtc_rac_init();

    if( hw_modem_init( ) )
    {
        LOG_ERR( "Could not initialize hardware modem, exiting." );
        return 0;
    }

    LOG_INF( "Modem is starting" );
    LOG_DBG( "Commit SHA1: %s", get_software_git_commit( ) );
    LOG_DBG( "Commit date: %s", get_software_git_date( ) );
    LOG_DBG( "Build date: %s", get_software_build_date( ) );

    while( 1 )
    {
        /* Check if a command is available */
        if( hw_modem_is_a_cmd_available( ) == true )
        {
            /* Command may generate work for the stack, so drop down to
             * smtc_modem_run_engine().
             */
            hw_modem_process_cmd( );
        }

        /* Modem process launch */
        sleep_time_ms = smtc_modem_run_engine( );

        /* Check sleep conditions (no command available and low power is possible) */
        irq_lock_key = irq_lock( );
        is_sleep_ok  = ( hw_modem_is_a_cmd_available( ) == false ) && ( hw_modem_is_low_power_ok( ) == true ) &&
                      ( smtc_modem_is_irq_flag_pending( ) == false );
        irq_unlock( irq_lock_key );

        if( is_sleep_ok )
        {
            uint32_t real_sleep_time_ms = MIN( sleep_time_ms, WATCHDOG_RELOAD_PERIOD_MS );

            LOG_DBG( "Sleeping for %d ms", real_sleep_time_ms );
            smtc_modem_hal_interruptible_msleep( K_MSEC( real_sleep_time_ms ) );
        }
    }
    return 0;
}
