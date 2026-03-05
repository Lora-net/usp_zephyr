/**
 * @file      sx126x_hal_stm32wl.c
 *
 * @brief     SX126x HAL implementation for the STM32WL internal radio.
 *
 * On STM32WLE5/STM32WL55, the SX126x-compatible radio is embedded inside
 * the SoC. Reset and busy signals are not exposed as external GPIO pins but
 * are accessed through CPU-internal registers (RCC and PWR). The SPI bus is
 * the internal subghzspi peripheral.
 *
 * This file replaces sx126x_hal.c when CONFIG_SEMTECH_SX126X_STM32WL is set.
 *
 * Busy-wait strategy
 * ------------------
 * On an external SX126x the BUSY pin is high while the radio processes a
 * command and low when it is ready for the next one.  On STM32WL the
 * equivalent is PWR_SR2.RFBUSYS:
 *
 *   RFBUSYS = 1  NSS is asserted (SPI transaction in progress) OR the radio
 *                is completing its wakeup from deep-sleep.
 *   RFBUSYS = 0  NSS is deasserted and the radio has finished any wakeup.
 *
 * IMPORTANT: RFBUSYS can be 1 at power-on or after a peripheral reset because
 * a previous SPI transaction may have left NSS asserted.  Polling RFBUSYS
 * BEFORE starting a transaction therefore creates a deadlock – we wait for NSS
 * to deassert, but NSS only deasserts when the transaction we have not yet
 * started completes.  The correct sequence is:
 *
 *   1. Start the SPI transaction via spi_write_dt() / spi_transceive_dt().
 *   2. Wait for RFBUSYS = 0 (NSS deasserted, wakeup complete).
 *
 * Copyright (c) 2026 Semtech Corporation
 * SPDX-License-Identifier: Apache-2.0
 * Created by Charles-Henri Hallard (charles@evolyenergy.com)
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <zephyr/irq.h>

/* STM32WL low-level register helpers */
#include <stm32wlxx_ll_pwr.h>
#include <stm32wlxx_ll_rcc.h>

#include <sx126x_hal.h>
#include "sx126x_stm32wl_hal_context.h"
#include <zephyr/usp/lora_lbm_transceiver.h>

LOG_MODULE_DECLARE( lora_sx126x, CONFIG_LORA_BASICS_MODEM_DRIVERS_LOG_LEVEL );

/* SX126x command opcodes used for RF switch and sleep handling */
#define SX126X_SET_STANDBY_OPCODE 0x80U /* radio → STDBY_RC or STDBY_XOSC   */
#define SX126X_SET_RX_OPCODE      0x82U /* radio → RX mode                   */
#define SX126X_SET_TX_OPCODE      0x83U /* radio → TX mode                   */
#define SX126X_SET_SLEEP_OPCODE   0x84U /* radio → sleep mode                */

/* NOP / GetStatus opcode – any byte triggers wakeup from sleep */
#define SX126X_NOP_OPCODE 0x00U

/**
 * @brief Drive external RF switch GPIOs according to the radio command.
 *
 * Call this BEFORE sending the SPI command so the RF path is connected
 * by the time the radio starts transmitting or receiving.
 *
 * Only SetTx (0x83) and SetRx (0x82) change the switch position.
 * All other commands (SetStandby, SetSleep, configuration writes, …)
 * put both pins low (idle / safe state).
 */
static void sx126x_stm32wl_set_rf_switch( const struct sx126x_stm32wl_hal_context_cfg_t* config,
                                          uint8_t opcode )
{
    bool tx_on = ( opcode == SX126X_SET_TX_OPCODE );
    bool rx_on = ( opcode == SX126X_SET_RX_OPCODE );

    if( config->tx_rf_switch.port != NULL )
    {
        gpio_pin_set_dt( &config->tx_rf_switch, tx_on ? 1 : 0 );
    }
    if( config->rx_rf_switch.port != NULL )
    {
        gpio_pin_set_dt( &config->rx_rf_switch, rx_on ? 1 : 0 );
    }
}

/**
 * @brief Wait until RFBUSYS is cleared (NSS deasserted, radio ready).
 *
 * Must be called AFTER an SPI transaction, never before.  Calling it before
 * a transaction while RFBUSYS is already 1 (stale NSS from a previous cycle)
 * would block forever.
 */
static void sx126x_stm32wl_wait_on_busy( void )
{
    uint32_t end = k_uptime_get_32( ) + CONFIG_LORA_BASICS_MODEM_DRIVERS_HAL_WAIT_ON_BUSY_TIMEOUT_MSEC;

    while( k_uptime_get_32( ) <= end )
    {
        /* RFBUSYS = 0 means NSS is deasserted and wakeup (if any) is done */
        if( !LL_PWR_IsActiveFlag_RFBUSYS( ) )
        {
            return;
        }
        k_usleep( 100 );
    }

    LOG_ERR( "Timeout of %dms waiting for STM32WL RFBUSYS to clear",
             CONFIG_LORA_BASICS_MODEM_DRIVERS_HAL_WAIT_ON_BUSY_TIMEOUT_MSEC );
    k_oops( );
}

/* ---------------------------------------------------------------------------
 * Public HAL API
 * ---------------------------------------------------------------------------
 */

sx126x_hal_status_t sx126x_hal_write( const void* context, const uint8_t* command, const uint16_t command_length,
                                      const uint8_t* data, const uint16_t data_length )
{
    const struct device*                           dev      = ( const struct device* ) context;
    const struct sx126x_stm32wl_hal_context_cfg_t* config   = dev->config;
    struct sx126x_stm32wl_hal_context_data_t*      dev_data = dev->data;
    int                                            ret;

    const struct spi_buf tx_bufs[] = {
        { .buf = ( void* ) command, .len = command_length },
        { .buf = ( void* ) data,    .len = data_length    },
    };
    const struct spi_buf_set tx_buf_set = { tx_bufs, .count = ARRAY_SIZE( tx_bufs ) };

    /* Drive external RF switch before the SPI command reaches the radio.
     * SetTx → TX path; SetRx → RX path; anything else → idle (both low). */
    sx126x_stm32wl_set_rf_switch( config, command[0] );

    /* No pre-SPI busy check: RFBUSYS may be 1 at startup due to a stale NSS
     * from a previous power cycle.  The wait belongs after the transaction. */

    ret = spi_write_dt( &config->spi, &tx_buf_set );
    if( ret )
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    if( command[0] == SX126X_SET_SLEEP_OPCODE )
    {
        /* After SET_SLEEP the radio enters sleep mode.  RFBUSYS will stay high
         * during sleep, so we must not poll it.  Instead wait the mandatory
         * 500 µs calibration time and record the new state. */
        dev_data->radio_status = RADIO_SLEEP;
        k_usleep( 500 );
    }
    else
    {
        /* Wait for NSS to deassert and any wakeup sequence to complete.
         * If the radio was asleep, RFBUSYS stays 1 until wakeup is done. */
        sx126x_stm32wl_wait_on_busy( );
        dev_data->radio_status = RADIO_AWAKE;
    }

    /* Re-enable the SUBGHZ radio interrupt after every SPI write.
     *
     * The ISR (sx126x_stm32wl_radio_isr) calls irq_disable() to prevent an
     * IRQ storm caused by EXTI line 44 being level-triggered: DIO1 stays HIGH
     * until ClearIrqStatus is sent over SPI.  Once the SPI transaction
     * completes here (DIO1 is either already LOW or will deassert on the very
     * next ClearIrqStatus write), it is safe to re-arm the NVIC line so the
     * next radio event (RX_DONE, RX_TIMEOUT, TX_DONE, …) is not missed. */
    lora_transceiver_board_enable_interrupt( dev );

    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_read( const void* context, const uint8_t* command, const uint16_t command_length,
                                     uint8_t* data, const uint16_t data_length )
{
    const struct device*                           dev    = ( const struct device* ) context;
    const struct sx126x_stm32wl_hal_context_cfg_t* config = dev->config;
    struct sx126x_stm32wl_hal_context_data_t*      dev_data = dev->data;
    int                                            ret;

    const struct spi_buf tx_bufs[] = {
        { .buf = ( uint8_t* ) command, .len = command_length },
        { .buf = NULL,                 .len = data_length    },
    };
    const struct spi_buf rx_bufs[] = {
        { .buf = NULL, .len = command_length },
        { .buf = data, .len = data_length    },
    };
    const struct spi_buf_set tx_buf_set = { .buffers = tx_bufs, .count = ARRAY_SIZE( tx_bufs ) };
    const struct spi_buf_set rx_buf_set = { .buffers = rx_bufs, .count = ARRAY_SIZE( rx_bufs ) };

    /* No pre-SPI busy check for the same reason as sx126x_hal_write(). */

    ret = spi_transceive_dt( &config->spi, &tx_buf_set, &rx_buf_set );
    if( ret )
    {
        return SX126X_HAL_STATUS_ERROR;
    }

    /* Wait for NSS to deassert after the read transaction. */
    sx126x_stm32wl_wait_on_busy( );
    dev_data->radio_status = RADIO_AWAKE;

    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_reset( const void* context )
{
    const struct device*                           dev    = ( const struct device* ) context;
    const struct sx126x_stm32wl_hal_context_cfg_t* config = dev->config;
    struct sx126x_stm32wl_hal_context_data_t*      data   = dev->data;

    /* On STM32WL the radio reset is an internal RCC SubGHz sub-system reset.
     * There is no external NRESET pin. */
    LL_RCC_RF_EnableReset( );
    k_msleep( 20 );
    LL_RCC_RF_DisableReset( );
    k_msleep( 10 );

    /* After reset the radio is in standby — put RF switch in idle state. */
    sx126x_stm32wl_set_rf_switch( config, SX126X_SET_STANDBY_OPCODE );
    data->radio_status = RADIO_AWAKE;
    return SX126X_HAL_STATUS_OK;
}

sx126x_hal_status_t sx126x_hal_wakeup( const void* context )
{
    const struct device*                           dev      = ( const struct device* ) context;
    const struct sx126x_stm32wl_hal_context_cfg_t* config   = dev->config;
    struct sx126x_stm32wl_hal_context_data_t*      dev_data = dev->data;

    if( dev_data->radio_status == RADIO_SLEEP )
    {
        /* Any SPI transaction (NSS assertion) wakes the radio from sleep.
         * Send a NOP byte to trigger the wakeup, then wait for RFBUSYS = 0
         * which confirms the wakeup sequence has completed. */
        uint8_t                  nop        = SX126X_NOP_OPCODE;
        const struct spi_buf     tx_buf     = { .buf = &nop, .len = 1 };
        const struct spi_buf_set tx_buf_set = { .buffers = &tx_buf, .count = 1 };

        spi_write_dt( &config->spi, &tx_buf_set );
        sx126x_stm32wl_wait_on_busy( );
        dev_data->radio_status = RADIO_AWAKE;
    }

    return SX126X_HAL_STATUS_OK;
}
