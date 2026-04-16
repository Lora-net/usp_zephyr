/*!
 * \file      main_flrc_burst.c
 *
 * \brief     main program for FLRC burst example
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2021. All rights reserved.
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

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */
#include <stdio.h>
#include <stdint.h>    // C99 types
#include <inttypes.h>  // PRIu32
#include <stdbool.h>   // bool type
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#define SMTC_HAL_DBG_TRACE_C
#include <smtc_hal_dbg_trace.h>
#include <smtc_sw_platform_helper.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include <smtc_modem_hal.h>

#include <smtc_rac_api.h>

#include "apps_configuration.h"
#include "flrc_burst_rx.h"
#include "flrc_burst_tx.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

LOG_MODULE_REGISTER( flrc_burst, LOG_LEVEL_INF );

#define RECEIVER 1
#define TRANSMITTER 2

#if( ROLE == RECEIVER )

#elif( ROLE == TRANSMITTER )

#else
#error "Please define ROLE as either RECEIVER or TRANSMITTER"
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/**
 * @brief Watchdog counter reload value during sleep (The period must be lower than MCU watchdog period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS 20000

#if( ROLE == TRANSMITTER )
#define DATA_SIZE ( 20 * 1024 )
#else
#define DATA_SIZE ( 20 * 1024 )
#endif

/**
 * @brief Blue user button or BUTTON 1 on nrf52840-dk
 */

#define USER_BUTTON_NODE DT_ALIAS( smtc_user_button )
#if !DT_NODE_HAS_STATUS( USER_BUTTON_NODE, okay )
#error "Unsupported board: smtc-user-button devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR( USER_BUTTON_NODE, gpios, { 0 } );
static struct gpio_callback      button_cb_data;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

typedef struct user_button_s
{
    bool     is_pressed;
    uint32_t last_press_timestamp;
} user_button_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */
static const uint32_t BUTTON_TRIGGER_DELAY = 500;  // ms

static user_button_t user_button = {
    .is_pressed           = false,
    .last_press_timestamp = 0,
};

#if( ROLE == TRANSMITTER )
static const uint32_t SLEEP_DELAY     = 4000;  // ms
static uint8_t        data_flrc_count = 0;
static uint32_t       tx_timestamp_ms = 0;
#endif

static uint8_t data[DATA_SIZE];  // TX (data to send) or RX (data received)

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

#if( ROLE == RECEIVER )
static void data_received_cb( void );
#endif

#if( ROLE == TRANSMITTER )
static void data_sent_cb( bool is_successful );
#endif

static int  configure_user_button( void );
static void user_button_callback( const void* context );
void        button_pressed( const struct device* dev, struct gpio_callback* cb, uint32_t pins );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/**
 * @brief Example to send data using the Burst FLRC protocol
 *
 */
int main( void )
{
    if( configure_user_button( ) != 0 )
    {
        SMTC_HAL_TRACE_ERROR( "Issue when configuring user button, aborting\n" );
        return 1;
    }

    SMTC_HAL_TRACE_INFO( "===== flrc burst example =====\r\n" );
    SMTC_SW_PLATFORM_INIT( );
    SMTC_SW_PLATFORM_VOID( smtc_rac_init( ) );

    // initialize LEDs
    init_leds( );
    set_led( SMTC_PF_LED_TX, false );
    set_led( SMTC_PF_LED_RX, false );

#if( ROLE == RECEIVER )
    flrc_burst_protocol_rx_init( data, sizeof( data ), data_received_cb );
#elif( ROLE == TRANSMITTER )
    flrc_burst_protocol_tx_init( data_sent_cb );
#endif

    while( true )
    {
        smtc_rac_run_engine( );

#if( ROLE == TRANSMITTER )
        if( ( user_button.is_pressed == true ) ||
            ( ( smtc_modem_hal_get_time_in_ms( ) - tx_timestamp_ms ) > SLEEP_DELAY ) )
        {
            tx_timestamp_ms = smtc_modem_hal_get_time_in_ms( );

            SMTC_HAL_TRACE_INFO( "button pressed or timer\n" );

            if( flrc_burst_protocol_tx_is_ready( ) )
            {
                // Prepare new data to send
                for( uint32_t i = 0; i < DATA_SIZE; i++ )
                {
                    if( i % 128 == 0 )
                    {
                        data_flrc_count++;
                    }
                    data[i] = data_flrc_count;
                }
                user_button.is_pressed = false;

                flrc_burst_protocol_tx_start_new_transaction( data, sizeof( data ) );
            }
        }
#endif
        // k_yield( );
        // k_sleep( K_MSEC( 1 ) );
    }
    return 0;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static void user_button_callback( const void* context )
{
    UNUSED( context );
    uint32_t timestamp = smtc_modem_hal_get_time_in_ms( );
    if( ( timestamp - user_button.last_press_timestamp ) > BUTTON_TRIGGER_DELAY )
    {
        user_button.last_press_timestamp = timestamp;
        user_button.is_pressed           = true;
    }
}

void button_pressed( const struct device* dev, struct gpio_callback* cb, uint32_t pins )
{
    printk( "%s", __func__ );
    user_button_callback( dev );
}

static int configure_user_button( void )
{
    int ret = 0;
    if( !gpio_is_ready_dt( &button ) )
    {
        printk( "Error: button device %s is not ready\n", button.port->name );
        return 1;
    }

    ret = gpio_pin_configure_dt( &button, GPIO_INPUT );
    if( ret != 0 )
    {
        printk( "Error %d: failed to configure %s pin %d\n", ret, button.port->name, button.pin );
        return 1;
    }

    ret = gpio_pin_interrupt_configure_dt( &button, GPIO_INT_EDGE_TO_ACTIVE );
    if( ret != 0 )
    {
        printk( "Error %d: failed to configure interrupt on %s pin %d\n", ret, button.port->name, button.pin );
        return 1;
    }

    gpio_init_callback( &button_cb_data, button_pressed, BIT( button.pin ) );
    gpio_add_callback( button.port, &button_cb_data );

    return 0;
}

#if( ROLE == RECEIVER )
static void data_received_cb( void )
{
    flrc_burst_rx_results_t results;
    flrc_burst_rx_get_results( &results );
    bool ok = results.rx_data_size == DATA_SIZE;
    SMTC_HAL_TRACE_INFO( "Data %s (%" PRIu32 "/ %d)", ( ok ? "OK" : "INCOMPLETE" ), results.rx_data_size, DATA_SIZE );
    flrc_burst_rx_start_listening( );
}
#endif

#if( ROLE == TRANSMITTER )
static void data_sent_cb( bool is_successful )
{
    if( is_successful )
    {
        SMTC_HAL_TRACE_INFO( "Data sent" );
    }
    else
    {
        SMTC_HAL_TRACE_INFO( "Failed to sent data" );
    }
}
#endif

/* --- EOF ------------------------------------------------------------------ */
