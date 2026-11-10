/**
 * @file      hw_modem.c
 *
 * @brief     Single-UART hw_modem transport (no external bridge)
 *
 * Copied from USP examples/main_examples/hw_modem/hw_modem_serial.c with Zephyr
 * radio_utilities and unified_uart includes.
 * Host sends [CMD][LEN][PAYLOAD] without CRC. Responses include a 2-byte bridge
 * header for compatibility with rac_modem_api / device_manager.py.
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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/usp/lora_lbm_transceiver.h>
#include <zephyr/usp/smtc_hal_dbg_trace.h>

#include "hw_modem.h"
#include "smtc_modem_utilities.h"
#include "unified_uart.h"
#include "cmd_parser.h"

#define HW_MODEM_RX_BUFF_MAX_LENGTH 261
#define BRIDGE_HEADER_BYTE0 0x01
#define BRIDGE_HEADER_BYTE1 0x00
#define BRIDGE_HEADER_SIZE 2

static uint8_t       modem_response_buff[HW_MODEM_RX_BUFF_MAX_LENGTH];
static uint8_t       modem_received_buff[HW_MODEM_RX_BUFF_MAX_LENGTH];
static size_t        response_length;
static volatile bool hw_cmd_available;
static bool          stay_awake_once;

static void hw_modem_event_handler( void );
static void serial_poll_rx( void );

static void serial_poll_rx( void )
{
    uint16_t avail = unified_uart_rx_available( );
    uint8_t  len;

    if( avail < 2 )
    {
        return;
    }

    if( !unified_uart_rx_peek( 1, &len ) )
    {
        return;
    }

    if( avail < ( uint16_t ) ( 2 + len ) )
    {
        return;
    }

    for( uint16_t i = 0; i < 2 + len; i++ )
    {
        unified_uart_rx_get_byte( &modem_received_buff[i] );
    }

    uint8_t crc = 0;
    for( uint16_t i = 0; i < 2 + len; i++ )
    {
        crc ^= modem_received_buff[i];
    }
    modem_received_buff[2 + len] = crc;

    hw_cmd_available = true;
}

int hw_modem_init( void )
{
    memset( modem_response_buff, 0, HW_MODEM_RX_BUFF_MAX_LENGTH );
    memset( modem_received_buff, 0xFF, HW_MODEM_RX_BUFF_MAX_LENGTH );
    hw_cmd_available = false;
    stay_awake_once  = false;

    smtc_modem_init( &hw_modem_event_handler );

#if defined( PERF_TEST_ENABLED )
    SMTC_HAL_TRACE_WARNING( "HARDWARE MODEM RUNNING PERF TEST MODE" );
#endif
    return 0;
}

void hw_modem_process_cmd( void )
{
    uint8_t              cmd_length = 0xFF;
    cmd_response_t       output;
    cmd_input_t          input;
    cmd_serial_rc_code_t rc_code;
    uint8_t              crc = 0;

    if( modem_received_buff[0] < 0xFF )
    {
        cmd_length = modem_received_buff[1];

        for( int i = 0; i < cmd_length + 2; i++ )
        {
            crc = crc ^ modem_received_buff[i];
        }
        uint8_t calculated_crc = crc;
        uint8_t cmd_crc        = modem_received_buff[cmd_length + 2];
        uint8_t cmd_id         = modem_received_buff[0];

        if( calculated_crc != cmd_crc )
        {
            rc_code         = CMD_RC_FRAME_ERROR;
            response_length = 0;
            SMTC_HAL_TRACE_ERROR( "Cmd with bad crc %x / %x", calculated_crc, cmd_crc );
        }
        else if( ( modem_received_buff[cmd_length + 3] != 0xFF ) && ( cmd_length != 0xFF ) )
        {
            rc_code         = CMD_RC_FRAME_ERROR;
            response_length = 0;
            SMTC_HAL_TRACE_WARNING( " Extra data after the command" );
        }
        else
        {
            SMTC_HAL_TRACE_ARRAY( "Cmd input uart", modem_received_buff, cmd_length + 2 );
            input.cmd_code = cmd_id;
            input.length   = cmd_length;
            input.buffer   = &modem_received_buff[2];
            output.buffer  = &modem_response_buff[2];
            parse_cmd( &input, &output );
            rc_code         = output.return_code;
            response_length = output.length;
        }

        modem_response_buff[0] = rc_code;
        modem_response_buff[1] = response_length;

        SMTC_HAL_TRACE_ARRAY( "Cmd output on uart", modem_response_buff, response_length + 2 );
        hw_cmd_available = false;

        crc = 0;
        for( int i = 0; i < ( int ) ( response_length + 2 ); i++ )
        {
            crc = crc ^ modem_response_buff[i];
        }
        modem_response_buff[response_length + 2] = crc;

        {
            size_t  modem_len = response_length + 3;
            uint8_t bridge_response[BRIDGE_HEADER_SIZE + HW_MODEM_RX_BUFF_MAX_LENGTH];

            bridge_response[0] = BRIDGE_HEADER_BYTE0;
            bridge_response[1] = BRIDGE_HEADER_BYTE1;
            memcpy( &bridge_response[BRIDGE_HEADER_SIZE], modem_response_buff, modem_len );
            unified_uart_tx( bridge_response, BRIDGE_HEADER_SIZE + modem_len, UNIFIED_UART_MSG_DATA );
        }
    }
    else
    {
        hw_cmd_available = false;
    }

    stay_awake_once = true;
    memset( modem_received_buff, 0xFF, HW_MODEM_RX_BUFF_MAX_LENGTH );
}

bool hw_modem_is_a_cmd_available( void )
{
    if( !hw_cmd_available )
    {
        serial_poll_rx( );
    }
    return hw_cmd_available;
}

bool hw_modem_is_low_power_ok( void )
{
    if( hw_cmd_available || ( unified_uart_rx_available( ) > 0 ) )
    {
        return false;
    }

    if( stay_awake_once )
    {
        stay_awake_once = false;
        return false;
    }

    return true;
}

void hw_modem_unset_event_pin( void )
{
}

void hw_modem_disable_irq( void )
{
}

void hw_modem_set_tx_power_offset( const void* context, uint8_t tx_pwr_offset_db )
{
    radio_utilities_set_tx_power_offset( context, tx_pwr_offset_db );
}

uint8_t hw_modem_get_tx_power_offset( const void* context )
{
    return radio_utilities_get_tx_power_offset( context );
}

static void hw_modem_event_handler( void )
{
    SMTC_HAL_TRACE_INFO( "Event available" );
}
