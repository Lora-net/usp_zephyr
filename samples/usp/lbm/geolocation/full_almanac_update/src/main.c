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

#include <time.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>

#include <smtc_modem_api.h>
#include <smtc_modem_utilities.h>
#include <smtc_modem_hal.h>
#include <smtc_modem_geolocation_api.h>

#include <smtc_sw_platform_helper.h>
#define SMTC_HAL_DBG_TRACE_C
#include <smtc_hal_dbg_trace.h>
#include <smtc_rac_api.h>

#include "app_full_almanac_update.h"

LOG_MODULE_REGISTER( full_almanac_update, LOG_LEVEL_INF );

/**
 * @brief Helper macro that returned a human-friendly message if a command does not return
 * SMTC_MODEM_RC_OK
 *
 * @remark The macro is implemented to be used with functions returning a @ref
 * smtc_modem_return_code_t
 *
 * @param[in] rc  Return code
 */

void assert_smtc_modem_rc( const char* file, const char* func, int line, smtc_modem_return_code_t rc )
{
    char* rc_msg = "";
    int   level  = -1;

    if( rc == SMTC_MODEM_RC_NOT_INIT )
    {
        rc_msg = STRINGIFY( SMTC_MODEM_RC_NOT_INIT );
        level  = LOG_LEVEL_ERR;
    }
    else if( rc == SMTC_MODEM_RC_INVALID )
    {
        rc_msg = STRINGIFY( SMTC_MODEM_RC_INVALID );
        level  = LOG_LEVEL_ERR;
    }
    else if( rc == SMTC_MODEM_RC_BUSY )
    {
        rc_msg = STRINGIFY( SMTC_MODEM_RC_BUSY );
        level  = LOG_LEVEL_ERR;
    }
    else if( rc == SMTC_MODEM_RC_FAIL )
    {
        rc_msg = STRINGIFY( SMTC_MODEM_RC_FAIL );
        level  = LOG_LEVEL_ERR;
    }
    else if( rc == SMTC_MODEM_RC_INVALID_STACK_ID )
    {
        rc_msg = STRINGIFY( SMTC_MODEM_RC_INVALID_STACK_ID );
        level  = LOG_LEVEL_ERR;
    }
    else if( rc == SMTC_MODEM_RC_NO_TIME )
    {
        rc_msg = STRINGIFY( SMTC_MODEM_RC_NO_TIME );
        level  = LOG_LEVEL_WRN;
    }
    else if( rc == SMTC_MODEM_RC_NO_EVENT )
    {
        rc_msg = STRINGIFY( SMTC_MODEM_RC_NO_EVENT );
        level  = LOG_LEVEL_INF;
    }

    if( level == LOG_LEVEL_INF )
    {
        LOG_INF( "In %s - %s (line %d): %s", file, func, line, rc_msg );
    }
    else if( level == LOG_LEVEL_WRN )
    {
        LOG_WRN( "In %s - %s (line %d): %s", file, func, line, rc_msg );
    }
    else if( level == LOG_LEVEL_ERR )
    {
        LOG_ERR( "In %s - %s (line %d): %s", file, func, line, rc_msg );
    }
}

#define ASSERT_SMTC_MODEM_RC( rc_func ) assert_smtc_modem_rc( __FILE__, __func__, __LINE__, rc_func )

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

 /**
 * @brief Watchdog counter reload value during sleep (The period must be lower than MCU watchdog
 * period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS 20000

#define TIME_BUFFER_SIZE ( 80 )

#define OFFSET_BETWEEN_GPS_EPOCH_AND_UNIX_EPOCH ( 315964800 )

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static const struct gpio_dt_spec rx_led = GPIO_DT_SPEC_GET( DT_PATH( leds, lr11xx_rx_led ), gpios );
static const struct gpio_dt_spec tx_led = GPIO_DT_SPEC_GET( DT_PATH( leds, lr11xx_tx_led ), gpios );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

 /**
 * Stack id value (multistacks modem is not yet available)
 */
#define STACK_ID 0

/**
 * @brief User callback for modem event
 *
 *  This callback is called every time an event ( see smtc_modem_event_t ) appears in the modem.
 *  Several events may have to be read from the modem when this callback is called.
 */
static void modem_event_callback( void );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */


/**
 * @brief Example to flash full almanac to LR11XX
 *
 */
int main( void )
{
    gpio_pin_configure_dt( &rx_led, GPIO_OUTPUT_INACTIVE );
    gpio_pin_configure_dt( &tx_led, GPIO_OUTPUT_INACTIVE );

    LOG_INF( "Full almanac flasher example" );

    SMTC_SW_PLATFORM_INIT( );
    SMTC_SW_PLATFORM_VOID( smtc_rac_init( ) );
    SMTC_SW_PLATFORM_VOID( smtc_modem_init( &modem_event_callback ) );
    
    while( true )
    {
        uint32_t sleep_time_ms = smtc_modem_run_engine( );
        smtc_rac_run_engine( );
        if( smtc_rac_is_irq_flag_pending( ) )
        {
            continue;
        }
        k_sem_take( smtc_modem_hal_get_event_sem(), K_MSEC( MIN( sleep_time_ms, WATCHDOG_RELOAD_PERIOD_MS ) ) );
    }
    
    return 0;
}


//  WARNING : Run in USP/RAC Thread if compiled with threads
static void modem_event_callback( void )
{
    LOG_DBG( "Modem event callback" );

    smtc_modem_event_t      current_event = { 0 };
    uint8_t                 stack_id = STACK_ID;
    uint8_t                 event_pending_count;

    /* Continue to read modem event until all event has been processed */
    do
    {
        /* Read modem event */
        ASSERT_SMTC_MODEM_RC( smtc_modem_get_event( &current_event, &event_pending_count ) );

        switch( current_event.event_type )
        {
        case SMTC_MODEM_EVENT_RESET:
            SMTC_HAL_TRACE_INFO( "Event received: RESET\n" );
            if( check_lr11xx_fw_version( transceiver ) != true )
            {
                SMTC_HAL_TRACE_ERROR( "LR11xx firmware version is not compatible with this example\n" );
                break;
            }
            almanac_full_update( stack_id );
            
            break;

        default:
            LOG_ERR( "Unknown event %u", current_event.event_type );
            break;
        }
    } while( event_pending_count > 0 );
}

/* --- EOF ------------------------------------------------------------------ */
