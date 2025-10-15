/**
 * @file      smtc_sw_platform_helper.h
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

#ifndef PLATFORM_HELPER_H
#define PLATFORM_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <zephyr/kernel.h>

// To map SMTC_SW_PLATFORM(RACAPI) to zephyr RAC API
#include <zephyr/usp/smtc_zephyr_usp_api.h>

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */

#if !defined( CONFIG_USP_MAIN_THREAD )
extern const struct device* transceiver;
#endif

// To map SMTC_SW_PLATFORM(RACAPI) to zephyr RAC API or to direct RAC  API with mutex protection
#if defined( CONFIG_USP_MAIN_THREAD )

#define SMTC_SW_PLATFORM_INIT( )           \
    do                                     \
    {                                      \
        zephyr_usp_initialization_wait( ); \
    } while( 0 )

#if defined( CONFIG_USP_THREADS_MUTEXES )
extern struct k_mutex rac_api_mutex;
#define SMTC_SW_PLATFORM( call )                   \
    ( {                                            \
        k_mutex_lock( &rac_api_mutex, K_FOREVER ); \
        __auto_type __result = ( call );           \
        k_mutex_unlock( &rac_api_mutex );          \
        __result;                                  \
    } )
#define SMTC_SW_PLATFORM_VOID( call )              \
    ( {                                            \
        k_mutex_lock( &rac_api_mutex, K_FOREVER ); \
        ( call );                                  \
        k_mutex_unlock( &rac_api_mutex );          \
    } )
#else  // #if defined(CONFIG_USP_THREADS_MUTEXES)
#define SMTC_SW_PLATFORM( call ) call
#define SMTC_SW_PLATFORM_VOID( call ) call
#endif
#else  // #if defined(CONFIG_USP_MAIN_THREAD)

#define SMTC_SW_PLATFORM_INIT( )                    \
    do                                              \
    {                                               \
        lorawan_smtc_modem_hal_init( transceiver ); \
    } while( 0 )

#define SMTC_SW_PLATFORM( call ) call
#define SMTC_SW_PLATFORM_VOID( call ) call
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC CONSTANTS --------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC TYPES ------------------------------------------------------------
 */
typedef enum
{
    SMTC_PF_LED_RX,
    SMTC_PF_LED_TX,
    SMTC_PF_LED_SCAN,
    SMTC_PF_LED_MAX
} smtc_led_pin_e;

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS PROTOTYPES ---------------------------------------------
 */

/**
 * @brief Configure LEDs
 */
void init_leds( void );

/**
 * @brief Toggle the state of the TX and RX LEDs
 */
void toggle_led( void );

/**
 * @brief Set the state of a specific LED
 * @param led The LED identifier
 * @param state true to turn on, false to turn off
 */
void set_led( smtc_led_pin_e led, bool state );

/**
 * @brief The function waits for one of the semaphores to be given or for a timeout to occur.
 * @param sems An array of pointers to the semaphores to wait on @see k_sem.
 * @param count The number of semaphores in the array @see size_t.
 * @param timeout The maximum time to wait for a semaphore to be given @see k_timeout_t.
 * @retval -1 Parameter error (count is 0 or sems is NULL)
 * @retval -2 Timeout or error during k_poll
 * @retval -3 No semaphore was given, unexpected issue
 * @retval >=0 Index of the semaphore that was given
 */
int wait_on_sems( struct k_sem* sems[], size_t count, k_timeout_t timeout );

#ifdef __cplusplus
}
#endif

#endif  // PLATFORM_HELPER_H

/* --- EOF ------------------------------------------------------------------ */
