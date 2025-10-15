/**
 * @file      zephyr_usp_thread.c
 *
 * @brief     zephyr_usp_thread implementation
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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#if defined( CONFIG_USP_LORA_BASICS_MODEM )
#include <smtc_modem_utilities.h>
#endif
#include <smtc_rac_api.h>

#include "zephyr/usp/smtc_sw_platform_helper.h"
#include "zephyr_usp_initialization.h"
#include "zephyr_usp_api_call.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */
LOG_MODULE_DECLARE( usp, CONFIG_USP_LOG_LEVEL );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE Types -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static const struct device* transceiver = DEVICE_DT_GET( DT_CHOSEN( zephyr_lorawan_transceiver ) );

static K_THREAD_STACK_DEFINE( usp_main_thread_stack, CONFIG_USP_MAIN_THREAD_STACK_SIZE );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */
static void usp_main_thread( void* p1, void* p2, void* p3 );

K_THREAD_DEFINE( lbm_main_thread_id, K_THREAD_STACK_SIZEOF( usp_main_thread_stack ), usp_main_thread, NULL, NULL,
                 NULL, CONFIG_USP_MAIN_THREAD_PRIORITY, 0, 0 );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static void usp_main_thread( void* p1, void* p2, void* p3 )
{
    uint32_t sleep_time_ms = 0;

    ARG_UNUSED( p1 );
    ARG_UNUSED( p2 );
    ARG_UNUSED( p3 );

    // Initialize smtc_modem hal (driver callback setting & driver HAL implementation for zephyr)
    lorawan_smtc_modem_hal_init( transceiver );

    // Initialize USP/RAC
    //     smtc_rac_init();

    // Notify User threads USP/RAC is Ready
    zephyr_usp_initialization_notify( );

    LOG_INF( "Starting loop..." );
    while( true )
    {
#if defined( CONFIG_USP_THREADS_MUTEXES )
        k_mutex_lock( &rac_api_mutex, K_FOREVER );
#endif
#if defined( CONFIG_USP_LORA_BASICS_MODEM )
        if( smtc_is_modem_initialized( ) == false )
        {
#if defined( CONFIG_USP_THREADS_MUTEXES )
            k_mutex_unlock( &rac_api_mutex );
#endif
            smtc_modem_hal_interruptible_msleep( K_MSEC( 50 ) );
            continue;
        }
        sleep_time_ms = smtc_modem_run_engine( );
#else  // #if defined( CONFIG_USP_LORA_BASICS_MODEM )
        sleep_time_ms = CONFIG_USP_MAIN_THREAD_MAX_SLEEP_MS;
#endif
        smtc_rac_run_engine( );
#if defined( CONFIG_USP_THREADS_MUTEXES )
        k_mutex_unlock( &rac_api_mutex );
#endif
        if( smtc_rac_is_irq_flag_pending( ) )
        {
            continue;
        }

#if CONFIG_USP_MAIN_THREAD_MAX_SLEEP_MS
        sleep_time_ms = MIN( sleep_time_ms, CONFIG_USP_MAIN_THREAD_MAX_SLEEP_MS );
#endif
        LOG_DBG( "Sleeping for %dms", sleep_time_ms );
        smtc_modem_hal_interruptible_msleep( K_MSEC( sleep_time_ms ) );
    }
}
