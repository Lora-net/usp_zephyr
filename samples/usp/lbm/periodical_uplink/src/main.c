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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>

#include <smtc_modem_api.h>
#include <smtc_modem_test_api.h>
#include <smtc_modem_utilities.h>
#include <smtc_modem_hal.h>
#include <smtc_modem_helper.h>

#include <smtc_zephyr_usp_api.h>
#include <smtc_sw_platform_helper.h>
#include <smtc_hal_led.h>

LOG_MODULE_REGISTER( periodical_uplink, LOG_LEVEL_INF );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/**
 * @brief Watchdog counter reload value during sleep (The period must be lower than MCU watchdog
 * period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS 20000

/**
 * @brief Periodical uplink alarm delay in seconds
 */
#ifndef PERIODICAL_UPLINK_DELAY_S
#define PERIODICAL_UPLINK_DELAY_S 60
#endif

#ifndef DELAY_FIRST_MSG_AFTER_JOIN
#define DELAY_FIRST_MSG_AFTER_JOIN 60
#endif

/**
 * @brief User button - Disable if not available on user board
 */

#define LBM_DTS_HAS_USER_BTN DT_NODE_EXISTS( DT_ALIAS( smtc_user_button ) )

#if LBM_DTS_HAS_USER_BTN
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET( DT_ALIAS( smtc_user_button ), gpios );
static struct gpio_callback      button_cb_data;
#endif
/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static uint32_t uplink_counter = 0; /* uplink raising counter */
#if LBM_DTS_HAS_USER_BTN
static volatile bool user_button_is_press = false; /* Flag for button status */
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

#if LBM_DTS_HAS_USER_BTN
/**
 * @brief User callback for button EXTI
 *
 * @param context Define by the user at the init
 */
static void user_button_callback( const void* context );

/**
 * @brief Configure User Button
 *
 */
static int configure_user_button( void );
#endif

/**
 * @brief Send the 32bits uplink counter on chosen port
 */
static void send_uplink_counter_on_port( uint8_t port );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/* A binary semaphore to notify the main LBM loop */
K_SEM_DEFINE( periodical_uplink_event_sem, 0, 1 );

#if LBM_DTS_HAS_USER_BTN
/* for zephyr compat*/
void button_pressed( const struct device* dev, struct gpio_callback* cb, uint32_t pins )
{
    printk( "%s", __func__ );
    user_button_callback( dev );
}
#endif

/**
 * @brief Example to send a user payload on an external event
 *
 */
int main( void )
{
#if LBM_DTS_HAS_USER_BTN
    if( configure_user_button( ) != 0 )
    {
        LOG_ERR( "Issue when configuring user button, aborting\n" );
        return 1;
    }
#endif

    LOG_INF( "LoRaWAN Periodical uplink (%d sec) example is starting", PERIODICAL_UPLINK_DELAY_S );

    SMTC_SW_PLATFORM_INIT( );
    SMTC_SW_PLATFORM_VOID( smtc_rac_init( ) );
    // Call smtc_modem_init() after smtc_rac_init()
    SMTC_SW_PLATFORM_VOID( smtc_modem_init( &modem_event_callback ) );

    hal_led_init( );
    hal_led_set( HAL_LED_TX, true );
    hal_led_set( HAL_LED_RX, false );

    while( true )
    {
#if LBM_DTS_HAS_USER_BTN
        if( user_button_is_press == true )
        {
            user_button_is_press = false;
            LOG_DBG( "Button pressed" );

            smtc_modem_status_mask_t status_mask = 0;

            smtc_modem_get_status( STACK_ID, &status_mask );

            // Check if the device has already joined a network
            if( ( status_mask & SMTC_MODEM_STATUS_JOINED ) == SMTC_MODEM_STATUS_JOINED )
            {
                // Send the uplink counter on port 102
                send_uplink_counter_on_port( 102 );
                smtc_modem_hal_wake_up( );
            }
        }
#endif

#if !defined( CONFIG_USP_MAIN_THREAD )
        uint32_t sleep_time_ms = smtc_modem_run_engine( );
        smtc_rac_run_engine( );
        if( smtc_rac_is_irq_flag_pending( ) )
        {
            continue;
        }
        // Allows waking up on radio event or push-button press
        struct k_sem* sems[] = { smtc_modem_hal_get_event_sem( ), &periodical_uplink_event_sem };
        ( void ) wait_on_sems( sems, 2, K_MSEC( MIN( sleep_time_ms, WATCHDOG_RELOAD_PERIOD_MS ) ) );
#else
#if LBM_DTS_HAS_USER_BTN
        if( user_button_is_press == false )
        {
            k_sem_take( &periodical_uplink_event_sem, K_MSEC( WATCHDOG_RELOAD_PERIOD_MS ) );
        }
#else
        k_sem_take( &periodical_uplink_event_sem, K_MSEC( WATCHDOG_RELOAD_PERIOD_MS ) );
#endif
#endif
    }
    return 0;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

#if LBM_DTS_HAS_USER_BTN
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
#endif

void smtc_modem_helper_event( smtc_modem_event_type_t event_type )
{
    switch( event_type )
    {
    case SMTC_MODEM_EVENT_ALARM:
        /* Send periodical uplink on port 101 */
        send_uplink_counter_on_port( 101 );
        /* Restart periodical uplink alarm */
        ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( PERIODICAL_UPLINK_DELAY_S ) );
        break;
    case SMTC_MODEM_EVENT_JOINED:
        /* Send first periodical uplink on port 101 */
        send_uplink_counter_on_port( 101 );
        /* start periodical uplink alarm */
        ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( DELAY_FIRST_MSG_AFTER_JOIN ) );
        break;
    default:
        break;
    }
}

#if LBM_DTS_HAS_USER_BTN
static void user_button_callback( const void* context )
{
    LOG_INF( "Button pushed" );

    static uint32_t last_press_timestamp_ms;

    /* Debounce the button press, avoid multiple triggers */
    if( ( int32_t ) ( smtc_modem_hal_get_time_in_ms( ) - last_press_timestamp_ms ) > 500 )
    {
        last_press_timestamp_ms = smtc_modem_hal_get_time_in_ms( );
        user_button_is_press    = true;
    }
    /* Wake up the main thread */
    k_sem_give( &periodical_uplink_event_sem );
}
#endif

static void send_uplink_counter_on_port( uint8_t port )
{
    /* Send uplink counter on port 102 */
    uint8_t buff[4] = { 0 };

    buff[0] = ( uplink_counter >> 24 ) & 0xFF;
    buff[1] = ( uplink_counter >> 16 ) & 0xFF;
    buff[2] = ( uplink_counter >> 8 ) & 0xFF;
    buff[3] = ( uplink_counter & 0xFF );
    ASSERT_SMTC_MODEM_RC( smtc_modem_request_uplink( STACK_ID, port, false, buff, 4 ) );
    /* Increment uplink counter */
    uplink_counter++;
}

/* --- EOF ------------------------------------------------------------------ */
