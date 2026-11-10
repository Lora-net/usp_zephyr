/**
 * @file      unified_uart.h
 *
 * @brief     Single-UART trace/data framing (host device_manager compatible)
 *
 * Frame layout matches smtc_hal_uart.c / device_manager.py:
 *   [type:1 ('T' trace | 'D' data)][len:2 LE][payload]
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2025. All rights reserved.
 */

#ifndef UNIFIED_UART_H__
#define UNIFIED_UART_H__

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum unified_uart_message_type_e
{
    UNIFIED_UART_MSG_TRACE,
    UNIFIED_UART_MSG_DATA,
} unified_uart_message_type_t;

/** @brief Initialize RX ring buffer and UART IRQ on smtc-hal-uart. */
int unified_uart_init( void );

void     unified_uart_tx( uint8_t* buff, uint16_t len, unified_uart_message_type_t type );
uint16_t unified_uart_rx_available( void );
bool     unified_uart_rx_get_byte( uint8_t* byte );
bool     unified_uart_rx_peek( uint16_t offset, uint8_t* byte );
void     unified_uart_rx_flush( void );

#ifdef __cplusplus
}
#endif

#endif /* UNIFIED_UART_H__ */
