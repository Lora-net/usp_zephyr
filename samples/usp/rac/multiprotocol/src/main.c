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
#include <zephyr/shell/shell.h>
#include <time.h>
#include <strings.h>

#include <smtc_modem_api.h>
#include <smtc_modem_test_api.h>
#include <smtc_modem_utilities.h>
#include <smtc_modem_hal.h>

#include <smtc_zephyr_usp_api.h>
#include <smtc_sw_platform_helper.h>
#include <smtc_modem_helper.h>
#include <smtc_zephyr_log_custom.h>
#include <smtc_hal_led.h>

#include <app_ranging_hopping.h>
#include <main_ranging_demo.h>

LOG_MODULE_REGISTER( multiprotocol, LOG_LEVEL_INF );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/**
 * @def KEEP_ALIVE_PORT
 * @brief Define the port used for keep-alive messages
 */
#define KEEP_ALIVE_PORT 101

/**
 * @def RANGING_UPLINK_PORT
 * @brief Define the port used for ranging uplink messages
 */
#define RANGING_UPLINK_PORT 102

/**
 * @def RANGING_UPLINK_MAX_RATE
 * @brief Define the maximum rate for ranging uplink messages (in ms)
 */
#define RANGING_UPLINK_MAX_RATE 60000

/**
 * @def WATCHDOG_RELOAD_PERIOD_MS
 * @brief Watchdog counter reload value during sleep (The period must be lower than MCU watchdog
 * period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS 20000

/**
 * @def PERIODICAL_UPLINK_DELAY_S
 * @brief Periodical uplink alarm delay in seconds
 */
#ifndef PERIODICAL_UPLINK_DELAY_S
#define PERIODICAL_UPLINK_DELAY_S 600
#endif

/**
 * @def DELAY_FIRST_MSG_AFTER_JOIN
 * @brief Delay before sending the first message after joining from alarm
 */
#ifndef DELAY_FIRST_MSG_AFTER_JOIN
#define DELAY_FIRST_MSG_AFTER_JOIN 60
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
/**
 * @struct multiprotocol_uplink_t
 * @brief Structure for multiprotocol uplink messages with ranging test result
 */
typedef struct __packed multiprotocol_uplink_s
{
    uint16_t distance; /**< Distance in meters */
    uint8_t  sf;       /**< Spreading factor */
    uint8_t  bw;       /**< Bandwidth */
} multiprotocol_uplink_t;

typedef enum multiprotocol_event_e
{
    MULTIPROTOCOL_EVENT_NONE         = 0x00,       /**< No event */
    MULTIPROTOCOL_EVENT_BUTTON_PRESS = ( 1 << 0 ), /**< Button press event */
    MULTIPROTOCOL_EVENT_RANGING      = ( 1 << 1 ), /**< Ranging event */
    MULTIPROTOCOL_EVENT_SET_MODE     = ( 1 << 2 ), /**< Set mode event */
    MULTIPROTOCOL_EVENT_KEEPALIVE    = ( 1 << 3 ), /**< Keepalive event */
    MULTIPROTOCOL_EVENT_REQ_MAC_TIME = ( 1 << 4 ), /**< Request MAC time event */
} multiprotocol_event;
typedef uint32_t multiprotocol_event_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static bool                is_mode_set  = false;             // Indicates that mode has not been set yet
static bool                is_manager   = true;              // Set is_manager for ranging test. Can be set once by user
static smtc_rac_priority_t rac_priority = RAC_LOW_PRIORITY;  // Default priority for RAC ranging test

/**
 * @brief Uplink message with ranging result @see multiprotocol_uplink_t
 */
static multiprotocol_uplink_t multiprotocol_uplink = { 0 };  // Ranging result used in uplink message

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
 * @brief Configure User Button
 *
 */
static int configure_user_button( void );

/**
 * @brief User callback for ranging result
 *
 *  This callback is fired every time a ranging result is available.
 */
static void ranging_results_callback( smtc_rac_radio_lora_params_t* radio_lora_params,
                                      ranging_params_settings_t*    ranging_params_settings,
                                      ranging_global_result_t* ranging_global_results, const char* region );

/**
 * @brief Function to get priority string name
 *
 *  @param priority Priority value @see smtc_rac_priority_t
 *  @return a string representation of the priority
 */
static char* get_priority_str( smtc_rac_priority_t rac_priority );

/**
 * @brief Function to get priority value from string name
 *
 *  @param priority Priority string name
 *  @return a priority value @see smtc_rac_priority_t or 0 if not found
 */
static smtc_rac_priority_t get_priority_from_str( char* priority );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/* An event to notify the main loop */
K_EVENT_DEFINE( main_loop_event );

/* for zephyr compat*/
void button_pressed( const struct device* dev, struct gpio_callback* cb, uint32_t pins )
{
    printk( "%s", __func__ );
    user_button_callback( dev );
}

/*
 * -----------------------------------------------------------------------------
 * --- SHELL COMMANDS IMPLEMENTATION ------------------------------------------
 */

/**
 * @brief Shell command: Start ranging exchange
 */
static int cmd_ranging_start( const struct shell* sh, size_t argc, char** argv )
{
    ARG_UNUSED( argc );
    ARG_UNUSED( argv );

    if( is_mode_set == true )
    {
        shell_print( sh, "Starting ranging exchange..." );
        k_event_set( &main_loop_event, MULTIPROTOCOL_EVENT_RANGING );
        return 0;
    }
    else
    {
        shell_error( sh, "Please set the mode first using: mode <manager|subordinate>" );
        return -1;
    }
}

/**
 * @brief Shell command: Show device status
 */
static int cmd_status( const struct shell* sh, size_t argc, char** argv )
{
    ARG_UNUSED( argc );
    ARG_UNUSED( argv );

    smtc_modem_status_mask_t status_mask = 0;
    smtc_modem_return_code_t rc;
    uint32_t                 gps_time_s       = 0;
    uint32_t                 gps_fractional_s = 0;
    uint8_t*                 user_dev_eui     = smtc_modem_helper_get_eui( );

    smtc_modem_get_status( STACK_ID, &status_mask );
    rc = smtc_modem_get_lorawan_mac_time( STACK_ID, &gps_time_s, &gps_fractional_s );
    shell_print( sh, "=== Device Status ===" );
    shell_print( sh, "LoRaWAN joined: %s", ( status_mask & SMTC_MODEM_STATUS_JOINED ) ? "YES" : "NO" );
    shell_print( sh, "Synchronized: %s", ( rc == SMTC_MODEM_RC_OK ) ? "YES" : "NO" );
    shell_print( sh, "Is manager: %s priority %s", ( is_mode_set == true ) ? ( is_manager ? "YES" : "NO" ) : "UNKNOWN",
                 get_priority_str( rac_priority ) );
    shell_print( sh, "User device EUI: 0x%02X %02X %02X %02X %02X %02X %02X %02X", user_dev_eui[0], user_dev_eui[1],
                 user_dev_eui[2], user_dev_eui[3], user_dev_eui[4], user_dev_eui[5], user_dev_eui[6], user_dev_eui[7] );

    return 0;
}

/**
 * @brief Shell command: Send LoRaWAN keepalive
 */
static int cmd_send_keepalive( const struct shell* sh, size_t argc, char** argv )
{
    ARG_UNUSED( argc );
    ARG_UNUSED( argv );
    shell_print( sh, "Request keepalive empty message " );
    k_event_set( &main_loop_event, MULTIPROTOCOL_EVENT_KEEPALIVE );

    return 0;
}

/**
 * @brief Shell command: Show ranging results
 */
static int cmd_ranging_info( const struct shell* sh, size_t argc, char** argv )
{
    ARG_UNUSED( argc );
    ARG_UNUSED( argv );

    shell_print( sh, "=== Ranging Information ===" );
    shell_print( sh, "Last distance: %u m", multiprotocol_uplink.distance );
    shell_print( sh, "Last SF: %u", multiprotocol_uplink.sf );
    shell_print( sh, "Last BW: %u kHz", multiprotocol_uplink.bw );
    shell_print( sh, "Mode: %s", is_manager ? "Manager" : "Subordinate" );

    return 0;
}

/**
 * @brief Shell command: Force button press simulation
 */
static int cmd_button_press( const struct shell* sh, size_t argc, char** argv )
{
    ARG_UNUSED( argc );
    ARG_UNUSED( argv );

    shell_print( sh, "Simulating button press..." );
    k_event_set( &main_loop_event, MULTIPROTOCOL_EVENT_BUTTON_PRESS );

    return 0;
}

/**
 * @brief Shell command: Show GPS time
 */
static int cmd_gps_time( const struct shell* sh, size_t argc, char** argv )
{
    ARG_UNUSED( argc );
    ARG_UNUSED( argv );

    uint32_t gps_time_s       = 0;
    uint32_t gps_fractional_s = 0;

    smtc_modem_return_code_t rc = smtc_modem_get_lorawan_mac_time( STACK_ID, &gps_time_s, &gps_fractional_s );

    if( rc == SMTC_MODEM_RC_OK )
    {
        shell_print( sh, "GPS Time: %u.%06u seconds", gps_time_s, gps_fractional_s );
        time_t     unix_time = gps_time_s + UNIX_GPS_EPOCH_OFFSET;
        struct tm* timeinfo  = gmtime( &unix_time );

        if( timeinfo != NULL )
        {
            shell_print( sh, "Date: %04d-%02d-%02d %02d:%02d:%02d.%03u UTC", timeinfo->tm_year + 1900,
                         timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec,
                         gps_fractional_s );
        }
    }
    else
    {
        shell_error( sh, "GPS time not available (rc=%d)", rc );
        uint64_t uptime_ms = smtc_modem_hal_get_time_in_ms( );
        shell_print( sh, "System uptime: %llu ms", uptime_ms );
    }

    return 0;
}

/**
 * @brief Shell command: Request mac time from lorawan
 */
static int cmd_req_mac_time( const struct shell* sh, size_t argc, char** argv )
{
    ARG_UNUSED( argc );
    ARG_UNUSED( argv );

    shell_print( sh, "Launch request MAC time" );
    k_event_set( &main_loop_event, MULTIPROTOCOL_EVENT_REQ_MAC_TIME );

    return 0;
}

/**
 * @brief Shell command: Set ranging mode (manager/subordinate)
 */
static int cmd_set_mode( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( is_mode_set == true )
    {
        shell_print( sh, "Mode has already been set to %s priority %s", is_manager ? "manager" : "subordinate",
                     get_priority_str( rac_priority ) );
    }
    else if( argc != 3 )
    {
        shell_error( sh, "Usage: mode <manager|subordinate> <VERY_HIGH|HIGH|MEDIUM|LOW|VERY_LOW>" );
        ret = -EINVAL;
    }
    else if( get_priority_from_str( argv[2] ) == 0 )
    {
        shell_error( sh, "Invalid priority '%s'. Use 'VERY_HIGH', 'HIGH', 'MEDIUM', 'LOW' or 'VERY_LOW'", argv[2] );
        ret = -EINVAL;
    }
    else
    {
        if( strcasecmp( argv[1], "manager" ) == 0 )
        {
            is_manager   = true;
            rac_priority = get_priority_from_str( argv[2] );
            shell_print( sh, "Device set as MANAGER" );
            shell_print( sh, "Ranging priority set to %s", get_priority_str( rac_priority ) );
            k_event_set( &main_loop_event, MULTIPROTOCOL_EVENT_SET_MODE );
        }
        else if( strcasecmp( argv[1], "subordinate" ) == 0 )
        {
            is_manager   = false;
            rac_priority = get_priority_from_str( argv[2] );
            shell_print( sh, "Device set as SUBORDINATE" );
            shell_print( sh, "Ranging priority set to %s", get_priority_str( rac_priority ) );
            k_event_set( &main_loop_event, MULTIPROTOCOL_EVENT_SET_MODE );
        }
        else
        {
            shell_error( sh, "Invalid mode '%s'. Use 'manager' or 'subordinate'", argv[1] );
            ret = -EINVAL;
        }
    }

    return ret;
}

/*
 * -----------------------------------------------------------------------------
 * --- SHELL COMMANDS REGISTRATION --------------------------------------------
 */

// Ranging shell subcommands
SHELL_STATIC_SUBCMD_SET_CREATE( sub_ranging, SHELL_CMD( start, NULL, "Start ranging exchange", cmd_ranging_start ),
                                SHELL_CMD( info, NULL, "Show ranging information", cmd_ranging_info ),
                                SHELL_SUBCMD_SET_END );

// Main shell commands
SHELL_CMD_REGISTER( status, NULL, "Show device status", cmd_status );
SHELL_CMD_REGISTER( ranging, &sub_ranging, "Ranging commands", NULL );
SHELL_CMD_REGISTER( uplink, NULL, "Send LoRaWAN keepalive", cmd_send_keepalive );
SHELL_CMD_REGISTER( button, NULL, "Simulate button press", cmd_button_press );
SHELL_CMD_REGISTER( time, NULL, "Show GPS/system time", cmd_gps_time );
SHELL_CMD_REGISTER( req_time, NULL, "Request mac time", cmd_req_mac_time );
SHELL_CMD_REGISTER( mode, NULL, "Set ranging mode <manager|subordinate>", cmd_set_mode );

/**
 * @brief Example to trig ranging test and send result in LoRaWAN uplink
 *
 */
int main( void )
{
    multiprotocol_event_t    event       = MULTIPROTOCOL_EVENT_NONE;
    smtc_modem_status_mask_t status_mask = 0;
    smtc_modem_return_code_t rc;

    custom_log_init( STACK_ID );
    if( configure_user_button( ) != 0 )
    {
        LOG_ERR( "Issue when configuring user button, aborting\n" );
        return 1;
    }

    LOG_INF( "Multiprotocol sample with LoRaWAN Periodical uplink (%d sec) example is starting",
             PERIODICAL_UPLINK_DELAY_S );

    SMTC_SW_PLATFORM_INIT( );
    SMTC_SW_PLATFORM_VOID( smtc_rac_init( ) );
    // Call smtc_modem_init() after smtc_rac_init()
    SMTC_SW_PLATFORM_VOID( smtc_modem_init( &modem_event_callback ) );

    hal_led_init( );
    hal_led_set( HAL_LED_TX, true );
    hal_led_set( HAL_LED_RX, false );

    while( true )
    {
#if !defined( CONFIG_USP_MAIN_THREAD )
        uint32_t sleep_time_ms = smtc_modem_run_engine( );
        smtc_rac_run_engine( );
        if( smtc_rac_is_irq_flag_pending( ) )
        {
            continue;
        }
        // Allows waking up on radio event, push-button press, or other events
        struct k_sem* sems[] = { smtc_modem_hal_get_event_sem( ) };
        int           result = wait_on_sems_and_event( sems, 1, &main_loop_event, 0xFFFFFFFF,
                                                       K_MSEC( MIN( sleep_time_ms, WATCHDOG_RELOAD_PERIOD_MS ) ) );

        event = k_event_test( &main_loop_event, 0xFFFFFFFF );
#else
        event = k_event_wait( &main_loop_event, 0xFFFFFFFF, false, K_MSEC( WATCHDOG_RELOAD_PERIOD_MS ) );
#endif
        if( event & MULTIPROTOCOL_EVENT_BUTTON_PRESS )
        {
            LOG_INF( "Button pressed" );
            // Start a ranging exchange on button press
            if( is_mode_set == true )
            {
                start_ranging_exchange( 0, is_manager );
                smtc_modem_hal_wake_up( );
            }
        }
        if( event & MULTIPROTOCOL_EVENT_RANGING )
        {
            if( is_mode_set == true )
            {
                LOG_INF( "Launch ranging" );
                start_ranging_exchange( 0, is_manager );
                smtc_modem_hal_wake_up( );
            }
        }
        if( event & MULTIPROTOCOL_EVENT_SET_MODE )
        {
            if( is_mode_set == false )
            {
                LOG_INF( "Set mode %s", ( is_manager == true ) ? "MANAGER" : "SUBORDINATE" );
                is_mode_set = true;
                app_radio_ranging_params_init( is_manager, rac_priority );
                app_radio_ranging_set_user_callback( ranging_results_callback );
                if( is_manager == false )
                {
                    start_ranging_exchange( 0, is_manager );
                    smtc_modem_hal_wake_up( );
                }
            }
        }
        if( event & MULTIPROTOCOL_EVENT_KEEPALIVE )
        {
            smtc_modem_get_status( STACK_ID, &status_mask );
            if( ( status_mask & SMTC_MODEM_STATUS_JOINED ) == 0 )
            {
                LOG_ERR( "Device not joined to LoRaWAN network" );
            }
            else
            {
                rc = smtc_modem_request_empty_uplink( STACK_ID, true, KEEP_ALIVE_PORT, false );

                if( rc == SMTC_MODEM_RC_OK )
                {
                    LOG_INF( "Send keepalive uplink" );
                    smtc_modem_hal_wake_up( );
                }
                else
                {
                    LOG_ERR( "Failed to schedule uplink (rc=%d)", rc );
                }
            }
        }
        if( event & MULTIPROTOCOL_EVENT_REQ_MAC_TIME )
        {
            smtc_modem_get_status( STACK_ID, &status_mask );

            // Check if the device has already joined a network
            if( ( status_mask & SMTC_MODEM_STATUS_JOINED ) == 0 )
            {
                LOG_ERR( "Device not joined to LoRaWAN network" );
            }
            else
            {
                rc = smtc_modem_trig_lorawan_mac_request( STACK_ID, SMTC_MODEM_LORAWAN_MAC_REQ_DEVICE_TIME );

                if( rc == SMTC_MODEM_RC_OK )
                {
                    smtc_modem_hal_wake_up( );
                    LOG_DBG( "MAC time request triggered successfully" );
                }
                else
                {
                    LOG_ERR( "Failed to trigger MAC time request (rc=%d)", rc );
                }
            }
        }

        k_event_clear( &main_loop_event, event );
        event = MULTIPROTOCOL_EVENT_NONE;
    }

    return 0;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

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

void smtc_modem_helper_event( smtc_modem_event_type_t event_type )
{
    switch( event_type )
    {
    case SMTC_MODEM_EVENT_JOINED:
        ASSERT_SMTC_MODEM_RC( smtc_modem_trig_lorawan_mac_request( STACK_ID, SMTC_MODEM_LORAWAN_MAC_REQ_DEVICE_TIME ) );
        /* start periodical uplink alarm */
        ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( DELAY_FIRST_MSG_AFTER_JOIN ) );
        break;
    case SMTC_MODEM_EVENT_ALARM:
        /* Send keep-alive */
        smtc_modem_request_empty_uplink( STACK_ID, true, KEEP_ALIVE_PORT, false );
        /* Restart periodical uplink alarm */
        ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( PERIODICAL_UPLINK_DELAY_S ) );
        break;
    case SMTC_MODEM_EVENT_DOWNDATA:
        /* Get downlink data */
        break;
    default:
        break;
    }
}

static void user_button_callback( const void* context )
{
    LOG_INF( "Button pushed" );

    static uint32_t last_press_timestamp_ms = 0;

    /* Debounce the button press, avoid multiple triggers */
    if( ( int32_t ) ( smtc_modem_hal_get_time_in_ms( ) - last_press_timestamp_ms ) > 500 )
    {
        last_press_timestamp_ms = smtc_modem_hal_get_time_in_ms( );
        /* Wake up the main thread */
        k_event_set( &main_loop_event, MULTIPROTOCOL_EVENT_BUTTON_PRESS );
    }
}

static void ranging_results_callback( smtc_rac_radio_lora_params_t* radio_lora_params,
                                      ranging_params_settings_t*    ranging_params_settings,
                                      ranging_global_result_t* ranging_global_results, const char* region )
{
    smtc_modem_status_mask_t status_mask              = 0;
    static uint32_t          last_uplink_timestamp_ms = 0;

    LOG_INF( "Ranging result: distance=%d m, SF=%u, BW=%u kHz", ranging_global_results->rng_distance,
             radio_lora_params->sf, radio_lora_params->bw );

    if( is_manager == true )
    {
        if( ( int32_t ) ( smtc_modem_hal_get_time_in_ms( ) - last_uplink_timestamp_ms ) >= RANGING_UPLINK_MAX_RATE )
        {
            smtc_modem_get_status( STACK_ID, &status_mask );
            // Check if the device has already joined a network
            if( ( status_mask & SMTC_MODEM_STATUS_JOINED ) == SMTC_MODEM_STATUS_JOINED )
            {
                // Send the uplink ranging message
                multiprotocol_uplink.distance = ( uint16_t ) MIN( ranging_global_results->rng_distance, 0xFFFF );
                multiprotocol_uplink.sf       = radio_lora_params->sf;
                multiprotocol_uplink.bw       = radio_lora_params->bw;
                ASSERT_SMTC_MODEM_RC( smtc_modem_request_uplink( STACK_ID, RANGING_UPLINK_PORT, false,
                                                                 ( uint8_t* ) &multiprotocol_uplink,
                                                                 sizeof( multiprotocol_uplink ) ) );
                last_uplink_timestamp_ms = smtc_modem_hal_get_time_in_ms( );
            }
        }
    }
}

static char* get_priority_str( smtc_rac_priority_t rac_priority )
{
    char* ret = "UNKNOWN";

    switch( rac_priority )
    {
    case RAC_VERY_HIGH_PRIORITY:
        ret = "VERY_HIGH";
        break;
    case RAC_HIGH_PRIORITY:
        ret = "HIGH";
        break;
    case RAC_MEDIUM_PRIORITY:
        ret = "MEDIUM";
        break;
    case RAC_LOW_PRIORITY:
        ret = "LOW";
        break;
    case RAC_VERY_LOW_PRIORITY:
        ret = "VERY_LOW";
        break;
    default:
        break;
    }

    return ret;
}

static smtc_rac_priority_t get_priority_from_str( char* priority )
{
    smtc_rac_priority_t ret = 0;

    if( strcasecmp( priority, "VERY_HIGH" ) == 0 )
    {
        ret = RAC_VERY_HIGH_PRIORITY;
    }
    else if( strcasecmp( priority, "HIGH" ) == 0 )
    {
        ret = RAC_HIGH_PRIORITY;
    }
    else if( strcasecmp( priority, "MEDIUM" ) == 0 )
    {
        ret = RAC_MEDIUM_PRIORITY;
    }
    else if( strcasecmp( priority, "LOW" ) == 0 )
    {
        ret = RAC_LOW_PRIORITY;
    }
    else if( strcasecmp( priority, "VERY_LOW" ) == 0 )
    {
        ret = RAC_VERY_LOW_PRIORITY;
    }

    return ret;
}

/*
 * Custom PM policy to prevent deep sleep while keeping UART operational.
 * This allows LPTIM to work (requires CONFIG_PM=y) but prevents the system
 * from entering STOP modes that would suspend the UART.
 * A proper implementation would involve to wake up on UART rx pin activity and switch between UART and GPIO pinctrl
 * states.
 */
 #ifdef CONFIG_PM_POLICY_CUSTOM
 const struct pm_state_info* pm_policy_next_state( uint8_t cpu, int32_t ticks )
 {
     ARG_UNUSED( cpu );
     ARG_UNUSED( ticks );
 
     /* Return NULL to prevent any sleep - keep CPU running for UART responsiveness.
      * This is the safest approach for the modem bridge application.
      * The LPTIM will still provide accurate timekeeping.
      */
     return NULL;
 }
 #endif

/* --- EOF ------------------------------------------------------------------ */
