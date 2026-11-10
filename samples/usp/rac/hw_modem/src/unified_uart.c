/**
 * @file      unified_uart.c
 *
 * @brief     Single-UART transport and log backend (device_manager compatible)
 *
 * Frame layout matches smtc_hal_uart.c / device_manager.py:
 *   [type:1 ('T' trace | 'D' data)][len:2 LE][payload]
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2025. All rights reserved.
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

#include <unified_uart.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log_backend.h>
#include <zephyr/logging/log_backend_std.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/logging/log_output.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/util.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include <smtc_core_hal.h>

/* Matches smtc_hal_uart.c. */
#define UNIFIED_UART_RX_BUFFER_SIZE 640
#define UNIFIED_UART_FRAME_HEADER_SIZE 3

/* Host UART: board console (chosen zephyr,console = USB/VCP link) by default;
 * a "smtc-hal-uart" alias in the overlay overrides it. */
#if DT_NODE_EXISTS( DT_ALIAS( smtc_hal_uart ) )
#define UNIFIED_UART_NODE DT_ALIAS( smtc_hal_uart )
#elif DT_HAS_CHOSEN( zephyr_console )
#define UNIFIED_UART_NODE DT_CHOSEN( zephyr_console )
#else
#error "hw_modem: no host UART found. Define the 'smtc-hal-uart' alias or a 'zephyr,console' chosen node in the board overlay."
#endif

static const struct device* const unified_uart_dev = DEVICE_DT_GET( UNIFIED_UART_NODE );

static uint8_t            unified_rx_buffer[UNIFIED_UART_RX_BUFFER_SIZE];
static volatile uint16_t  unified_rx_head;
static volatile uint16_t  unified_rx_tail;
static K_MUTEX_DEFINE( unified_tx_mutex );
static bool               unified_uart_ready;

static uint16_t unified_uart_rx_count( void )
{
    uint16_t head = unified_rx_head;
    uint16_t tail = unified_rx_tail;

    if( head >= tail )
    {
        return head - tail;
    }

    return UNIFIED_UART_RX_BUFFER_SIZE - tail + head;
}

static void unified_uart_rx_push( uint8_t byte )
{
    uint16_t next = ( unified_rx_head + 1U ) % UNIFIED_UART_RX_BUFFER_SIZE;

    if( next == unified_rx_tail )
    {
        unified_rx_tail = ( unified_rx_tail + 1U ) % UNIFIED_UART_RX_BUFFER_SIZE;
    }

    unified_rx_buffer[unified_rx_head] = byte;
    unified_rx_head                    = next;
}

static void unified_uart_irq_handler( const struct device* dev, void* user_data )
{
    ARG_UNUSED( user_data );
    uint8_t byte;
    bool    had_data = ( unified_uart_rx_count( ) > 0 );

    if( !uart_irq_update( dev ) )
    {
        return;
    }

    while( uart_irq_rx_ready( dev ) && ( uart_fifo_read( dev, &byte, 1 ) == 1 ) )
    {
        unified_uart_rx_push( byte );
    }

    if( !had_data && ( unified_uart_rx_count( ) > 0 ) )
    {
        smtc_modem_hal_wake_up( );
    }
}

static void unified_uart_write_header( unified_uart_message_type_t type, uint16_t len )
{
    uint8_t header[UNIFIED_UART_FRAME_HEADER_SIZE];

    header[0] = ( type == UNIFIED_UART_MSG_TRACE ) ? ( uint8_t ) 'T' : ( uint8_t ) 'D';
    header[1] = ( uint8_t ) ( len & 0xFF );
    header[2] = ( uint8_t ) ( ( len >> 8 ) & 0xFF );

    for( size_t i = 0; i < ARRAY_SIZE( header ); i++ )
    {
        uart_poll_out( unified_uart_dev, header[i] );
    }
}

int unified_uart_init( void )
{
    if( unified_uart_ready )
    {
        return 0;
    }

    if( !device_is_ready( unified_uart_dev ) )
    {
        return -ENODEV;
    }

    unified_rx_head = 0;
    unified_rx_tail = 0;

    uart_irq_callback_user_data_set( unified_uart_dev, unified_uart_irq_handler, NULL );
    uart_irq_rx_enable( unified_uart_dev );

    unified_uart_ready = true;
    return 0;
}

void unified_uart_tx( uint8_t* buff, uint16_t len, unified_uart_message_type_t type )
{
    if( !unified_uart_ready )
    {
        unified_uart_init( );
    }

    if( !unified_uart_ready )
    {
        return;
    }

    k_mutex_lock( &unified_tx_mutex, K_FOREVER );
    unified_uart_write_header( type, len );

    for( uint16_t i = 0; i < len; i++ )
    {
        uart_poll_out( unified_uart_dev, buff[i] );
    }

    k_mutex_unlock( &unified_tx_mutex );
}

uint16_t unified_uart_rx_available( void )
{
    return unified_uart_rx_count( );
}

bool unified_uart_rx_get_byte( uint8_t* byte )
{
    if( unified_uart_rx_count( ) == 0 )
    {
        return false;
    }

    *byte           = unified_rx_buffer[unified_rx_tail];
    unified_rx_tail = ( unified_rx_tail + 1U ) % UNIFIED_UART_RX_BUFFER_SIZE;
    return true;
}

bool unified_uart_rx_peek( uint16_t offset, uint8_t* byte )
{
    if( offset >= unified_uart_rx_count( ) )
    {
        return false;
    }

    uint16_t index = ( unified_rx_tail + offset ) % UNIFIED_UART_RX_BUFFER_SIZE;

    *byte = unified_rx_buffer[index];
    return true;
}

void unified_uart_rx_flush( void )
{
    unified_rx_tail = unified_rx_head;
}

#ifdef CONFIG_PM_POLICY_CUSTOM
const struct pm_state_info* pm_policy_next_state( uint8_t cpu, int32_t ticks )
{
    ARG_UNUSED( cpu );
    ARG_UNUSED( ticks );

    return NULL;
}
#endif

#define LB_UU_BUFFER_SIZE 512
#define LB_UU_TX_ACCUM_SIZE LB_UU_BUFFER_SIZE

static uint8_t  lb_uu_buffer[LB_UU_BUFFER_SIZE];
static uint8_t  lb_uu_tx_accum[LB_UU_TX_ACCUM_SIZE];
static size_t   lb_uu_tx_accum_len;
static uint32_t lb_uu_log_format = LOG_OUTPUT_TEXT;

static void lb_uu_init( const struct log_backend* const backend )
{
    ARG_UNUSED( backend );
    unified_uart_init( );
}

static void lb_uu_tx_flush( void )
{
    if( lb_uu_tx_accum_len == 0 )
    {
        return;
    }

    unified_uart_tx( lb_uu_tx_accum, ( uint16_t ) lb_uu_tx_accum_len, UNIFIED_UART_MSG_TRACE );
    lb_uu_tx_accum_len = 0;
}

static int lb_uu_char_out( uint8_t* data, size_t length, void* ctx )
{
    ARG_UNUSED( ctx );

    if( length == 0 )
    {
        return 0;
    }

    /* LOG_MODE_IMMEDIATE invokes this once per character. Batch into a single
     * unified_uart frame per line, matching bare-metal hal_trace_print(). */
    for( size_t i = 0; i < length; i++ )
    {
        if( lb_uu_tx_accum_len >= LB_UU_TX_ACCUM_SIZE )
        {
            lb_uu_tx_flush( );
            lb_uu_tx_accum_len = 0; // lb_uu_tx_flush does it, but coverity can't see it
        }

        lb_uu_tx_accum[lb_uu_tx_accum_len++] = data[i];

        if( data[i] == '\n' )
        {
            lb_uu_tx_flush( );
        }
    }

    return ( int ) length;
}

LOG_OUTPUT_DEFINE( lb_uu_output, lb_uu_char_out, lb_uu_buffer, LB_UU_BUFFER_SIZE );

static void lb_uu_panic( const struct log_backend* const backend )
{
    ARG_UNUSED( backend );
    lb_uu_tx_flush( );
    log_backend_std_panic( &lb_uu_output );
}

static void lb_uu_dropped( const struct log_backend* const backend, uint32_t cnt )
{
    ARG_UNUSED( backend );
    log_backend_std_dropped( &lb_uu_output, cnt );
}

static void lb_uu_process( const struct log_backend* const backend, union log_msg_generic* msg )
{
    ARG_UNUSED( backend );

    log_format_func_t log_output_func = log_format_func_t_get( lb_uu_log_format );

    log_output_func( &lb_uu_output, &msg->log, log_backend_std_get_flags( ) );
    lb_uu_tx_flush( );
}

static int lb_uu_format_set( const struct log_backend* const backend, uint32_t log_type )
{
    ARG_UNUSED( backend );
    lb_uu_log_format = log_type;
    return 0;
}

static const struct log_backend_api log_backend_unified_uart_api = {
    .process    = lb_uu_process,
    .panic      = lb_uu_panic,
    .init       = lb_uu_init,
    .dropped    = IS_ENABLED( CONFIG_LOG_MODE_IMMEDIATE ) ? NULL : lb_uu_dropped,
    .format_set = lb_uu_format_set,
};

LOG_BACKEND_DEFINE( log_backend_unified_uart, log_backend_unified_uart_api, true );
