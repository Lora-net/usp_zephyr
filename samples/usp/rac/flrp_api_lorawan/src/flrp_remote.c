/*!
 * \file      flrp_remote.c
 *
 * \brief     Test functions for FLRP remote over lorawan functions
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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include <smtc_modem_helper.h>
#include <smtc_modem_hal.h>

#include "flrp_remote.h"
#include "flrp_api_menu.h"
#include "flrp_display.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

LOG_MODULE_REGISTER( flrp_remote, LOG_LEVEL_INF );

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

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

#if defined( CONFIG_USP_LORA_BASICS_MODEM )
static void flrp_remote_handle_downlink( uint8_t* payload, uint8_t payload_size );
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void flrp_remote_send_keep_alive( void )
{
#if defined( CONFIG_USP_LORA_BASICS_MODEM )
    smtc_modem_status_mask_t status_mask = 0;
    smtc_modem_return_code_t rc;

    LOG_DBG( "Keep-alive event" );
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
#endif
}

#if defined( CONFIG_USP_LORA_BASICS_MODEM )
void smtc_modem_helper_event( smtc_modem_event_type_t event_type )
{
    uint8_t*                  rx_payload;
    uint8_t*                  rx_payload_size;
    smtc_modem_dl_metadata_t* rx_metadata;
    uint8_t*                  rx_remaining;

    switch( event_type )
    {
    case SMTC_MODEM_EVENT_JOINED:
        ASSERT_SMTC_MODEM_RC( smtc_modem_trig_lorawan_mac_request( STACK_ID, SMTC_MODEM_LORAWAN_MAC_REQ_DEVICE_TIME ) );
        /* start periodical uplink alarm */
        ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( DELAY_FIRST_MSG_AFTER_JOIN ) );
        flrp_display_icon( 0, 2, ICON_RADIO_JOINED );
        break;
    case SMTC_MODEM_EVENT_JOINFAIL:
        flrp_display_icon( 0, 2, ICON_RADIO_NOT_JOINED );
        break;
    case SMTC_MODEM_EVENT_ALARM:
        /* Send keep-alive */
        smtc_modem_request_empty_uplink( STACK_ID, true, KEEP_ALIVE_PORT, false );
        /* Restart periodical uplink alarm */
        ASSERT_SMTC_MODEM_RC( smtc_modem_alarm_start_timer( PERIODICAL_UPLINK_DELAY_S ) );
        break;
    case SMTC_MODEM_EVENT_DOWNDATA:
        /* Get downlink data */
        LOG_INF( "Event received: DOWNDATA" );
        rx_payload      = smtc_modem_helper_get_rx_payload( );
        rx_payload_size = smtc_modem_helper_get_rx_payload_size( );
        rx_metadata     = smtc_modem_helper_get_rx_metadata( );
        rx_remaining    = smtc_modem_helper_get_rx_remaining( );
        if( rx_metadata->fport == FLRP_REMOTE_PORT )
        {
            flrp_remote_handle_downlink( rx_payload, *rx_payload_size );
        }
        LOG_DBG( "Data received on port %u", rx_metadata->fport );
        LOG_HEXDUMP_DBG( rx_payload, *rx_payload_size, "Received payload" );
        break;
    default:
        break;
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static void flrp_remote_handle_downlink( uint8_t* payload, uint8_t payload_size )
{
    flrp_remote_init_cmd_t*     flrp_remote_init_cmd;
    flrp_remote_transmit_cmd_t* flrp_remote_transmit_cmd;

    LOG_INF( "Received downlink of size %u", payload_size );
    if( payload_size > 0 )
    {
        switch( ( flrp_remote_cmd_id_t ) payload[0] )
        {
        case FLRP_REMOTE_CMD_ID_RESET:
            if( sizeof( flrp_remote_cmd_id_t ) == payload_size )
            {
                LOG_INF( "Received reset command" );
                smtc_modem_hal_reset_mcu( );
            }
            break;
        case FLRP_REMOTE_CMD_ID_START_LISTENING:
            if( sizeof( flrp_remote_cmd_id_t ) == payload_size )
            {
                LOG_INF( "Received start listening command" );
                flrp_api_menu_start_listening( );
            }
            break;
        case FLRP_REMOTE_CMD_ID_STOP_LISTENING:
            if( sizeof( flrp_remote_cmd_id_t ) == payload_size )
            {
                LOG_INF( "Received stop listening command" );
                flrp_api_menu_stop_listening( );
            }
            break;
        case FLRP_REMOTE_CMD_ID_INIT:
            if( sizeof( flrp_remote_init_cmd_t ) == payload_size )
            {
                flrp_remote_init_cmd = ( flrp_remote_init_cmd_t* ) payload;
                flrp_api_menu_init( flrp_remote_init_cmd->device_eui, flrp_remote_init_cmd->key,
                                    flrp_remote_init_cmd->init_options.crypto_enabled,
                                    flrp_remote_init_cmd->init_options.is_low_frequency,
                                    flrp_remote_init_cmd->init_options.listening );
            }
            break;
        case FLRP_REMOTE_CMD_ID_INIT_TRANSFER:
            if( sizeof( flrp_remote_transmit_cmd_t ) == payload_size )
            {
                flrp_remote_transmit_cmd = ( flrp_remote_transmit_cmd_t* ) payload;
                flrp_api_menu_initiate_transfer( flrp_remote_transmit_cmd->transmit,
                                                 flrp_remote_transmit_cmd->target_device_eui );
            }
            break;
        default:
            LOG_WRN( "Received unknown command ID 0x%02X", payload[0] );
            break;
        }
    }
}

#endif  // CONFIG_USP_LORA_BASICS_MODEM

/* --- EOF ------------------------------------------------------------------ */