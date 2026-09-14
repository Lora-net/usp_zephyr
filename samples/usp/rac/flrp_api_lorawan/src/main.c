/*!
 * \file      main.c
 *
 * \brief     main program for FLRP API example
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
#include <stdint.h>   // C99 types
#include <stdbool.h>  // bool type

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#define SMTC_HAL_DBG_TRACE_C
#include <smtc_hal_dbg_trace.h>
#include <smtc_sw_platform_helper.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include <zephyr/usp/smtc_zephyr_log_custom.h>
#include <smtc_modem_hal.h>
#include <smtc_modem_helper.h>

#include <smtc_rac_api.h>
#include <smtc_flrp_api.h>
#include "flrp_api.h"
#include "flrp_api_menu.h"
#include "main.h"
#include "flrp_remote.h"
#include "flrp_display.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

LOG_MODULE_REGISTER( flrp_api_lorawan, LOG_LEVEL_INF );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/**
 * @brief Watchdog counter reload value during sleep (The period must be lower than MCU watchdog period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS 20000

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
    uint32_t last_press_timestamp;
} user_button_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */
static const uint32_t BUTTON_TRIGGER_DELAY = 500;  // ms

static user_button_t user_button = {
    .last_press_timestamp = 0,
};

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

static int  configure_user_button( void );
static void user_button_callback( const void* context );
void        button_pressed( const struct device* dev, struct gpio_callback* cb, uint32_t pins );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/* An event to notify the main LBM loop */
K_EVENT_DEFINE( flrp_api_main_loop_event );

/**
 * @brief Example to send data using the Burst FLRP protocol
 *
 */
int main( void )
{
    flrp_api_event_t event = FLRP_API_EVENT_NONE;

    custom_log_init( 0 );
    flrp_display_init( );
    if( configure_user_button( ) != 0 )
    {
        SMTC_HAL_TRACE_ERROR( "Issue when configuring user button, aborting\n" );
        return 1;
    }

    SMTC_HAL_TRACE_INFO( "===== flrp api example  =====\r\n" );
    SMTC_SW_PLATFORM_INIT( );
    SMTC_SW_PLATFORM_VOID( smtc_rac_init( ) );
#if defined( CONFIG_FLRP_TEST_APPLY_DEFAULT_CONFIG )
    flrp_api_menu_apply_startup_test_defaults( );
#endif
#if defined( CONFIG_USP_LORA_BASICS_MODEM )
    SMTC_SW_PLATFORM_VOID( smtc_modem_init( &modem_event_callback ) );
    flrp_display_icon( 0, 2, ICON_RADIO_NOT_JOINED );
#endif

    // initialize LEDs
    init_leds( );
    set_led( SMTC_PF_LED_TX, false );
    set_led( SMTC_PF_LED_RX, false );

    while( true )
    {
#if !defined( CONFIG_USP_MAIN_THREAD )
        uint32_t sleep_time_ms = WATCHDOG_RELOAD_PERIOD_MS;
#if defined( CONFIG_USP_LORA_BASICS_MODEM )
        sleep_time_ms = smtc_modem_run_engine( );
#endif
        smtc_rac_run_engine( );
        smtc_flrp_run_engine( );
        if( smtc_rac_is_irq_flag_pending( ) )
        {
            continue;
        }
        // Allows waking up on radio event or push-button press
        if( smtc_flrp_call_run( ) == false )
        {
            struct k_sem* sems[] = { smtc_modem_hal_get_event_sem( ) };
            wait_on_sems_and_event( sems, 1, &flrp_api_main_loop_event, 0xFFFFFFFF,
                                    K_MSEC( MIN( sleep_time_ms, WATCHDOG_RELOAD_PERIOD_MS ) ) );
        }
        event = k_event_test( &flrp_api_main_loop_event, 0xFFFFFFFF );
#else
        event = k_event_wait( &flrp_api_main_loop_event, 0xFFFFFFFF, false, K_MSEC( WATCHDOG_RELOAD_PERIOD_MS ) );
#endif

        if( event & FLRP_API_EVENT_BUTTON_PRESS )
        {
            LOG_DBG( "Button pressed" );
            if( flrp_api_initiate_tx( flrp_api_menu_get_target_device_eui( ) ) != true )
            {
                LOG_ERR( "Failed to initiate transmission" );
            }
        }
        if( event & FLRP_API_EVENT_KEEPALIVE )
        {
            flrp_remote_send_keep_alive( );
        }
        k_event_clear( &flrp_api_main_loop_event, event );
        event = FLRP_API_EVENT_NONE;
    }
    return 0;
}

void main_loop_set_event( flrp_api_event_t event )
{
    k_event_set( &flrp_api_main_loop_event, event );
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
        /* Wake up the main thread */
        main_loop_set_event( FLRP_API_EVENT_BUTTON_PRESS );
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

/* --- EOF ------------------------------------------------------------------ */