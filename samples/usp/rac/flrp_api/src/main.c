/**
 * @file      main.c
 *
 * @brief     Application main
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2026. All rights reserved.
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
#include <zephyr/drivers/gpio.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>

#include <smtc_modem_hal.h>

#include <smtc_zephyr_usp_api.h>
#include <smtc_sw_platform_helper.h>
#include <smtc_hal_led.h>
#include "app_flrp_api.h"

LOG_MODULE_REGISTER( flrp_api, LOG_LEVEL_INF );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/**
 * @brief Define the period for reloading the watchdog counter during sleep (The period must be lower than MCU watchdog
 * period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS 20000

/**
 * @brief User button
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

#if( FLRP_API_ROLE == FLRP_API_ROLE_INITIATOR )
static uint32_t periodic_timestamp_ms = 0;
#endif

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

/* A binary semaphore to notify the main loop */
K_SEM_DEFINE( flrp_api_event_sem, 0, 1 );

/**
 * @brief Example to send a user payload on an external event
 *
 */
int main( void )
{
    static bool flrp_tx = true;

    if( configure_user_button( ) != 0 )
    {
        LOG_ERR( "Issue when configuring user button, aborting\n" );
        return 1;
    }

    LOG_INF( "FLRP API example is starting" );

    SMTC_SW_PLATFORM_INIT( );
    SMTC_SW_PLATFORM_VOID( smtc_rac_init( ) );
    flrp_api_init( FLRP_API_CRYPTO_ENABLED, FLRP_API_IS_LOW_FREQUENCY, FLRP_API_IS_LISTENING );
    hal_led_init( );
    hal_led_set( HAL_LED_TX, true );
    hal_led_set( HAL_LED_RX, false );

    while( true )
    {
        if( user_button.is_pressed == true )
        {
            user_button.is_pressed = false;
            LOG_DBG( "Button pressed" );
#if( FLRP_API_ROLE == FLRP_API_ROLE_INITIATOR )
            periodic_timestamp_ms = smtc_modem_hal_get_time_in_ms( );
#endif
            flrp_tx = flrp_api_initiate_transfer( flrp_tx );
        }
#if( FLRP_API_ROLE == FLRP_API_ROLE_INITIATOR )
        else
        {
            if( ( smtc_flrp_call_run( ) == false ) &&
                ( ( smtc_modem_hal_get_time_in_ms( ) - periodic_timestamp_ms ) > FLRP_API_INTIATOR_PERIODIC_TRANSFER ) )
            {
                periodic_timestamp_ms = smtc_modem_hal_get_time_in_ms( );
                flrp_tx               = flrp_api_initiate_transfer( flrp_tx );
            }
        }
#endif

#if !defined( CONFIG_USP_MAIN_THREAD )
        smtc_rac_run_engine( );
        smtc_flrp_run_engine( );
        if( smtc_rac_is_irq_flag_pending( ) )
        {
            continue;
        }
        // Allows waking up on radio event or push-button press
        if( smtc_flrp_call_run( ) == false )
        {
            struct k_sem* sems[] = { smtc_modem_hal_get_event_sem( ), &flrp_api_event_sem };
            ( void ) wait_on_sems( sems, 2, K_MSEC( MIN( FLRP_API_INTIATOR_PERIODIC_TRANSFER, WATCHDOG_RELOAD_PERIOD_MS ) ) );
        }
#else
        if( user_button.is_pressed == false )
        {
            k_sem_take( &flrp_api_event_sem, K_MSEC( MIN( FLRP_API_INTIATOR_PERIODIC_TRANSFER ,WATCHDOG_RELOAD_PERIOD_MS ) ) );
        }
#endif
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
        /* Wake up the main thread */
        k_sem_give( &flrp_api_event_sem );
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
