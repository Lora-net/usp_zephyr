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

#include <smtc_zephyr_usp_api.h>
#include <smtc_sw_platform_helper.h>

#include "smtc_modem_api.h"
#include <smtc_rac_api.h>
#include "smtc_modem_geolocation_api.h"
#include "smtc_modem_utilities.h"
#include "smtc_modem_hal.h"
#include "lr11xx_system.h"

LOG_MODULE_REGISTER( geolocation_sample, LOG_LEVEL_INF );

/**
 * Stack id value (multistacks modem is not yet available)
 */
#define STACK_ID 0

/**
 * @brief Stack credentials
 */
#if !defined( CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS )
static const uint8_t user_dev_eui[8]  = DT_PROP( DT_PATH( zephyr_user ), user_lorawan_device_eui );
static const uint8_t user_join_eui[8] = DT_PROP( DT_PATH( zephyr_user ), user_lorawan_join_eui );
// static const uint8_t user_gen_app_key[16] = DT_PROP( DT_PATH( zephyr_user ), user_lorawan_gen_app_key );
static const uint8_t user_app_key[16] = DT_PROP( DT_PATH( zephyr_user ), user_lorawan_app_key );
#endif

#define DT_MODEM_REGION( region ) DT_CAT( SMTC_MODEM_REGION_, region )
#define MODEM_EXAMPLE_REGION DT_MODEM_REGION( DT_STRING_UNQUOTED( DT_PATH( zephyr_user ), user_lorawan_region ) )

// Defines for the case where some GPIOs aren't defined
#define LBM_DTS_HAS_LED_SCAN DT_NODE_EXISTS( DT_NODELABEL( lora_scanning_led ) )
#define LBM_DTS_HAS_LNA_CTRL DT_NODE_EXISTS( DT_NODELABEL( lora_gnss_lna_control ) )

// Get the GPIO struct for GNSS LNA directly from shield DTS
#if LBM_DTS_HAS_LED_SCAN
static const struct gpio_dt_spec scanning_led = GPIO_DT_SPEC_GET( DT_NODELABEL( lora_scanning_led ), gpios );
#endif

#if LBM_DTS_HAS_LNA_CTRL
static const struct gpio_dt_spec lna_control = GPIO_DT_SPEC_GET( DT_NODELABEL( lora_gnss_lna_control ), gpios );
#endif

/**
 * @brief User button
 */

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET( DT_ALIAS( smtc_user_button ), gpios );
static struct gpio_callback      button_cb_data;

// A binary semaphore to notify the main LBM loop
K_SEM_DEFINE( button_event_sem, 0, 1 );

/**
 * LEDs
 */

static const struct gpio_dt_spec rx_led = GPIO_DT_SPEC_GET( DT_PATH( leds, lr11xx_rx_led ), gpios );
static const struct gpio_dt_spec tx_led = GPIO_DT_SPEC_GET( DT_PATH( leds, lr11xx_tx_led ), gpios );

/**
 * @brief Watchdog counter reload value during sleep (The period must be lower than MCU watchdog
 * period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS ( 4000 )

#define CUSTOM_NB_TRANS ( 3 )
#define ADR_CUSTOM_LIST                                \
    {                                                  \
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 \
    }

static const uint8_t adr_custom_list[16] = ADR_CUSTOM_LIST;
static const uint8_t custom_nb_trans     = CUSTOM_NB_TRANS;

#define KEEP_ALIVE_PORT ( 2 )
#define KEEP_ALIVE_PERIOD_S ( 3600 / 2 )
#define KEEP_ALIVE_SIZE ( 4 )
uint8_t keep_alive_payload[KEEP_ALIVE_SIZE] = { 0x00 };

/**
 * @brief Defines the delay before starting the next scan sequence, value in [s].
 */
#define GEOLOCATION_GNSS_SCAN_PERIOD_S ( 5 * 60 )
#define GEOLOCATION_WIFI_SCAN_PERIOD_S ( 3 * 60 )

/**
 * @brief Time during which a LED is turned on when pulse, in [ms]
 */
#define LED_PERIOD_MS ( 250 )

/**
 * @brief Supported LR11XX radio firmware
 */
#define LR1110_FW_VERSION 0x0401
#define LR1120_FW_VERSION 0x0201

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

typedef enum almanac_demod_service_state
{
    ADS_STATE_INIT,
    ADS_STATE_STARTED,
    ADS_STATE_STOPPED
} ads_state_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static uint8_t                  rx_payload[SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH] = { 0 };  // Buffer for rx payload
static uint8_t                  rx_payload_size      = 0;               // Size of the payload in the rx_payload buffer
static smtc_modem_dl_metadata_t rx_metadata          = { 0 };           // Metadata of downlink
static uint8_t                  rx_remaining         = 0;               // Remaining downlink payload in modem
static volatile bool            user_button_is_press = false;           /* Flag for button status */
static ads_state_t              ads_state            = ADS_STATE_INIT;  // State of the almanac demodulation service

/**
 * @brief Internal credentials
 */
#if defined( CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS )
static uint8_t chip_eui[SMTC_MODEM_EUI_LENGTH] = { 0 };
static uint8_t chip_pin[SMTC_MODEM_PIN_LENGTH] = { 0 };
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/**
 * @brief User callback for modem event
 *
 *  This callback is called every time an event (see smtc_modem_event_t) appears in the modem.
 *  Several events may have to be read from the modem when this callback is called.
 */
static void modem_event_callback( void );

/**
 * @brief Read the LR11xx firmware version to ensure it is compatible with the almanac update
 */
static bool check_lr11xx_fw_version( void );

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

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/* for zephyr compat*/
void button_pressed( const struct device* dev, struct gpio_callback* cb, uint32_t pins )
{
    printk( "%s", __func__ );
    user_button_callback( dev );
}

/**
 * @brief Example to send a user payload on an external event
 *
 */
int main( void )
{
    if( configure_user_button( ) != 0 )
    {
        LOG_ERR( "Issue when configuring user button, aborting\n" );
        return 1;
    }

    gpio_pin_configure_dt( &rx_led, GPIO_OUTPUT_INACTIVE );
    gpio_pin_configure_dt( &tx_led, GPIO_OUTPUT_INACTIVE );
#if LBM_DTS_HAS_LED_SCAN
    gpio_pin_configure_dt( &scanning_led, GPIO_OUTPUT_INACTIVE );
#endif
#if LBM_DTS_HAS_LNA_CTRL
    gpio_pin_configure_dt( &lna_control, GPIO_OUTPUT_INACTIVE );
#endif

    LOG_INF( "GEOLOCATION example is starting" );

    SMTC_SW_PLATFORM_INIT( );
    SMTC_SW_PLATFORM_VOID( smtc_rac_init( ) );
    // Call smtc_modem_init() after smtc_rac_init()
    // Init the modem and use modem_event_callback as event callback, please note that the callback will be
    // called immediately after the first call to smtc_modem_run_engine because of the reset detection
    SMTC_SW_PLATFORM_VOID( smtc_modem_init( &modem_event_callback ) );

    while( true )
    {
        // Check button
        if( user_button_is_press == true )
        {
            user_button_is_press = false;
            if( ads_state == ADS_STATE_STARTED )
            {
                ads_state = ADS_STATE_STOPPED;
                SMTC_SW_PLATFORM( smtc_modem_almanac_demodulation_stop( STACK_ID ) );
            }
            else
            {
                ads_state = ADS_STATE_STARTED;
                SMTC_SW_PLATFORM( smtc_modem_almanac_demodulation_start( STACK_ID ) );
            }
        }
#if !defined( CONFIG_USP_MAIN_THREAD )
        uint32_t sleep_time_ms = smtc_modem_run_engine( );
        smtc_rac_run_engine( );
        if( smtc_rac_is_irq_flag_pending( ) )
        {
            continue;
        }
        // Allows waking up on radio event or push-button press
        struct k_sem* sems[] = { smtc_modem_hal_get_event_sem( ), &button_event_sem };
        ( void ) wait_on_sems( sems, 2, K_MSEC( MIN( sleep_time_ms, WATCHDOG_RELOAD_PERIOD_MS ) ) );
#else
        if( user_button_is_press == false )
        {
            k_sem_take( &button_event_sem, K_MSEC( WATCHDOG_RELOAD_PERIOD_MS ) );
        }
#endif
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

/**
 * @brief User callback for modem event
 *
 *  This callback is called every time an event (see smtc_modem_event_t) appears in the modem.
 *  Several events may have to be read from the modem when this callback is called.
 */
static void modem_event_callback( void )
{
    smtc_modem_event_t                                          current_event        = { 0 };
    uint8_t                                                     event_pending_count  = 0;
    uint8_t                                                     stack_id             = STACK_ID;
    smtc_modem_gnss_event_data_scan_done_t                      gnss_scan_done_data  = { 0 };
    smtc_modem_gnss_event_data_terminated_t                     gnss_terminated_data = { 0 };
    smtc_modem_almanac_demodulation_event_data_almanac_update_t almanac_update_data  = { 0 };
    smtc_modem_wifi_event_data_scan_done_t                      wifi_scan_done_data  = { 0 };
    smtc_modem_wifi_event_data_terminated_t                     wifi_terminated_data = { 0 };

    // Continue to read modem event until all event has been processed
    do
    {
        // Read modem event
        smtc_modem_get_event( &current_event, &event_pending_count );

        switch( current_event.event_type )
        {
        case SMTC_MODEM_EVENT_RESET:
            LOG_INF( "Event received: RESET" );
            if( check_lr11xx_fw_version( ) != true )
            {
                LOG_ERR( "LR11xx firmware version is not compatible with this example\n" );
                break;
            }

#if !defined( CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS )
            /* Set user credentials */
            smtc_modem_set_deveui( stack_id, user_dev_eui );
            smtc_modem_set_joineui( stack_id, user_join_eui );
            smtc_modem_set_nwkkey( stack_id, user_app_key );
#else
            // Get internal credentials
            smtc_modem_get_chip_eui( stack_id, chip_eui );
            LOG_HEXDUMP_INF( chip_eui, SMTC_MODEM_EUI_LENGTH, "CHIP_EUI" );
            smtc_modem_get_pin( stack_id, chip_pin );
            LOG_HEXDUMP_INF( chip_pin, SMTC_MODEM_PIN_LENGTH, "CHIP_PIN" );
#endif
            /* Set user region */
            smtc_modem_set_region( stack_id, MODEM_EXAMPLE_REGION );
            /* Schedule a Join LoRaWAN network */
            smtc_modem_join_network( stack_id );
            /* Configure almanac demodulation service */
            smtc_modem_almanac_demodulation_set_constellations( stack_id, SMTC_MODEM_GNSS_CONSTELLATION_GPS_BEIDOU );
            /* Set GNSS and Wi-Fi send mode */
            smtc_modem_store_and_forward_flash_clear_data( stack_id );
            smtc_modem_store_and_forward_set_state( stack_id, SMTC_MODEM_STORE_AND_FORWARD_ENABLE );
            smtc_modem_gnss_send_mode( stack_id, SMTC_MODEM_SEND_MODE_STORE_AND_FORWARD );
            smtc_modem_wifi_send_mode( stack_id, SMTC_MODEM_SEND_MODE_UPLINK );
            /* Program Wi-Fi scan */
            smtc_modem_wifi_set_scan_mode( stack_id, SMTC_MODEM_WIFI_SCAN_MODE_MAC );
            smtc_modem_wifi_scan( stack_id, 0 );
            /* Program GNSS scan */
            smtc_modem_gnss_set_constellations( stack_id, SMTC_MODEM_GNSS_CONSTELLATION_GPS_BEIDOU );
            smtc_modem_gnss_scan( stack_id, SMTC_MODEM_GNSS_MODE_MOBILE, 0 );
            /* Notify user with leds */
            gpio_pin_set_dt( &tx_led, 1 );
            break;

        case SMTC_MODEM_EVENT_ALARM:
            LOG_INF( "Event received: ALARM" );
            smtc_modem_request_uplink( stack_id, KEEP_ALIVE_PORT, false, keep_alive_payload, KEEP_ALIVE_SIZE );
            smtc_modem_alarm_start_timer( KEEP_ALIVE_PERIOD_S );
            break;

        case SMTC_MODEM_EVENT_JOINED:
            LOG_INF( "Event received: JOINED" );
            /* Notify user with leds */
            gpio_pin_set_dt( &tx_led, 0 );
            gpio_pin_set_dt( &tx_led, 1 );
            k_msleep( LED_PERIOD_MS );
            gpio_pin_set_dt( &tx_led, 0 );

            /* Set custom ADR profile for geolocation */
            smtc_modem_adr_set_profile( stack_id, SMTC_MODEM_ADR_PROFILE_CUSTOM, adr_custom_list );
            smtc_modem_set_nb_trans( stack_id, custom_nb_trans );
            /* Start time for regular uplink */
            smtc_modem_alarm_start_timer( KEEP_ALIVE_PERIOD_S );
            break;

        case SMTC_MODEM_EVENT_TXDONE:
            LOG_INF( "Event received: TXDONE (%d)", current_event.event_data.txdone.status );
            LOG_INF( "Transmission done" );
            break;

        case SMTC_MODEM_EVENT_DOWNDATA:
            LOG_INF( "Event received: DOWNDATA" );
            // Get downlink data
            smtc_modem_get_downlink_data( rx_payload, &rx_payload_size, &rx_metadata, &rx_remaining );
            LOG_INF( "Data received on port %u", rx_metadata.fport );
            LOG_HEXDUMP_INF( rx_payload, rx_payload_size, "Received payload" );
            break;

        case SMTC_MODEM_EVENT_JOINFAIL:
            LOG_INF( "Event received: JOINFAIL" );
            LOG_WRN( "Join request failed" );
            break;

        case SMTC_MODEM_EVENT_ALCSYNC_TIME:
            LOG_INF( "Event received: TIME" );
            break;

        case SMTC_MODEM_EVENT_GNSS_SCAN_DONE:
            LOG_INF( "Event received: GNSS_SCAN_DONE" );
            /* Get event data */
            smtc_modem_gnss_get_event_data_scan_done( stack_id, &gnss_scan_done_data );
            /* Start almanac demodulation service if the radio is synchronized with GPS time */
            if( ( gnss_scan_done_data.time_available == true ) && ( ads_state == ADS_STATE_INIT ) )
            {
                ads_state = ADS_STATE_STARTED;
                smtc_modem_almanac_demodulation_start( stack_id );
            }
            break;

        case SMTC_MODEM_EVENT_GNSS_TERMINATED:
            LOG_INF( "Event received: GNSS_TERMINATED" );
            /* Notify user with leds */
            gpio_pin_set_dt( &tx_led, 1 );
            k_msleep( LED_PERIOD_MS );
            gpio_pin_set_dt( &tx_led, 0 );
            /* Get event data */
            smtc_modem_gnss_get_event_data_terminated( stack_id, &gnss_terminated_data );
            /* launch next scan */
            smtc_modem_gnss_scan( stack_id, SMTC_MODEM_GNSS_MODE_MOBILE, GEOLOCATION_GNSS_SCAN_PERIOD_S );
            break;

        case SMTC_MODEM_EVENT_GNSS_ALMANAC_DEMOD_UPDATE:
            LOG_INF( "Event received: GNSS_ALMANAC_DEMOD_UPDATE" );
            smtc_modem_almanac_demodulation_get_event_data_almanac_update( stack_id, &almanac_update_data );
            /* Store progress in keep alive payload */
            keep_alive_payload[0] = almanac_update_data.update_progress_gps;
            keep_alive_payload[1] = almanac_update_data.update_progress_beidou;
            LOG_INF( "GPS progress: %u%%", almanac_update_data.update_progress_gps );
            LOG_INF( "BDS progress: %u%%", almanac_update_data.update_progress_beidou );
            LOG_INF( "aborted: %u", almanac_update_data.stat_nb_aborted_by_rp );
            break;

        case SMTC_MODEM_EVENT_WIFI_SCAN_DONE:
            LOG_INF( "Event received: WIFI_SCAN_DONE" );
            /* Get event data */
            smtc_modem_wifi_get_event_data_scan_done( stack_id, &wifi_scan_done_data );
            break;

        case SMTC_MODEM_EVENT_WIFI_TERMINATED:
            LOG_INF( "Event received: WIFI_TERMINATED" );
            /* Notify user with leds */
            gpio_pin_set_dt( &tx_led, 1 );
            k_msleep( LED_PERIOD_MS );
            gpio_pin_set_dt( &tx_led, 0 );
            /* Get event data */
            smtc_modem_wifi_get_event_data_terminated( stack_id, &wifi_terminated_data );
            /* launch next scan */
            smtc_modem_wifi_scan( stack_id, GEOLOCATION_WIFI_SCAN_PERIOD_S );
            break;

        case SMTC_MODEM_EVENT_LINK_CHECK:
            LOG_INF( "Event received: LINK_CHECK" );
            break;

        case SMTC_MODEM_EVENT_CLASS_B_STATUS:
            LOG_INF( "Event received: CLASS_B_STATUS" );
            break;

        case SMTC_MODEM_EVENT_REGIONAL_DUTY_CYCLE:
            LOG_INF( "Event received: SMTC_MODEM_EVENT_REGIONAL_DUTY_CYCLE\n" );
            break;

        default:
            LOG_ERR( "Unknown event %u", current_event.event_type );
            break;
        }
    } while( event_pending_count > 0 );
}

static void user_button_callback( const void* context )
{
    LOG_INF( "Button pushed" );

    ( void ) context;  // Not used in the example - avoid warning

    static uint32_t last_press_timestamp_ms = 0;

    // Debounce the button press, avoid multiple triggers
    if( ( int32_t ) ( smtc_modem_hal_get_time_in_ms( ) - last_press_timestamp_ms ) > 500 )
    {
        last_press_timestamp_ms = smtc_modem_hal_get_time_in_ms( );
        user_button_is_press    = true;
    }
    // Wake up the main thread
    k_sem_give( &button_event_sem );
}

static bool check_lr11xx_fw_version( void )
{
    lr11xx_status_t         status;
    lr11xx_system_version_t lr11xx_fw_version;

    /* suspend modem to get access to the radio */
    smtc_modem_suspend_radio_communications( true );

    status = lr11xx_system_get_version( transceiver, &lr11xx_fw_version );
    if( status != LR11XX_STATUS_OK )
    {
        LOG_ERR( "Failed to get LR11XX firmware version\n" );
        smtc_modem_suspend_radio_communications( false );
        return false;
    }

    if( ( lr11xx_fw_version.type == LR11XX_SYSTEM_VERSION_TYPE_LR1110 ) &&
        ( lr11xx_fw_version.fw < LR1110_FW_VERSION ) )
    {
        LOG_ERR( "Wrong LR1110 firmware version, expected 0x%04X, got 0x%04X\n", LR1110_FW_VERSION,
                 lr11xx_fw_version.fw );
        smtc_modem_suspend_radio_communications( false );
        return false;
    }
    if( ( lr11xx_fw_version.type == LR11XX_SYSTEM_VERSION_TYPE_LR1120 ) &&
        ( lr11xx_fw_version.fw < LR1120_FW_VERSION ) )
    {
        LOG_ERR( "Wrong LR1120 firmware version, expected 0x%04X, got 0x%04X\n", LR1120_FW_VERSION,
                 lr11xx_fw_version.fw );
        smtc_modem_suspend_radio_communications( false );
        return false;
    }

    /* release radio to the modem */
    smtc_modem_suspend_radio_communications( false );
    LOG_INF( "LR11XX FW: 0x%04X, type: 0x%02X\n", lr11xx_fw_version.fw, lr11xx_fw_version.type );
    return true;
}

/* --- EOF ------------------------------------------------------------------ */
