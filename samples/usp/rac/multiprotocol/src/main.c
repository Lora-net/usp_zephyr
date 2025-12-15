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
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_output_custom.h>
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

#include <app_ranging_hopping.h>
#include <main_ranging_demo.h>

LOG_MODULE_REGISTER( usp, LOG_LEVEL_INF );

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
 * @def STACK_ID
 * @brief Define the stack id value (multistacks modem is not yet available)
 */
#define STACK_ID 0

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
 * @def UNIX_GPS_EPOCH_OFFSET
 * @brief Define the offset between Unix epoch and GPS epoch in seconds
 */
#define UNIX_GPS_EPOCH_OFFSET 315964800

/**
 * @def RANGING_UPLINK_MAX_RATE
 * @brief Define the maximum rate for ranging uplink messages (in ms)
 */
#define RANGING_UPLINK_MAX_RATE 60000

/**
 * @brief Stack credentials
 */
#if !defined( CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS )
static const uint8_t user_dev_eui[8]      = DT_PROP( DT_PATH( zephyr_user ), user_lorawan_device_eui );
static const uint8_t user_join_eui[8]     = DT_PROP( DT_PATH( zephyr_user ), user_lorawan_join_eui );
static const uint8_t user_gen_app_key[16] = DT_PROP( DT_PATH( zephyr_user ), user_lorawan_gen_app_key );
static const uint8_t user_app_key[16]     = DT_PROP( DT_PATH( zephyr_user ), user_lorawan_app_key );
#endif

#define DT_MODEM_REGION( region ) DT_CAT( SMTC_MODEM_REGION_, region )

#define MODEM_REGION DT_MODEM_REGION( DT_STRING_UNQUOTED( DT_PATH( zephyr_user ), user_lorawan_region ) )

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
static uint8_t                  rx_payload[SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH] = { 0 }; /* Buffer for rx payload */
static uint8_t                  rx_payload_size = 0;     /* Size of the payload in the rx_payload buffer */
static smtc_modem_dl_metadata_t rx_metadata     = { 0 }; /* Metadata of downlink */
static uint8_t                  rx_remaining    = 0;     /* Remaining downlink payload in modem */

#if defined( CONFIG_LORA_BASICS_MODEM_RELAY_TX )
static smtc_modem_relay_tx_config_t relay_config = { 0 };
#endif
/**
 * @brief Internal credentials
 */
#if defined( CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS )
static uint8_t chip_eui[SMTC_MODEM_EUI_LENGTH] = { 0 };
static uint8_t chip_pin[SMTC_MODEM_PIN_LENGTH] = { 0 };
#endif
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
 * @brief User callback for modem event
 *
 *  This callback is called every time an event ( see smtc_modem_event_t ) appears in the modem.
 *  Several events may have to be read from the modem when this callback is called.
 */
static void modem_event_callback( void );

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

#if !defined( CONFIG_USP_MAIN_THREAD )
static int wait_on_sems_and_event( struct k_sem* sems[], size_t sem_count, struct k_event* event, uint32_t event_mask,
                                   k_timeout_t timeout );
#endif

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

/**
 * @brief User get timestamp for logging
 *
 *  This function is called when a timestamp is requested by the logging module.

 *  @return a timestamp value @see log_timestamp_t
 */
log_timestamp_t log_timestamp( void );

/**
 * @brief User timestamp formatter for logging
 *
 *  This function is called every time a timestamp need to be formated by the logging module.
 *  @param output A pointer to the log output instance @see log_output
 *  @param timestamp The timestamp value @see log_timestamp_t
 *  @param printer Prototype of a printer function that can print the given timestamp @see log_timestamp_printer_t
 */
int custom_timestamp_formatter( const struct log_output* output, const log_timestamp_t timestamp,
                                const log_timestamp_printer_t printer );

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

    log_set_timestamp_func( log_timestamp, 1000 );
    log_custom_timestamp_set( custom_timestamp_formatter );
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

    set_led( SMTC_PF_LED_TX, true );
    set_led( SMTC_PF_LED_RX, false );

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

#if !defined( CONFIG_USP_MAIN_THREAD )
/**
 * @brief Wait on semaphores and event simultaneously
 *
 * @param sems Array of semaphore pointers
 * @param sem_count Number of semaphores
 * @param event Event object pointer
 * @param event_mask Event mask to wait for
 * @param timeout Timeout value
 * @return int Index of triggered semaphore (0-based), or negative for event/error
 *         -1: Parameter error
 *         -2: Timeout
 *         -3: No signal (should not happen)
 *         -4: Event triggered (check event_mask)
 */
static int wait_on_sems_and_event( struct k_sem* sems[], size_t sem_count, struct k_event* event, uint32_t event_mask,
                                   k_timeout_t timeout )
{
    if( ( sem_count == 0 || sems == NULL ) && event == NULL )
    {
        return -1;  // Parameter error
    }

    // Compute total number of events to wait on
    size_t              total_count = sem_count + ( event ? 1 : 0 );
    struct k_poll_event poll_events[total_count];
    size_t              event_index = 0;

    // Initialize semaphore events
    for( size_t i = 0; i < sem_count; i++ )
    {
        k_poll_event_init( &poll_events[event_index], K_POLL_TYPE_SEM_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY, sems[i] );
        event_index++;
    }

    // Initialize the event if provided
    if( event != NULL )
    {
        k_poll_event_init( &poll_events[event_index], K_POLL_TYPE_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY, event );
        poll_events[event_index].tag = event_mask;  // Stocker le masque dans tag
    }

    // Wait for any object to be signaled
    int ret = k_poll( poll_events, total_count, timeout );
    if( ret != 0 )
    {
        return -2;  // Timeout or error
    }

    // Check which semaphore was signaled
    for( size_t i = 0; i < sem_count; i++ )
    {
        if( poll_events[i].state == K_POLL_STATE_SEM_AVAILABLE )
        {
            k_sem_take( sems[i], K_NO_WAIT );
            return ( int ) i;  // Return the index of the semaphore
        }
    }

    // Check if event was signaled
    if( event != NULL && poll_events[sem_count].state == K_POLL_STATE_DATA_AVAILABLE )
    {
        // Check which events are set
        uint32_t current_events = k_event_test( event, event_mask );
        if( current_events & event_mask )
        {
            return -4;
        }
    }

    return -3;  // No signal, issue
}
#endif

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

//  WARNING : Run in USP/RAC Thread if compiled with threads
static void modem_event_callback( void )
{
    LOG_DBG( "Modem event callback" );

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
            LOG_INF( "Event received: RESET" );

#if !defined( CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS )
            /* Set user credentials */
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_deveui( stack_id, user_dev_eui ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_joineui( stack_id, user_join_eui ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_appkey( stack_id, user_gen_app_key ) );
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_nwkkey( stack_id, user_app_key ) );
#else
            /* Get internal credentials */
            ASSERT_SMTC_MODEM_RC( smtc_modem_get_chip_eui( stack_id, chip_eui ) );
            SMTC_HAL_TRACE_ARRAY( "CHIP_EUI", chip_eui, SMTC_MODEM_EUI_LENGTH );
            ASSERT_SMTC_MODEM_RC( smtc_modem_get_pin( stack_id, chip_pin ) );
            SMTC_HAL_TRACE_ARRAY( "CHIP_PIN", chip_pin, SMTC_MODEM_PIN_LENGTH );
#endif
            /* Set user region */
            ASSERT_SMTC_MODEM_RC( smtc_modem_set_region( stack_id, MODEM_REGION ) );
            /* Schedule a Join LoRaWAN network */
#if defined( CONFIG_LORA_BASICS_MODEM_RELAY_TX )
            /* by default when relay mode is activated , CSMA is also activated by
             * default to at least protect the WOR transmission if you want to disable
             * the csma please uncomment the next line
             */
            /* ASSERT_SMTC_MODEM_RC(smtc_modem_csma_set_state (stack_id,false)); */

            relay_config.second_ch_enable = false;

            /* The RelayModeActivation field indicates how the end-device SHOULD manage
             * the relay mode.
             */
            relay_config.activation = SMTC_MODEM_RELAY_TX_ACTIVATION_MODE_DYNAMIC;

            /* number_of_miss_wor_ack_to_switch_in_nosync_mode  field indicates that the
             * relay mode SHALL be restart in no sync mode when it does not receive a
             * WOR ACK frame after number_of_miss_wor_ack_to_switch_in_nosync_mode
             * consecutive uplinks.
             */
            relay_config.number_of_miss_wor_ack_to_switch_in_nosync_mode = 3;

            /* smart_level field indicates that the relay mode SHALL be enabled if
             * the end-device does not receive a valid downlink after
             * smart_level consecutive uplinks.
             */
            relay_config.smart_level = 8;

            /* The BackOff field indicates how the end-device SHALL behave when it does
             * not receive a WOR ACK frame. BackOff Description 0 Always send a LoRaWAN
             * uplink 1..63 Send a LoRaWAN uplink after X WOR frames without a WOR ACK
             */
            relay_config.backoff = 4;
            ASSERT_SMTC_MODEM_RC( smtc_modem_relay_tx_enable( stack_id, &relay_config ) );
#endif
            ASSERT_SMTC_MODEM_RC( smtc_modem_join_network( stack_id ) );
            break;

        case SMTC_MODEM_EVENT_ALARM:
            LOG_INF( "Event received: ALARM" );
            /* Send keep-alive */
            smtc_modem_request_empty_uplink( STACK_ID, true, KEEP_ALIVE_PORT, false );
            /* Restart periodical uplink alarm */
            ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( PERIODICAL_UPLINK_DELAY_S ) );
            break;

        case SMTC_MODEM_EVENT_JOINED:
            LOG_INF( "Event received: JOINED" );
            LOG_INF( "Modem is now joined " );
            ASSERT_SMTC_MODEM_RC(
                smtc_modem_trig_lorawan_mac_request( STACK_ID, SMTC_MODEM_LORAWAN_MAC_REQ_DEVICE_TIME ) );
            /* start periodical uplink alarm */
            ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( DELAY_FIRST_MSG_AFTER_JOIN ) );
            break;

        case SMTC_MODEM_EVENT_TXDONE:
            LOG_INF( "Event received: TXDONE" );
            LOG_INF( "Transmission done " );
            break;

        case SMTC_MODEM_EVENT_DOWNDATA:
            LOG_INF( "Event received: DOWNDATA" );
            /* Get downlink data */
            ASSERT_SMTC_MODEM_RC(
                smtc_modem_get_downlink_data( rx_payload, &rx_payload_size, &rx_metadata, &rx_remaining ) );

            LOG_DBG( "Data received on port %u", rx_metadata.fport );
            LOG_HEXDUMP_DBG( rx_payload, rx_payload_size, "Received payload" );
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
            if( current_event.event_data.no_downlink.status != 0 )
            {
                // Leave and re-join network
                smtc_modem_alarm_clear_timer( );
                ASSERT_SMTC_MODEM_RC( smtc_modem_leave_network( STACK_ID ) );
                ASSERT_SMTC_MODEM_RC( smtc_modem_join_network( STACK_ID ) );
                LOG_INF( "Event received: %s-%s\n",
                         current_event.event_data.no_downlink.status & SMTC_MODEM_EVENT_NO_RX_THRESHOLD_ADR_BACKOFF_END
                             ? "ADR backoff end-"
                             : "",
                         current_event.event_data.no_downlink.status & SMTC_MODEM_EVENT_NO_RX_THRESHOLD_USER_THRESHOLD
                             ? "-User threshold reached"
                             : "" );
            }
            else  // Event cleared
            {
                LOG_INF( "Event type: Cleared\n" );
            }
            break;
        }
        case SMTC_MODEM_EVENT_TEST_MODE:
        {
            uint8_t status_test_mode = current_event.event_data.test_mode_status.status;
            char*   status_name[]    = { "SMTC_MODEM_EVENT_TEST_MODE_ENDED", "SMTC_MODEM_EVENT_TEST_MODE_TX_COMPLETED",
                                         "SMTC_MODEM_EVENT_TEST_MODE_TX_DONE", "SMTC_MODEM_EVENT_TEST_MODE_RX_DONE",
                                         "SMTC_MODEM_EVENT_TEST_MODE_RX_ABORTED" };
            // When aborted, do not print the status to avoid log flooding
            if( status_test_mode < SMTC_MODEM_EVENT_TEST_MODE_RX_ABORTED )
            {
                LOG_DBG( "Event received: TEST_MODE :  %s\n", status_name[status_test_mode] );
            }
            if( status_test_mode == SMTC_MODEM_EVENT_TEST_MODE_RX_DONE )
            {
                int16_t rssi;
                int16_t snr;
                uint8_t rx_payload_length;
                smtc_modem_test_get_last_rx_packets( &rssi, &snr, rx_payload, &rx_payload_length );
                LOG_HEXDUMP_DBG( rx_payload, rx_payload_length, "rx_payload" );
                LOG_INF( "rssi: %d, snr: %d\n", rssi, snr );
            }
            break;
        }

        default:
            LOG_ERR( "Unknown event %u", current_event.event_type );
            break;
        }
    } while( event_pending_count > 0 );
}

log_timestamp_t log_timestamp( void )
{
    uint32_t        gps_time_s       = 0;
    uint32_t        gps_fractional_s = 0;
    log_timestamp_t ret              = 0;

    if( smtc_modem_get_lorawan_mac_time( STACK_ID, &gps_time_s, &gps_fractional_s ) == SMTC_MODEM_RC_OK )
    {
        ret = ( log_timestamp_t ) gps_time_s;
        ret += UNIX_GPS_EPOCH_OFFSET;
        ret *= 1000;
        ret += ( gps_fractional_s );
    }
    else
    {
        ret = ( log_timestamp_t ) UNIX_GPS_EPOCH_OFFSET;
        ret *= 1000;
        ret += ( log_timestamp_t ) ( smtc_modem_hal_get_time_in_ms( ) );
    }

    return ret;
}

int custom_timestamp_formatter( const struct log_output* output, const log_timestamp_t timestamp,
                                const log_timestamp_printer_t printer )
{
    // Convertir le timestamp en secondes et millisecondes
    uint64_t   timestamp_ms = ( uint64_t ) timestamp;
    uint32_t   seconds      = timestamp_ms / 1000;
    uint32_t   milliseconds = timestamp_ms % 1000;
    time_t     unix_time    = seconds;
    struct tm* timeinfo     = gmtime( &unix_time );

    if( timeinfo != NULL )
    {
        return printer( output, "[%04d-%02d-%02d %02d:%02d:%02d.%03u] ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                        timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, milliseconds );
    }
    else
    {
        return printer( output, "[%u.%03u] ", seconds, milliseconds );
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

/* --- EOF ------------------------------------------------------------------ */
