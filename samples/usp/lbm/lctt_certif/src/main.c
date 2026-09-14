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
#include <smtc_modem_helper.h>
#include "smtc_modem_api.h"
#include "smtc_modem_utilities.h"
#include "smtc_modem_hal.h"

#if defined( CONFIG_LORA_BASICS_MODEM_RELAY_TX )
#include <smtc_modem_relay_api.h>
#endif

#include <smtc_rac_api.h>

LOG_MODULE_REGISTER( lctt_certif, LOG_LEVEL_INF );

/* lr11xx radio context and its use in the ralf layer */
static const struct device* transceiver = DEVICE_DT_GET( DT_ALIAS( lora_transceiver ) );

/**
 * @brief Watchdog counter reload value during sleep (The period must be lower than MCU watchdog
 * period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS 20000

/**
 * @brief Periodical uplink alarm delay in seconds
 */
#define PERIODICAL_UPLINK_DELAY_S 10

#define USER_BUTTON_NODE DT_ALIAS( lctt_certif_button )
#if !DT_NODE_HAS_STATUS( USER_BUTTON_NODE, okay )
#error "Unsupported board: lctt-certif-button devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR( USER_BUTTON_NODE, gpios, { 0 } );
static struct gpio_callback      button_cb_data;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static volatile bool user_button_is_press;
static uint32_t      uplink_counter;

static bool certif_running;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/**
 * @brief User callback for button EXTI
 *
 * @param context Define by the user at the init
 */
static void user_button_callback( const void* context );

/**
 * @brief Handle action taken if button is pushed
 *
 */
static void main_handle_push_button( void );

/**
 * @brief Send the 32bits uplink counter on chosen port
 */
static void send_uplink_counter_on_port( uint8_t port );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/* for zephyr compat*/
void button_pressed( const struct device* dev, struct gpio_callback* cb, uint32_t pins )
{
    user_button_callback( dev );
}

int main( void )
{
    uint32_t sleep_time_ms = 0;
    int      ret;

    lorawan_smtc_modem_hal_init( transceiver );

    /* Init the modem and use modem_event_callback as event callback, please note that the
     * callback will be called immediately after the first call to smtc_modem_run_engine because
     * of the reset detection
     */
    smtc_rac_init( );
    smtc_modem_init( &modem_event_callback );

    /* Configure Nucleo blue button as EXTI */
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
    ret = gpio_pin_interrupt_configure_dt( &button, GPIO_INT_EDGE_TO_INACTIVE );
    if( ret != 0 )
    {
        printk( "Error %d: failed to configure interrupt on %s pin %d\n", ret, button.port->name, button.pin );
        return 1;
    }
    gpio_init_callback( &button_cb_data, button_pressed, BIT( button.pin ) );
    gpio_add_callback( button.port, &button_cb_data );

    LOG_INF( "Certification example is starting" );
    LOG_INF( "Push button to enable/disable certification" );

    while( 1 )
    {
        /* Check button */
        if( user_button_is_press == true )
        {
            user_button_is_press = false;

            main_handle_push_button( );
        }

        /* Modem process launch */
        sleep_time_ms = smtc_modem_run_engine( );

        /* Check sleep conditions (button was not pressed) */
        if( ( user_button_is_press == false ) && ( smtc_modem_is_irq_flag_pending( ) == false ) )
        {
            uint32_t real_sleep_time_ms = MIN( sleep_time_ms, WATCHDOG_RELOAD_PERIOD_MS );

            smtc_modem_hal_interruptible_msleep( K_MSEC( real_sleep_time_ms ) );
        }
    }
    return 0;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */
void modem_event_callback( void )
{
    LOG_INF( "Modem event callback" );

    smtc_modem_event_t current_event = { 0 };
    uint8_t            event_pending_count;
    uint8_t            stack_id = STACK_ID;

    /* Continue to read modem event until all event has been processed */
    do
    {
        /* Read modem event */
        ASSERT_SMTC_MODEM_RC( smtc_modem_get_event( &current_event, &event_pending_count ) );

        switch( current_event.event_type )
        {
        case SMTC_MODEM_EVENT_RESET:
        {
            LOG_INF( "Event received: RESET" );

#if !defined( CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS )
            /* Set user credentials */
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_deveui( stack_id, smtc_modem_helper_get_eui( ) ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_joineui( stack_id, smtc_modem_helper_get_join_eui( ) ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_appkey( stack_id, smtc_modem_helper_get_gen_app_key( ) ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_nwkkey( stack_id, smtc_modem_helper_get_app_key( ) ) );
#else
            /* Get internal credentials */
            ASSERT_SMTC_MODEM_RC( smtc_modem_get_chip_eui( stack_id, smtc_modem_helper_get_eui( ) ) );
            SMTC_HAL_TRACE_ARRAY( "CHIP_EUI", smtc_modem_helper_get_eui( ), SMTC_MODEM_EUI_LENGTH );
            ASSERT_SMTC_MODEM_RC( smtc_modem_get_pin( stack_id, smtc_modem_helper_get_pin( ) ) );
            SMTC_HAL_TRACE_ARRAY( "CHIP_PIN", smtc_modem_helper_get_pin( ), SMTC_MODEM_PIN_LENGTH );
#endif
            /* Set user region */
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_region( stack_id, MODEM_REGION ) );
#if defined( CONFIG_LORA_BASICS_MODEM_RELAY_TX )
            smtc_modem_helper_get_relay_config( )->second_ch_enable = false;
            smtc_modem_helper_get_relay_config( )->activation       = SMTC_MODEM_RELAY_TX_ACTIVATION_MODE_ED_CONTROLLED;
            smtc_modem_helper_get_relay_config( )->number_of_miss_wor_ack_to_switch_in_nosync_mode = 1;
            smtc_modem_helper_get_relay_config( )->smart_level                                     = 5;
            smtc_modem_helper_get_relay_config( )->backoff                                         = 4;
            ASSERT_SMTC_MODEM_RC( smtc_modem_relay_tx_enable( stack_id, smtc_modem_helper_get_relay_config( ) ) );
#endif
            ASSERT_SMTC_MODEM_RC( smtc_modem_get_certification_mode( stack_id, &certif_running ) );
            if( certif_running == false )
            {
                ASSERT_SMTC_MODEM_RC( smtc_modem_join_network( stack_id ) );
            }
            break;
        }
        case SMTC_MODEM_EVENT_ALARM:
            LOG_INF( "Event received: ALARM" );
            if( certif_running == true )
            {
                ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_clear_timer( ) );
            }
            else
            {
                /* Send periodical uplink on port 101 */
                send_uplink_counter_on_port( 101 );
                ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( PERIODICAL_UPLINK_DELAY_S ) );
            }
            break;

        case SMTC_MODEM_EVENT_JOINED:
            LOG_INF( "Event received: JOINED" );
            LOG_INF( "Modem is now joined " );

            if( certif_running == false )
            {
                /* start periodical uplink alarm */
                ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( PERIODICAL_UPLINK_DELAY_S ) );
            }
            break;

        case SMTC_MODEM_EVENT_TXDONE:
            LOG_INF( "Event received: TXDONE" );
            LOG_INF( "Transmission done " );
            break;

        case SMTC_MODEM_EVENT_DOWNDATA:
            LOG_INF( "Event received: DOWNDATA" );
            // Get downlink data
            smtc_modem_get_downlink_data( smtc_modem_helper_get_rx_payload( ), smtc_modem_helper_get_rx_payload_size( ),
                                          smtc_modem_helper_get_rx_metadata( ), smtc_modem_helper_get_rx_remaining( ) );
            LOG_INF( "Data received on port %u", smtc_modem_helper_get_rx_metadata( )->fport );
            LOG_HEXDUMP_INF( smtc_modem_helper_get_rx_payload( ), *smtc_modem_helper_get_rx_payload_size( ),
                             "Received payload" );
            break;

        case SMTC_MODEM_EVENT_JOINFAIL:
            LOG_INF( "Event received: JOINFAIL" );
            break;

        case SMTC_MODEM_EVENT_ALCSYNC_TIME:
            LOG_INF( "Event received: ALCSync service TIME" );
            break;

        case SMTC_MODEM_EVENT_LINK_CHECK:
            LOG_INF( "Event received: LINK_CHECK" );
            break;

        case SMTC_MODEM_EVENT_CLASS_B_PING_SLOT_INFO:
            LOG_INF( "Event received: CLASS_B_PING_SLOT_INFO" );
            break;

        case SMTC_MODEM_EVENT_CLASS_B_STATUS:
            LOG_INF( "Event received: CLASS_B_STATUS" );
            break;

        case SMTC_MODEM_EVENT_LORAWAN_MAC_TIME:
            LOG_WRN( "Event received: LORAWAN MAC TIME" );
            break;

        case SMTC_MODEM_EVENT_LORAWAN_FUOTA_DONE:
        {
            bool status = current_event.event_data.fuota_status.successful;

            if( status == true )
            {
                LOG_INF( "Event received: FUOTA SUCCESSFUL" );
            }
            else
            {
                LOG_WRN( "Event received: FUOTA FAIL" );
            }
            break;
        }

        case SMTC_MODEM_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_C:
            LOG_INF( "Event received: MULTICAST CLASS_C STOP" );
            break;

        case SMTC_MODEM_EVENT_NO_MORE_MULTICAST_SESSION_CLASS_B:
            LOG_INF( "Event received: MULTICAST CLASS_B STOP" );
            break;

        case SMTC_MODEM_EVENT_NEW_MULTICAST_SESSION_CLASS_C:
            LOG_INF( "Event received: New MULTICAST CLASS_C " );
            break;

        case SMTC_MODEM_EVENT_NEW_MULTICAST_SESSION_CLASS_B:
            LOG_INF( "Event received: New MULTICAST CLASS_B" );
            break;

        case SMTC_MODEM_EVENT_FIRMWARE_MANAGEMENT:
            LOG_INF( "Event received: FIRMWARE_MANAGEMENT" );
            if( current_event.event_data.fmp.status == SMTC_MODEM_EVENT_FMP_REBOOT_IMMEDIATELY )
            {
                smtc_modem_hal_reset_mcu( );
            }
            break;

        case SMTC_MODEM_EVENT_STREAM_DONE:
            LOG_INF( "Event received: STREAM_DONE" );
            break;

        case SMTC_MODEM_EVENT_UPLOAD_DONE:
            LOG_INF( "Event received: UPLOAD_DONE" );
            break;

        case SMTC_MODEM_EVENT_DM_SET_CONF:
            LOG_INF( "Event received: DM_SET_CONF" );
            break;

        case SMTC_MODEM_EVENT_MUTE:
            LOG_INF( "Event received: MUTE" );
            break;

        case SMTC_MODEM_EVENT_RELAY_TX_DYNAMIC:
            /* Relay TX dynamic mode has enable or disable the WOR protocol */
            LOG_INF( "Event received: RELAY_TX_DYNAMIC" );
            break;

        case SMTC_MODEM_EVENT_RELAY_TX_MODE:
            /* Relay TX activation has been updated */
            LOG_INF( "Event received: RELAY_TX_MODE" );
            break;

        case SMTC_MODEM_EVENT_RELAY_TX_SYNC:
            /* Relay TX synchronisation has changed */
            LOG_INF( "Event received: RELAY_TX_SYNC" );
            break;

        case SMTC_MODEM_EVENT_RELAY_RX_RUNNING:
            LOG_INF( "Event received: RELAY_RX_RUNNING\n" );
#if defined( ADD_CSMA ) && defined( ADD_RELAY_RX )
            bool csma_state = false;
            ASSERT_SMTC_MODEM_RC( smtc_modem_csma_get_state( STACK_ID, &csma_state ) );
            if( ( current_event.event_data.relay_rx.status == true ) && ( csma_state == true ) )
            {
                // Disable CSMA when Relay Rx Is enabled by network
                ASSERT_SMTC_MODEM_RC( smtc_modem_csma_set_state( STACK_ID, false ) );
            }
#if defined( ENABLE_CSMA_BY_DEFAULT ) && defined( ADD_RELAY_RX )
            if( current_event.event_data.relay_rx.status == false )
            {
                ASSERT_SMTC_MODEM_RC( smtc_modem_csma_set_state( STACK_ID, true ) );
            }
#endif  // ENABLE_CSMA_BY_DEFAULT
#endif  // ADD_CSMA
            break;

        case SMTC_MODEM_EVENT_REGIONAL_DUTY_CYCLE:
            LOG_INF( "Event received: REGIONAL_DUTY_CYCLE\n" );
            break;

        case SMTC_MODEM_EVENT_NO_DOWNLINK_THRESHOLD:
        {
            LOG_INF( "Event received: NO_DOWNLINK_THRESHOLD\n" );
            break;
        }

        default:
            LOG_ERR( "Unknown event %u", current_event.event_type );
            break;
        }
    } while( event_pending_count > 0 );
}

static void user_button_callback( const void* context )
{
    LOG_INF( "Button pushed" );

    static uint32_t last_press_timestamp_ms;

    /* Debounce the button press, avoid multiple triggers */
    if( ( int32_t ) ( smtc_modem_hal_get_time_in_ms( ) - last_press_timestamp_ms ) > 500 )
    {
        last_press_timestamp_ms = smtc_modem_hal_get_time_in_ms( );
        smtc_modem_hal_wake_up( );
        user_button_is_press = true;
    }
}
static void main_handle_push_button( void )
{
    if( certif_running == true )
    {
        smtc_modem_set_certification_mode( STACK_ID, false );
        ASSERT_SMTC_MODEM_RC( smtc_modem_leave_network( STACK_ID ) );
        ASSERT_SMTC_MODEM_RC( smtc_modem_join_network( STACK_ID ) );
        certif_running = false;
    }
    else
    {
        smtc_modem_set_certification_mode( STACK_ID, true );
        certif_running = true;
    }
}

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
