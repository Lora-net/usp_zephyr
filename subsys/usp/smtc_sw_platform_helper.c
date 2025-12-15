/**
 * @file      smtc_sw_platform_helper.c
 *
 * @brief     Ranging and frequency hopping for LR1110 or LR1120 chip
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

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */
#include <stdint.h>  // C99 types

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include "smtc_sw_platform_helper.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

#define HAS_LED_SCAN DT_NODE_EXISTS( DT_NODELABEL( lora_scanning_led ) )
#define HAS_LED_TXRX \
    ( ( DT_NODE_EXISTS( DT_NODELABEL( lora_rx_led ) ) ) && ( DT_NODE_EXISTS( DT_NODELABEL( lora_tx_led ) ) ) )

#if HAS_LED_TXRX
#define RX_LED_NODE DT_NODELABEL( lora_rx_led )
#define TX_LED_NODE DT_NODELABEL( lora_tx_led )
#endif
#if HAS_LED_SCAN
#define SCAN_LED_NODE DT_NODELABEL( lora_scanning_led )
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */
// If USP/RAC thread is not used, the transceiver has to be initialized
#if !defined( CONFIG_USP_MAIN_THREAD )
const struct device* transceiver = DEVICE_DT_GET( DT_CHOSEN( zephyr_lorawan_transceiver ) );
#endif

#if defined( CONFIG_USP_MAIN_THREAD )
#if defined( CONFIG_USP_THREADS_MUTEXES )
K_MUTEX_DEFINE( rac_api_mutex );
#endif
#endif

static const struct gpio_dt_spec pf_led_pin[SMTC_PF_LED_MAX] = {
#if HAS_LED_TXRX
    [SMTC_PF_LED_RX] = GPIO_DT_SPEC_GET( RX_LED_NODE, gpios ),
    [SMTC_PF_LED_TX] = GPIO_DT_SPEC_GET( TX_LED_NODE, gpios ),
#else
    [SMTC_PF_LED_RX] = { 0 },
    [SMTC_PF_LED_TX] = { 0 },
#endif
#if HAS_LED_SCAN
    [SMTC_PF_LED_SCAN] = GPIO_DT_SPEC_GET( SCAN_LED_NODE, gpios ),
#else
    [SMTC_PF_LED_SCAN] = { 0 },
#endif
};

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/**
 * @brief Configure LEDs
 */
void init_leds( void )
{
#if HAS_LED_TXRX
    ( void ) gpio_pin_configure_dt( &pf_led_pin[SMTC_PF_LED_RX], GPIO_OUTPUT_INACTIVE );
    ( void ) gpio_pin_configure_dt( &pf_led_pin[SMTC_PF_LED_TX], GPIO_OUTPUT_INACTIVE );
#endif
#if HAS_LED_SCAN
    ( void ) gpio_pin_configure_dt( &pf_led_pin[SMTC_PF_LED_SCAN], GPIO_OUTPUT_INACTIVE );
#endif
}

/**
 * @brief Toggle the state of the TX and RX LEDs
 */
void toggle_led( void )
{
#if HAS_LED_TXRX
    gpio_pin_toggle_dt( &pf_led_pin[SMTC_PF_LED_RX] );
    gpio_pin_toggle_dt( &pf_led_pin[SMTC_PF_LED_TX] );
#endif
}

/**
 * @brief Set the state of a specific LED
 * @param led The LED identifier
 * @param state true to turn on, false to turn off
 */
void set_led( smtc_led_pin_e led, bool state )
{
    if( led < 0 || led >= SMTC_PF_LED_MAX )
    {
        return;
    }
    if( led == SMTC_PF_LED_SCAN )
    {
#if !HAS_LED_SCAN
        return;
#endif
    }

    if( led == SMTC_PF_LED_TX || led == SMTC_PF_LED_RX )
    {
#if !HAS_LED_TXRX
        return;
#endif
    }

#if HAS_LED_SCAN || HAS_LED_TXRX
    ( void ) gpio_pin_set_dt( &pf_led_pin[led], state ? 1 : 0 );
#endif
}

int wait_on_sems( struct k_sem* sems[], size_t count, k_timeout_t timeout )
{
    if( count == 0 || sems == NULL )
    {
        return -1;  // Parameter error
    }

    struct k_poll_event events[count];
    for( size_t i = 0; i < count; i++ )
    {
        k_poll_event_init( &events[i], K_POLL_TYPE_SEM_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY, sems[i] );
    }

    int ret = k_poll( events, count, timeout );
    if( ret != 0 )
    {
        return -2;  // Timeout or error
    }

    for( size_t i = 0; i < count; i++ )
    {
        if( events[i].state == K_POLL_STATE_SEM_AVAILABLE )
        {
            k_sem_take( sems[i], K_NO_WAIT );
            return ( int ) i;
        }
    }

    return -3;  // No semaphore, issue
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTION DEFINITIONS --------------------------------------------
 */

/* --- EOF ------------------------------------------------------------------ */
