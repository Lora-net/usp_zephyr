/**
 * @file      hw_modem.c
 *
 * @brief     hw_modem implementation
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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include "smtc_modem_utilities.h"

#include "hw_modem.h"
#include "cmd_parser.h"

LOG_MODULE_DECLARE( hw_modem, 3 );

#define HW_MODEM_RX_BUFF_MAX_LENGTH 261

typedef enum
{
    HW_MODEM_LP_ENABLE,
    HW_MODEM_LP_DISABLE_ONCE,
    HW_MODEM_LP_DISABLE,
} hw_modem_lp_mode_t;

static uint8_t            modem_received_buff[HW_MODEM_RX_BUFF_MAX_LENGTH];
static size_t             received_length;
static uint8_t            modem_response_buff[HW_MODEM_RX_BUFF_MAX_LENGTH];
static size_t             response_length;
static volatile bool      hw_cmd_available;
static volatile bool      is_hw_modem_ready_to_receive = true;
static hw_modem_lp_mode_t lp_mode                      = HW_MODEM_LP_ENABLE;

static const struct gpio_dt_spec hw_modem_event_gpios =
    GPIO_DT_SPEC_GET( DT_PATH( zephyr_user ), hw_modem_event_gpios );
static const struct gpio_dt_spec hw_modem_busy_gpios = GPIO_DT_SPEC_GET( DT_PATH( zephyr_user ), hw_modem_busy_gpios );
static const struct gpio_dt_spec hw_modem_command_gpios =
    GPIO_DT_SPEC_GET( DT_PATH( zephyr_user ), hw_modem_command_gpios );

#if defined( CONFIG_LORA_BASICS_MODEM_GEOLOCATION )

#if DT_NODE_EXISTS( DT_NODELABEL( lora_scanning_led ) )
static const struct gpio_dt_spec hw_modem_led_scan_gpios = GPIO_DT_SPEC_GET(
    DT_PATH( zephyr_user ), hw_modem_led_scan_gpios );
#endif

#if DT_NODE_EXISTS( DT_NODELABEL( lora_gnss_lna_control ) )
static const struct gpio_dt_spec lora_gnss_lna_control =
    GPIO_DT_SPEC_GET( DT_NODELABEL( lora_gnss_lna_control ), gpios );
#endif

#endif  // CONFIG_LORA_BASICS_MODEM_GEOLOCATION

static const struct device* hw_modem_uart = DEVICE_DT_GET( DT_ALIAS( smtc_hal_uart ) );

/**
 * @brief prepare and start the reception of the command on a uart using a dma
 * @param [none]
 * @return [none]
 */
void hw_modem_start_reception( void );

/**
 * @brief function that will be called every time the COMMAND line in asserted or de-asserted by the
 * host
 * @param *context  unused context
 * @return none
 */
void wakeup_line_irq_handler( const struct device* port, struct gpio_callback* cb, gpio_port_pins_t pins );

/**
 * @brief function that will be called by the soft modem engine each time an async event is
 * available
 * @param *context  unused context
 * @return none
 */
void                 hw_modem_event_handler( void );
struct gpio_callback command_callback;

int hw_modem_init( void )
{
    int ret;

    /* init hw modem pins */
    ret = gpio_pin_configure_dt( &hw_modem_event_gpios, GPIO_OUTPUT_INACTIVE );
    if( ret )
    {
        printk( "Error %d: failed to configure %s pin %d\n", ret, hw_modem_event_gpios.port->name,
                hw_modem_event_gpios.pin );
        return ret;
    }
    ret = gpio_pin_configure_dt( &hw_modem_busy_gpios, GPIO_OUTPUT_LOW );
    if( ret )
    {
        printk( "Error %d: failed to configure %s pin %d\n", ret, hw_modem_busy_gpios.port->name,
                hw_modem_busy_gpios.pin );
        return ret;
    }
    gpio_pin_set_dt( &hw_modem_busy_gpios, 1 );

#if defined( CONFIG_LORA_BASICS_MODEM_GEOLOCATION )
/* init LED and LNA enable in case of LR1110/LR1120 w/ geoloc */
#if DT_NODE_EXISTS( DT_NODELABEL( lora_scanning_led ) )
    ret = gpio_pin_configure_dt( &hw_modem_led_scan_gpios, GPIO_OUTPUT_LOW );
    if( ret )
    {
        printk( "Error %d: failed to configure %s pin %d\n", ret, hw_modem_led_scan_gpios.port->name,
                hw_modem_led_scan_gpios.pin );
        return ret;
    }
#endif

#if DT_NODE_EXISTS( DT_NODELABEL( lora_gnss_lna_control ) )
    ret = gpio_pin_configure_dt( &lora_gnss_lna_control, GPIO_OUTPUT_LOW );
    if( ret )
    {
        printk( "Error %d: failed to configure %s pin %d\n", ret, lora_gnss_lna_control.port->name,
                lora_gnss_lna_control.pin );
        return ret;
    }
#endif

#endif  // CONFIG_LORA_BASICS_MODEM_GEOLOCATION

    /* init irq on COMMAND pin */
    gpio_pin_configure_dt( &hw_modem_command_gpios, GPIO_INPUT | GPIO_PULL_UP );
    ret = gpio_pin_interrupt_configure_dt( &hw_modem_command_gpios, GPIO_INT_ENABLE | GPIO_INT_EDGE_BOTH );
    if( ret )
    {
        printk( "Error %d: failed to configure interrupt on %s pin %d\n", ret, hw_modem_command_gpios.port->name,
                hw_modem_command_gpios.pin );
        return ret;
    }

    gpio_init_callback( &command_callback, wakeup_line_irq_handler, BIT( hw_modem_command_gpios.pin ) );
    ret = gpio_add_callback_dt( &hw_modem_command_gpios, &command_callback );
    if( ret )
    {
        printk( "Error %d: failed to add interrupt on %s pin %d\n", ret, hw_modem_command_gpios.port->name,
                hw_modem_command_gpios.pin );
        return ret;
    }

    if( !device_is_ready( hw_modem_uart ) )
    {
        printk( "Hardware modem UART is not ready!" );
        return 1;
    }

    // TESTING
    // Disable uart RX at startup (doesn't seem to really work)
    // uart_irq_rx_disable( hw_modem_uart );

    memset( modem_response_buff, 0, HW_MODEM_RX_BUFF_MAX_LENGTH );
    hw_cmd_available             = false;
    is_hw_modem_ready_to_receive = true;

    /* init the soft modem */
    smtc_modem_init( &hw_modem_event_handler );

#if defined( PERF_TEST_ENABLED )
    LOG_WRN( "HARDWARE MODEM RUNNING PERF TEST MODE" );
#endif
    return 0;
}

static void uart_irq_rx_callback_handler( const struct device* dev, void* user_data )
{
    ARG_UNUSED( user_data );
    uint8_t c;

    if( !uart_irq_update( dev ) )
    {
        return;
    }

    if( !uart_irq_rx_ready( dev ) )
    {
        return;
    }

    int status = uart_err_check( dev );
    if( status > 0 )
    {
        LOG_ERR( "UART error detected: %d", status );
    }

    /* read until FIFO empty */
    while( uart_fifo_read( dev, &c, 1 ) == 1 )
    {
        if( received_length == HW_MODEM_RX_BUFF_MAX_LENGTH )
        {
            LOG_ERR( "Received more data than the buffer can hold!" );
        }
        else
        {
            modem_received_buff[received_length++] = c;
        }
    }
}

void hw_modem_start_reception( void )
{
    memset( modem_received_buff, 0xFF, HW_MODEM_RX_BUFF_MAX_LENGTH );
    received_length = 0;

    /* during the receive process the hw modem cannot accept an other cmd, prevent it */
    is_hw_modem_ready_to_receive = false;

    uart_irq_callback_user_data_set( hw_modem_uart, uart_irq_rx_callback_handler, NULL );
    int ret = uart_irq_rx_ready( hw_modem_uart );
    if( ret >= 1 )
    {  // If there was a previously-generated event (should not happen) (EVENTS_RXDRDY)
        // Read FIFO
        if( !uart_irq_update( hw_modem_uart ) )
        {
            return;
        }

        // Empty UART RX FIFO if there were trailing chars
        uint8_t c;
        while( uart_fifo_read( hw_modem_uart, &c, 1 ) == 1 )
            ;
    }
    uart_irq_rx_enable( hw_modem_uart );

    /* indicate to bridge or host that the modem is ready to receive on uart */
    gpio_pin_set_dt( &hw_modem_busy_gpios, 0 );
}

void hw_modem_process_cmd( void )
{
    uint8_t              cmd_length = 0xFF;
    cmd_response_t       output;
    cmd_input_t          input;
    cmd_serial_rc_code_t rc_code;

    /* check if not false detection (0xFF is default filled buff value) */
    if( modem_received_buff[0] < 0xFF )
    {
        cmd_length  = modem_received_buff[1];
        uint8_t crc = 0;

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
            LOG_ERR( "Cmd with bad crc %x / %x", calculated_crc, cmd_crc );
        }
        else if( ( modem_received_buff[cmd_length + 3] != 0xFF ) && ( cmd_length != 0xFF ) )
        {
            /* Too Many cmd enqueued */
            rc_code         = CMD_RC_FRAME_ERROR;
            response_length = 0;
            LOG_WRN( " Extra data after the command" );
        }
        else
        {
            /* go into soft modem */
            LOG_HEXDUMP_INF( modem_received_buff, cmd_length + 2, "Cmd input uart" );
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

        LOG_HEXDUMP_INF( modem_response_buff, response_length + 2, "Cmd output on uart" );

        /* now the hw modem can accept new commands */
        is_hw_modem_ready_to_receive = true;
        hw_cmd_available             = false;

        /* set busy pin to indicate to bridge or host that the hw_modem answer
         * will be soon sent
         */
        gpio_pin_set_dt( &hw_modem_busy_gpios, 1 );

        /* wait to bridge delay */
        k_usleep( 1000 );

        for( int i = 0; i < response_length + 2; i++ )
        {
            crc = crc ^ modem_response_buff[i];
        }
        modem_response_buff[response_length + 2] = crc;

        /* Blocking send */
        for( size_t i = 0; i < response_length + 3; i++ )
        {
            uart_poll_out( hw_modem_uart, modem_response_buff[i] );
        }
    }
    else
    {
        /* now the hw modem can accept new commands */
        is_hw_modem_ready_to_receive = true;
        hw_cmd_available             = false;

        /* set busy pin to indicate to bridge or host that the hw_modem answer
         * will be soon sent
         */
        gpio_pin_set_dt( &hw_modem_busy_gpios, 1 );
    }
}

bool hw_modem_is_a_cmd_available( void )
{
    return hw_cmd_available;
}

bool hw_modem_is_low_power_ok( void )
{
    if( lp_mode == HW_MODEM_LP_ENABLE )
    {
        return true;
    }
    else if( lp_mode == HW_MODEM_LP_DISABLE_ONCE )
    {
        /* next time lp  will be ok */
        lp_mode = HW_MODEM_LP_ENABLE;
        return false;
    }
    else
    {
        return false;
    }
}

// COMMAND pin
void wakeup_line_irq_handler( const struct device* port, struct gpio_callback* cb, gpio_port_pins_t pins )
{
    if( is_hw_modem_ready_to_receive && gpio_pin_get_dt( &hw_modem_command_gpios ) == 0 )
    {
        /* start receiving uart with dma */
        hw_modem_start_reception( );

        /* force exit of stop mode */
        lp_mode = HW_MODEM_LP_DISABLE;
    }

    if( !is_hw_modem_ready_to_receive && gpio_pin_get_dt( &hw_modem_command_gpios ) == 1 )
    {
        /* stop uart on dma reception */
        uart_irq_rx_disable( hw_modem_uart );

        /* inform that a command has arrived */
        hw_cmd_available = true;

        /* wake up thread to process the command */
        smtc_modem_hal_wake_up( );

        /* force one more loop in main loop and then re-enable low power feature */
        lp_mode = HW_MODEM_LP_DISABLE_ONCE;
    }
}

void hw_modem_event_handler( void )
{
    /* raise the event line to indicate to host that events are available */
    gpio_pin_set_dt( &hw_modem_event_gpios, 1 );
    smtc_modem_hal_wake_up( );
    LOG_INF( "Event available" );
}
