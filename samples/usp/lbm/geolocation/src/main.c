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

#include "smtc_modem_api.h"
#include <smtc_rac_api.h>
#include "smtc_modem_geolocation_api.h"
#include "smtc_modem_utilities.h"
#include "lr11xx_system.h"

LOG_MODULE_REGISTER( geolocation_sample, LOG_LEVEL_INF );

/**
 * Stack id value (multistacks modem is not yet available)
 */
#define STACK_ID 0

/* lr11xx radio context and its use in the ralf layer */
static const struct device* transceiver = DEVICE_DT_GET( DT_ALIAS( lora_transceiver ) );

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
#define ADR_CUSTOM_LIST { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 }

static const uint8_t adr_custom_list[16] = ADR_CUSTOM_LIST;
static const uint8_t custom_nb_trans     = CUSTOM_NB_TRANS;

#define KEEP_ALIVE_PORT ( 2 )
#define KEEP_ALIVE_PERIOD_S ( 30 )
#define KEEP_ALIVE_SIZE ( 4 )
uint8_t keep_alive_payload[KEEP_ALIVE_SIZE] = { 0x00 };

/**
 * @brief Defines the delay before starting the next scan sequence, value in [s].
 */
#define GEOLOCATION_GNSS_SCAN_PERIOD_S ( 2 * 60 )
#define GEOLOCATION_WIFI_SCAN_PERIOD_S ( 1 * 60 )

/**
 * @brief Time during which a LED is turned on when pulse, in [ms]
 */
#define LED_PERIOD_MS ( 250 )

/**
 * @brief Supported LR11XX radio firmware
 */
#define LR1110_FW_VERSION 0x0401
#define LR1120_FW_VERSION 0x0201

/* Buffer for rx payload */
static uint8_t rx_payload[SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH] = { 0 };
/* Size of the payload in the rx_payload buffer */
static uint8_t rx_payload_size;
/* Metadata of downlink */
static smtc_modem_dl_metadata_t rx_metadata = { 0 };
/* Remaining downlink payload in modem */
static uint8_t rx_remaining;

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

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/*
static uint32_t uplink_counter; // uplink raising counter

static void send_uplink_counter_on_port( uint8_t port )
{
   // Send uplink counter on port 102
   uint8_t buff[4] = { 0 };

   buff[0] = ( uplink_counter >> 24 ) & 0xFF;
   buff[1] = ( uplink_counter >> 16 ) & 0xFF;
   buff[2] = ( uplink_counter >> 8 ) & 0xFF;
   buff[3] = ( uplink_counter & 0xFF );
   smtc_modem_request_uplink( STACK_ID, port, false, buff, 4 );
   // Increment uplink counter
   uplink_counter++;
}
*/

/**
 * @brief Example to send a user payload on an external event
 *
 */
int main( void )
{
    uint32_t                sleep_time_ms = 0;
    lr11xx_system_version_t lr11xx_fw_version;
    lr11xx_status_t         status;

    gpio_pin_configure_dt( &rx_led, GPIO_OUTPUT_INACTIVE );
    gpio_pin_configure_dt( &tx_led, GPIO_OUTPUT_INACTIVE );
#if LBM_DTS_HAS_LED_SCAN
    gpio_pin_configure_dt( &scanning_led, GPIO_OUTPUT_INACTIVE );
#endif
#if LBM_DTS_HAS_LNA_CTRL
    gpio_pin_configure_dt( &lna_control, GPIO_OUTPUT_INACTIVE );
#endif

    lorawan_smtc_modem_hal_init( transceiver );
    smtc_rac_init( );
    smtc_modem_init( modem_event_callback );

    k_msleep( 400 );

    LOG_INF( "GEOLOCATION example is starting" );

    /* Check LR11XX Firmware version */
    status = lr11xx_system_get_version( transceiver, &lr11xx_fw_version );
    if( status != LR11XX_STATUS_OK )
    {
        LOG_ERR( "Failed to get LR11XX firmware version" );
    }

    if( ( lr11xx_fw_version.type == LR11XX_SYSTEM_VERSION_TYPE_LR1110 ) &&
        ( lr11xx_fw_version.fw < LR1110_FW_VERSION ) )
    {
        LOG_ERR( "Wrong LR1110 firmware version, expected 0x%04X, got 0x%04X", LR1110_FW_VERSION,
                 lr11xx_fw_version.fw );
    }

    if( ( lr11xx_fw_version.type == LR11XX_SYSTEM_VERSION_TYPE_LR1120 ) &&
        ( lr11xx_fw_version.fw < LR1120_FW_VERSION ) )
    {
        LOG_ERR( "Wrong LR1120 firmware version, expected 0x%04X, got 0x%04X", LR1120_FW_VERSION,
                 lr11xx_fw_version.fw );
    }

    LOG_INF( "LR11XX FW: 0x%04X, type: 0x%02X", lr11xx_fw_version.fw, lr11xx_fw_version.type );

    while( true )
    {
        /* Modem process launch */
        sleep_time_ms = smtc_modem_run_engine( );

        /* Atomically check sleep conditions */
        if( !smtc_modem_is_irq_flag_pending( ) )
        {
            smtc_modem_hal_interruptible_msleep( K_MSEC( MIN( sleep_time_ms, WATCHDOG_RELOAD_PERIOD_MS ) ) );
        }
        // send_uplink_counter_on_port( 102 );
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

/**
 * @brief User callback for modem event
 *
 *  This callback is called every time an event (see smtc_modem_event_t) appears in the modem.
 *  Several events may have to be read from the modem when this callback is called.
 */
static void modem_event_callback( void )
{
    LOG_INF( "get_event () callback" );

    smtc_modem_event_t                                          current_event;
    uint8_t                                                     event_pending_count;
    uint8_t                                                     stack_id = STACK_ID;
    smtc_modem_gnss_event_data_scan_done_t                      scan_done_data;
    smtc_modem_gnss_event_data_terminated_t                     terminated_data;
    smtc_modem_almanac_demodulation_event_data_almanac_update_t almanac_update_data;
    smtc_modem_wifi_event_data_scan_done_t                      wifi_scan_done_data;
    smtc_modem_wifi_event_data_terminated_t                     wifi_terminated_data;

    /* Continue to read modem event until all event has been processed */
    do
    {
        /* Read modem event */
        smtc_modem_get_event( &current_event, &event_pending_count );

        switch( current_event.event_type )
        {
        case SMTC_MODEM_EVENT_RESET:
            LOG_INF( "Event received: RESET" );

#if !defined( CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS )
            /* Set user credentials */
            smtc_modem_set_deveui( stack_id, user_dev_eui );
            smtc_modem_set_joineui( stack_id, user_join_eui );
            smtc_modem_set_nwkkey( stack_id, user_app_key );
#else
            /* Get internal credentials */
            smtc_modem_get_chip_eui( stack_id, chip_eui );
            SMTC_HAL_TRACE_ARRAY( "CHIP_EUI", chip_eui, SMTC_MODEM_EUI_LENGTH );
            smtc_modem_get_pin( stack_id, chip_pin );
            SMTC_HAL_TRACE_ARRAY( "CHIP_PIN", chip_pin, SMTC_MODEM_PIN_LENGTH );
#endif
            /* Set user region */
            smtc_modem_set_region( stack_id, MODEM_EXAMPLE_REGION );
            /* Schedule a Join LoRaWAN network */
            smtc_modem_join_network( stack_id );
            /* Start almanac demodulation service */
            smtc_modem_almanac_demodulation_set_constellations( stack_id, SMTC_MODEM_GNSS_CONSTELLATION_GPS_BEIDOU );
            smtc_modem_almanac_demodulation_start( stack_id );
            /* Set GNSS and Wi-Fi send mode */
            smtc_modem_store_and_forward_flash_clear_data( stack_id );
            smtc_modem_store_and_forward_set_state( stack_id, SMTC_MODEM_STORE_AND_FORWARD_ENABLE );
            smtc_modem_gnss_send_mode( stack_id,
                                       /*SMTC_MODEM_SEND_MODE_STORE_AND_FORWARD*/ SMTC_MODEM_SEND_MODE_UPLINK );
            smtc_modem_wifi_send_mode( stack_id, SMTC_MODEM_SEND_MODE_UPLINK );
            /* Program Wi-Fi scan */
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
            /* Get downlink data */
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
            smtc_modem_gnss_get_event_data_scan_done( stack_id, &scan_done_data );
            break;

        case SMTC_MODEM_EVENT_GNSS_TERMINATED:
            LOG_INF( "Event received: GNSS_TERMINATED" );
            /* Notify user with leds */
            gpio_pin_set_dt( &tx_led, 1 );
            k_msleep( LED_PERIOD_MS );
            gpio_pin_set_dt( &tx_led, 0 );
            /* Get event data */
            smtc_modem_gnss_get_event_data_terminated( stack_id, &terminated_data );
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

/* --- EOF ------------------------------------------------------------------ */
