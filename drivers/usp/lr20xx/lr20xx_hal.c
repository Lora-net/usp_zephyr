/**
 * @file      lr20xx_hal.c
 *
 * @brief     lr20xx_hal HAL implementation
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

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER( lr20xx_hal, CONFIG_LORA_BASICS_MODEM_DRIVERS_LOG_LEVEL );

#include "lr20xx_hal.h"
#include "lr20xx_hal_context.h"

#define LR20XX_HAL_WAIT_ON_BUSY_TIMEOUT_SEC CONFIG_LR20XX_HAL_WAIT_ON_BUSY_TIMEOUT_SEC
#define LR20XX_HAL_SPI_BUFFER_MAX_LENGTH CONFIG_LR20XX_HAL_SPI_BUFFER_MAX_LENGTH

/**
 * @brief Wait until radio busy pin returns to inactive state or
 * until LR20XX_HAL_WAIT_ON_BUSY_TIMEOUT_SEC passes.
 *
 * @retval LR20XX_HAL_STATUS_OK
 */
static lr20xx_hal_status_t lr20xx_hal_wait_on_busy( const void* context )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;
    uint32_t end       = k_uptime_get_32( ) + CONFIG_LORA_BASICS_MODEM_DRIVERS_HAL_WAIT_ON_BUSY_TIMEOUT_MSEC;
    bool     timed_out = false;

    while( k_uptime_get_32( ) <= end )
    {
        timed_out = ( gpio_pin_get_dt( &config->busy ) == 0 ) ? true : false;
        if( timed_out == true )
        {
            break;
        }
        else
        {
            k_usleep( 100 );
        }
    }

    if( !timed_out )
    {
        LOG_ERR( "Timeout of %dms hit when waiting for lr20xx busy!",
                 CONFIG_LORA_BASICS_MODEM_DRIVERS_HAL_WAIT_ON_BUSY_TIMEOUT_MSEC );
        k_oops( );
    }
    return LR20XX_HAL_STATUS_OK;
}

/**
 * @brief Check if device is ready to receive spi transaction.
 *
 * If the device is in sleep mode, it will awake it and then wait until it is ready
 *
 */
static void lr20xx_hal_check_device_ready( const void* context )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;
    struct lr20xx_hal_context_data_t*      data   = dev->data;

    if( data->radio_status != RADIO_SLEEP )
    {
        lr20xx_hal_wait_on_busy( context );
    }
    else
    {
        // Busy is HIGH in sleep mode, wake-up the device with a small glitch on NSS
        const struct gpio_dt_spec* cs = &( config->spi.config.cs.gpio );

        gpio_pin_set_dt( cs, 1 );
        gpio_pin_set_dt( cs, 0 );
        lr20xx_hal_wait_on_busy( context );
        data->radio_status = RADIO_AWAKE;
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

lr20xx_hal_status_t lr20xx_hal_reset( const void* context )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;
    struct lr20xx_hal_context_data_t*      data   = dev->data;

    gpio_pin_set_dt( &config->reset, 1 );
    k_sleep( K_USEC( 100 ) );
    gpio_pin_set_dt( &config->reset, 0 );
    k_sleep( K_USEC( 3500 ) );

    data->radio_status = RADIO_AWAKE;

    return LR20XX_HAL_STATUS_OK;
}

lr20xx_hal_status_t lr20xx_hal_wakeup( const void* context )
{
    lr20xx_hal_check_device_ready( context );

    return LR20XX_HAL_STATUS_OK;
}

/*
void wait_spi_bytes(const struct spi_dt_spec *spi, uint32_t bytes, bool set)
{
    const struct spi_cs_control *cs = &spi->config.cs;
    uint32_t delay_us = (
        bytes * 8 * 1000 * 1000 / spi->config.frequency
        + cs->delay
    );

    k_busy_wait(delay_us);
    if (set) {
        gpio_pin_set_dt(&cs->gpio, 0);
    }
}
*/

// Outside the function to prevent stack errors
static uint8_t tx_buffer[LR20XX_HAL_SPI_BUFFER_MAX_LENGTH];
static uint8_t rx_buffer[LR20XX_HAL_SPI_BUFFER_MAX_LENGTH];

lr20xx_hal_status_t lr20xx_hal_write( const void* context, const uint8_t* command, const uint16_t command_length,
                                      const uint8_t* data, const uint16_t data_length )
{
    const struct device*                   dev      = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config   = dev->config;
    struct lr20xx_hal_context_data_t*      dev_data = dev->data;
    int                                    ret;

    if( command_length + data_length > LR20XX_HAL_SPI_BUFFER_MAX_LENGTH )
    {
        // Early fail if length of data to exchange overflow allocated buffers
        return LR20XX_HAL_STATUS_ERROR;
    }

    //  Make a single SPI transaction packet
    memcpy( tx_buffer, command, command_length );
    memcpy( tx_buffer + command_length, data, data_length );

    lr20xx_hal_check_device_ready( context );
    const struct spi_buf tx_buf[] = { { .buf = ( uint8_t* ) tx_buffer, .len = command_length + data_length } };

    const struct spi_buf_set tx = { .buffers = tx_buf, .count = ARRAY_SIZE( tx_buf ) };

    ret = spi_write_dt( &config->spi, &tx );

    // ret = spi_write_signal(config->spi.bus, &config->spi.config, &tx, NULL);
    // wait_spi_bytes(&config->spi, command_length + data_length, true);

    // LOG_INF("%s finished writing %dbytes", __func__, command_length);
    if( ret )
    {
        return LR20XX_HAL_STATUS_ERROR;
    }

    // LR20XX_SYSTEM_SET_SLEEP_OC=0x011B opcode. In sleep mode the radio busy line is held at 1
    // => do not test it
    if( ( command[0] == 0x01 ) && ( command_length > 1 ) && ( command[1] == 0x27 ) )
    {
        dev_data->radio_status = RADIO_SLEEP;

        // add a incompressible delay to prevent trying to wake the radio
        // before it is full asleep
        k_sleep( K_USEC( 500 ) );
    }

    return LR20XX_HAL_STATUS_OK;
}

lr20xx_hal_status_t lr20xx_hal_read( const void* context, const uint8_t* command, const uint16_t command_length,
                                     uint8_t* data, const uint16_t data_length )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;
    int                                    ret;

    if( ( 2 + data_length ) > LR20XX_HAL_SPI_BUFFER_MAX_LENGTH )
    {
        // Early fail if length of data to exchange overflow allocated buffers
        return LR20XX_HAL_STATUS_ERROR;
    }

    lr20xx_hal_check_device_ready( context );

    const struct spi_buf tx_buf[] = { {
        .buf = ( uint8_t* ) command,
        .len = command_length,
    } };

    const struct spi_buf_set tx = { .buffers = tx_buf, .count = ARRAY_SIZE( tx_buf ) };

    ret = spi_write_dt( &config->spi, &tx );
    if( ret )
    {
        return LR20XX_HAL_STATUS_ERROR;
    }
    // wait_spi_bytes(&config->spi, command_length, true);

    if( data_length > 0 )
    {
        lr20xx_hal_check_device_ready( context );

        const struct spi_buf rx_buf[] = { // save dummy for crc calculation
                                          { .buf = rx_buffer, .len = 2 + data_length }
        };

        const struct spi_buf_set rx = { .buffers = rx_buf, .count = ARRAY_SIZE( rx_buf ) };

        ret = spi_read_dt( &config->spi, &rx );
        if( ret )
        {
            return LR20XX_HAL_STATUS_ERROR;
        }
        // wait_spi_bytes(&config->spi, data_length, true);
        memcpy( data, rx_buffer + 2, data_length );
    }

    return LR20XX_HAL_STATUS_OK;
}

lr20xx_hal_status_t lr20xx_hal_direct_read( const void* context, uint8_t* data, const uint16_t data_length )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;
    int                                    ret;

    lr20xx_hal_check_device_ready( context );

    const struct spi_buf rx_buf[] = { { .buf = data, .len = data_length } };

    const struct spi_buf_set rx = { .buffers = rx_buf, .count = ARRAY_SIZE( rx_buf ) };

    ret = spi_read_dt( &config->spi, &rx );
    if( ret )
    {
        return LR20XX_HAL_STATUS_ERROR;
    }
    // wait_spi_bytes(&config->spi, data_length, true);

    return LR20XX_HAL_STATUS_OK;
}

lr20xx_hal_status_t lr20xx_hal_direct_read_fifo( const void* context, const uint8_t* command,
                                                 const uint16_t command_length, uint8_t* data,
                                                 const uint16_t data_length )
{
    const struct device*                   dev    = ( const struct device* ) context;
    const struct lr20xx_hal_context_cfg_t* config = dev->config;
    int                                    ret;

    if( command_length + data_length > LR20XX_HAL_SPI_BUFFER_MAX_LENGTH )
    {
        // Early fail if length of data to exchange overflow allocated buffers
        return LR20XX_HAL_STATUS_ERROR;
    }

    memcpy( tx_buffer, command, command_length );
    memset( tx_buffer + command_length, 0, data_length );

    lr20xx_hal_check_device_ready( context );

    const struct spi_buf tx_bufs[] = { { .buf = ( uint8_t* ) tx_buffer, .len = command_length + data_length } };

    const struct spi_buf rx_bufs[] = { { .buf = rx_buffer, .len = command_length + data_length } };

    const struct spi_buf_set tx_buf_set = { .buffers = tx_bufs, .count = ARRAY_SIZE( tx_bufs ) };
    const struct spi_buf_set rx_buf_set = { .buffers = rx_bufs, .count = ARRAY_SIZE( rx_bufs ) };

    ret = spi_transceive_dt( &config->spi, &tx_buf_set, &rx_buf_set );
    if( ret )
    {
        return LR20XX_HAL_STATUS_ERROR;
    }
    // wait_spi_bytes(&config->spi, command_length + data_length, true);
    memcpy( data, rx_buffer + command_length, data_length );
    return LR20XX_HAL_STATUS_OK;
}
