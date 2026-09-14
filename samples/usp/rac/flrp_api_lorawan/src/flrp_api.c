/*!
 * \file      flrp_api.c
 *
 * \brief     Test functions for FLRP API example
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
#include <stdint.h>   // C99 types
#include <stdbool.h>  // bool type
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <smtc_flrp_api.h>
#include <smtc_flrp_crypto.h>
#include <flrp_configuration.h>
#include "flrp_api.h"
#include "flrp_display.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

LOG_MODULE_REGISTER( flrp_api_tools, LOG_LEVEL_INF );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define DATA_SIZE ( 20 * 1024 )

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */
typedef struct flrp_api_message_s
{
    uint16_t counter;
    uint8_t  data[DATA_SIZE];
} flrp_api_message_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */
static uint16_t flrp_api_tx_counter = 0;

static flrp_api_message_t data_flrp = { 0 };

/* Cumulative reception stats */
static uint32_t total_nb_packets_received_ok  = 0;
static uint32_t total_nb_packets_check_error  = 0;
static uint32_t total_nb_packets_received_nok = 0;
static int32_t  last_rssi_mean                = 0;
static uint32_t total_payload_size            = 0;
static uint32_t total_payload_size_expected   = 0;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

static void user_rx_callback( const void* context, smtc_flrp_return_code_t status, uint32_t payload_size,
                              uint8_t* src_addr, smtc_flrp_rx_stats_t payload_stats );
static void user_tx_callback( const void* context, smtc_flrp_return_code_t err_code, bool send_successful,
                              uint8_t* dest_addr );
static void flrp_api_setup_tx_buffer( void );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

bool flrp_api_init( uint8_t* device_eui, uint8_t* key, bool crypto_enabled, bool is_low_frequency, bool listening )
{
    smtc_flrp_api_config_t  flrp_config;
    smtc_flrp_return_code_t return_code;
    bool                    ret = true;

    flrp_config.crypto_enabled = crypto_enabled;
    flrp_config.freq_plan      = is_low_frequency ? SMTC_FLRP_FREQ_865MHz : SMTC_FLRP_FREQ_2GHz4;
    flrp_config.crystal_error  = 10;
    memcpy( ( void* ) flrp_config.dev_eui, ( void* ) device_eui, SMTC_FLRP_EUI_LENGTH );
    return_code = smtc_flrp_init( flrp_config, user_tx_callback, user_rx_callback, NULL );

    smtc_flrp_crypto_init( );
    ret = flrp_api_set_key( key );

    if( listening == true )
    {
        return_code |= smtc_flrp_start_periodic_listening( ( uint8_t* ) &data_flrp, sizeof( data_flrp ) );
        memset( ( void* ) &data_flrp, 0x00, sizeof( data_flrp ) );
        for( uint32_t i = 0; i < sizeof( data_flrp.data ); i++ )
        {
            data_flrp.data[i] = ( uint8_t ) i;
        }
        return_code |= smtc_flrp_slave_prepare_data_to_send( ( uint8_t* ) &data_flrp, sizeof( data_flrp ) );
    }

    if( ( return_code != SMTC_FLRP_RC_OK ) || ( ret != true ) )
    {
        ret = false;
        LOG_ERR( "Failed to start periodic RX (error %u)", return_code );
    }
    else
    {
        flrp_display_message( 0, 0, FLRP_DISPLAY_FONT_SIZE_SMALL, "FLRP %02X%02X%02X%02X%02X%02X%02X%02X->...",
                              device_eui[0], device_eui[1], device_eui[2], device_eui[3], device_eui[4], device_eui[5],
                              device_eui[6], device_eui[7] );
    }
    return ret;
}

bool flrp_api_initiate_tx( uint8_t* target_device_eui )
{
    bool ret = true;

    smtc_flrp_com_config_t com_config = {
        .com_mode             = SMTC_FLRP_BIDIRECTIONAL,
        .link_adaptation_mode = SMTC_FLRP_LINK_ADAPTATION_CHANNEL_SELECTION_ONLY,
    };
    memcpy( ( void* ) com_config.slave_dev_eui, ( void* ) target_device_eui, SMTC_FLRP_EUI_LENGTH );
    flrp_api_setup_tx_buffer( );

    smtc_flrp_return_code_t return_code =
        smtc_flrp_initiate_transmission( ( uint8_t* ) &data_flrp, sizeof( data_flrp ), com_config );
    if( return_code != SMTC_FLRP_RC_OK )
    {
        LOG_ERR( "Failed to initiate transmission (error %u)\n", return_code );
        ret = false;
    }

    return ret;
}

bool flrp_api_initiate_rx( uint8_t* target_device_eui )
{
    bool ret = true;

    smtc_flrp_com_config_t com_config = {
        .com_mode             = SMTC_FLRP_BIDIRECTIONAL,
        .link_adaptation_mode = SMTC_FLRP_LINK_ADAPTATION_CHANNEL_SELECTION_ONLY,
    };
    memcpy( ( void* ) com_config.slave_dev_eui, ( void* ) target_device_eui, SMTC_FLRP_EUI_LENGTH );
    memset( ( void* ) &data_flrp.data, 0x00, sizeof( data_flrp.data ) );

    smtc_flrp_return_code_t return_code =
        smtc_flrp_initiate_reception( ( uint8_t* ) &data_flrp, sizeof( data_flrp ), com_config );
    if( return_code != SMTC_FLRP_RC_OK )
    {
        LOG_ERR( "Failed to initiate reception (error %u)\n", return_code );
        ret = false;
    }

    return ret;
}

bool flrp_api_set_key( uint8_t* key )
{
    bool ret = true;

    smtc_flrp_crypto_return_code_t crypto_return_code = smtc_flrp_crypto_set_key( SMTC_SE_APP_KEY, key, 0 );
    if( crypto_return_code != SMTC_FLRP_CRYPTO_RC_SUCCESS )
    {
        LOG_ERR( "Failed to set key (error %u)\n", crypto_return_code );
        ret = false;
    }

    return ret;
}

void flrp_api_get_burst_target_per( uint8_t* burst_target_per )
{
    smtc_flrp_radio_config_t cfg = smtc_flrp_get_current_radio_config( );

    *burst_target_per = cfg.flrc.burst_target_per;
}

bool flrp_api_set_burst_target_per( uint8_t burst_target_per, bool restart_listening )
{
    smtc_flrp_return_code_t  return_code;
    smtc_flrp_radio_config_t cfg;

    smtc_flrp_stop_periodic_listening( );
    // No trigger to signal end of wor reception available yet
    k_sleep( K_MSEC( RESTART_WOR_RX_DELAY_MS *
                     2 ) );  // Ensure the previous listening has stopped before applying new config
    cfg                       = smtc_flrp_get_current_radio_config( );
    cfg.flrc.burst_target_per = burst_target_per;
    return_code               = smtc_flrp_set_new_radio_config( cfg );
    if( restart_listening == true )
    {
        return_code |= smtc_flrp_start_periodic_listening( ( uint8_t* ) &data_flrp, sizeof( data_flrp ) );
    }

    return ( bool ) ( return_code == SMTC_FLRP_RC_OK );
}

bool flrp_api_start_listening( void )
{
    bool ret = true;

    smtc_flrp_return_code_t return_code =
        smtc_flrp_start_periodic_listening( ( uint8_t* ) &data_flrp, sizeof( data_flrp ) );
    if( return_code != SMTC_FLRP_RC_OK )
    {
        LOG_ERR( "Failed to start periodic listening (error %u)\n", return_code );
        ret = false;
    }

    return ret;
}

bool flrp_api_stop_listening( void )
{
    bool ret = true;

    smtc_flrp_return_code_t return_code = smtc_flrp_stop_periodic_listening( );
    if( return_code != SMTC_FLRP_RC_OK )
    {
        LOG_ERR( "Failed to stop periodic listening (error %u)\n", return_code );
        ret = false;
    }

    return ret;
}

void flrp_api_display_rx_stats( void )
{
    LOG_INF( "Num packets OK         : %u", total_nb_packets_received_ok );
    LOG_INF( "Num packets check error: %u", total_nb_packets_check_error );
    LOG_INF( "Num packets NOK        : %u", total_nb_packets_received_nok );
    LOG_INF( "Total packets received : %u",
             total_nb_packets_received_ok + total_nb_packets_check_error + total_nb_packets_received_nok );
    LOG_INF( "Total received size    : %u", total_payload_size );
    LOG_INF( "Total expected size    : %u", total_payload_size_expected );
    LOG_INF( "Last burst mean RSSI   : %d dBm", last_rssi_mean );
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static void user_rx_callback( const void* context, smtc_flrp_return_code_t status, uint32_t payload_size,
                              uint8_t* src_addr, smtc_flrp_rx_stats_t rx_stats )
{
    if( src_addr != NULL )
    {
        LOG_INF( "user_rx_callback %d %d 0x%02X%02X%02X%02X%02X%02X%02X%02X %d", status, payload_size, src_addr[0],
                 src_addr[1], src_addr[2], src_addr[3], src_addr[4], src_addr[5], src_addr[6], src_addr[7],
                 data_flrp.counter );
    }
    else
    {
        LOG_INF( "user_rx_callback %d %d (no src_addr) %d", status, payload_size, data_flrp.counter );
    }
    LOG_INF(
        "RX stats: nb pkt OK=%" PRIu32 ", nb pkt err=%" PRIu32 ", nb pkt Nok=%" PRIu32 ", "
        "rssi burst=%" PRId32 ", expected payload size=%" PRIu32 ", wor rssi=%" PRId16 ", wor snr=%" PRId8,
        rx_stats.burst.nb_packets_received_ok, rx_stats.burst.nb_packets_check_error,
        rx_stats.burst.nb_packets_received_nok, rx_stats.burst.rssi_mean, rx_stats.burst.payload_size_expected,
        rx_stats.wor.rssi, rx_stats.wor.snr );

    total_nb_packets_received_ok += rx_stats.burst.nb_packets_received_ok;
    total_nb_packets_check_error += rx_stats.burst.nb_packets_check_error;
    total_nb_packets_received_nok += rx_stats.burst.nb_packets_received_nok;
    last_rssi_mean = rx_stats.burst.rssi_mean;
    total_payload_size_expected += rx_stats.burst.payload_size_expected;
    total_payload_size += payload_size;

    if( ( status == SMTC_FLRP_RC_OK ) && ( payload_size > sizeof( data_flrp.counter ) ) )
    {
        LOG_HEXDUMP_INF( data_flrp.data, MIN( 64, payload_size - sizeof( data_flrp.counter ) ),
                         "Received displayed data" );
        flrp_display_message( 0, 4, FLRP_DISPLAY_FONT_SIZE_BIG, " Rx   : %05d", data_flrp.counter );
    }
    flrp_api_setup_tx_buffer( );
}

static void user_tx_callback( const void* context, smtc_flrp_return_code_t err_code, bool send_successful,
                              uint8_t* dest_addr )
{
    if( dest_addr != NULL )
    {
        LOG_INF( "user_tx_callback %d %d 0x%02X%02X%02X%02X%02X%02X%02X%02X %d", err_code, send_successful,
                 dest_addr[0], dest_addr[1], dest_addr[2], dest_addr[3], dest_addr[4], dest_addr[5], dest_addr[6],
                 dest_addr[7], data_flrp.counter );
    }
    else
    {
        LOG_INF( "user_tx_callback %d %d (no dest_addr) %d", err_code, send_successful, data_flrp.counter );
    }

    if( ( err_code == SMTC_FLRP_RC_OK ) && ( send_successful == true ) )
    {
        LOG_HEXDUMP_INF( data_flrp.data, MIN( 64, sizeof( data_flrp.data ) ), "Transmitted displayed data" );
        flrp_display_message( 0, 6, FLRP_DISPLAY_FONT_SIZE_BIG, " Tx   : %05d", data_flrp.counter );
        flrp_api_tx_counter++;
    }
    flrp_api_setup_tx_buffer( );
}

static void flrp_api_setup_tx_buffer( void )
{
    memset( ( void* ) &data_flrp.data, 0x00, sizeof( data_flrp.data ) );
    data_flrp.counter = flrp_api_tx_counter;

    for( uint32_t i = 0; i < sizeof( data_flrp.data ); i++ )
    {
        data_flrp.data[i] = ( uint8_t ) i;
    }
}

/* --- EOF ------------------------------------------------------------------ */
