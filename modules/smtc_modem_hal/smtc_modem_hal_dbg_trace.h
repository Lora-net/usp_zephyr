/**
 * @file      smtc_modem_hal_dbg_trace.h
 *
 * @brief     smtc_modem_hal_dbg_trace HAL implementation
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

#ifndef __SMTC_MODEM_HAL_DBG_TRACE_H__
#define __SMTC_MODEM_HAL_DBG_TRACE_H__

/**
 * @brief This file is provided *to* the LoRa Basics Modem source code as a
 * HAL implementation of its logging API.
 * It also provides some definitions like MODEM_HAL_FEATURE_ON.
 */

#include <stdio.h>

#include <zephyr/logging/log.h>

/* NOTE: Only required because other sources are missing this include… */
#include "smtc_modem_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SMTC_MODEM_HAL_DBG_TRACE_C
#ifdef CONFIG_USP
LOG_MODULE_DECLARE( lorawan, CONFIG_USP_LOG_LEVEL );
#elif CONFIG_LORA_BASICS_MODEM
LOG_MODULE_DECLARE( lorawan, CONFIG_LORA_BASICS_MODEM_LOG_LEVEL );
#endif
#endif

#define MODEM_HAL_FEATURE_OFF ( 0 )
#define MODEM_HAL_FEATURE_ON ( !MODEM_HAL_FEATURE_OFF )

/**
 * @brief Trims the end of a string
 *
 * This is used to trim Semtech's logging strings that contain trailing
 * spaces and endlines.
 * The trimming is done by writing a null character when the whitespaces start.
 *
 * @param[in] text The text to trim
 */
void smtc_str_trim_end( char* text );

/* NOTE: modem_utilities/circularfs.c will be patched in LBM 4.9.0 to reduce this size */
#define SMTC_PRINT_BUFFER_SIZE 220

/**
 * @brief This macro matches the Semtech LBM logging macros with the logging
 * macros LOG_* from zephyr, while trimming the endlines present in LBM.
 */
#define SMTC_LOG( log_fn, ... )                                        \
    do                                                                 \
    {                                                                  \
        char __log_buffer[SMTC_PRINT_BUFFER_SIZE];                     \
        snprintf( __log_buffer, SMTC_PRINT_BUFFER_SIZE, __VA_ARGS__ ); \
        smtc_str_trim_end( __log_buffer );                             \
        log_fn( "%s", __log_buffer );                                  \
    } while( 0 )

#define MODEM_HAL_DBG_TRACE MODEM_HAL_FEATURE_ON

#define MODEM_HAL_DBG_TRACE_COLOR_BLACK ""
#define MODEM_HAL_DBG_TRACE_COLOR_RED ""
#define MODEM_HAL_DBG_TRACE_COLOR_GREEN ""
#define MODEM_HAL_DBG_TRACE_COLOR_YELLOW ""
#define MODEM_HAL_DBG_TRACE_COLOR_BLUE ""
#define MODEM_HAL_DBG_TRACE_COLOR_MAGENTA ""
#define MODEM_HAL_DBG_TRACE_COLOR_CYAN ""
#define MODEM_HAL_DBG_TRACE_COLOR_WHITE ""
#define MODEM_HAL_DBG_TRACE_COLOR_DEFAULT ""

#define SMTC_MODEM_HAL_TRACE_MSG( msg ) SMTC_LOG( LOG_INF, "%s", msg );
#define SMTC_MODEM_HAL_TRACE_PRINTF( ... ) SMTC_LOG( LOG_INF, __VA_ARGS__ );
#define SMTC_MODEM_HAL_TRACE_INFO( ... ) SMTC_LOG( LOG_INF, __VA_ARGS__ )
#define SMTC_MODEM_HAL_TRACE_WARNING( ... ) SMTC_LOG( LOG_WRN, __VA_ARGS__ );
#define SMTC_MODEM_HAL_TRACE_ERROR( ... ) SMTC_LOG( LOG_ERR, __VA_ARGS__ );
#define SMTC_MODEM_HAL_TRACE_ARRAY( msg, array, len ) LOG_HEXDUMP_INF( array, len, msg );
#define SMTC_MODEM_HAL_TRACE_PACKARRAY( msg, array, len ) LOG_HEXDUMP_INF( array, len, msg );

#if CONFIG_LORA_BASICS_MODEM_LOG_VERBOSE

#define MODEM_HAL_DEEP_DBG_TRACE MODEM_HAL_FEATURE_ON

/* Deep debug trace default definitions*/
#define SMTC_MODEM_HAL_TRACE_PRINTF_DEBUG( ... ) SMTC_LOG( LOG_DBG, __VA_ARGS__ );
#define SMTC_MODEM_HAL_TRACE_MSG_DEBUG( msg ) SMTC_LOG( LOG_DBG, "%s", msg );
#define SMTC_MODEM_HAL_TRACE_MSG_COLOR_DEBUG( msg, color ) SMTC_LOG( LOG_DBG, "%s", msg );
#define SMTC_MODEM_HAL_TRACE_INFO_DEBUG( ... ) SMTC_LOG( LOG_INF, __VA_ARGS__ );
#define SMTC_MODEM_HAL_TRACE_WARNING_DEBUG( ... ) SMTC_LOG( LOG_WRN, __VA_ARGS__ );
#define SMTC_MODEM_HAL_TRACE_ERROR_DEBUG( ... ) SMTC_LOG( LOG_ERR, __VA_ARGS__ );
#define SMTC_MODEM_HAL_TRACE_ARRAY_DEBUG( msg, array, len ) LOG_HEXDUMP_DBG( array, len, msg );
#define SMTC_MODEM_HAL_TRACE_PACKARRAY_DEBUG( msg, array, len ) LOG_HEXDUMP_DBG( array, len, msg );

#else /* CONFIG_LORA_BASICS_MODEM_LOG_VERBOSE */

/* Deep debug trace default definitions*/
#define SMTC_MODEM_HAL_TRACE_PRINTF_DEBUG( ... )
#define SMTC_MODEM_HAL_TRACE_MSG_DEBUG( msg )
#define SMTC_MODEM_HAL_TRACE_MSG_COLOR_DEBUG( msg, color )
#define SMTC_MODEM_HAL_TRACE_INFO_DEBUG( ... )
#define SMTC_MODEM_HAL_TRACE_WARNING_DEBUG( ... )
#define SMTC_MODEM_HAL_TRACE_ERROR_DEBUG( ... )
#define SMTC_MODEM_HAL_TRACE_ARRAY_DEBUG( msg, array, len )
#define SMTC_MODEM_HAL_TRACE_PACKARRAY_DEBUG( ... )

#endif /* CONFIG_LORA_BASICS_MODEM_LOG_VERBOSE */

#if CONFIG_LORA_BASICS_MODEM_RADIO_PLANNER_LOG_VERBOSE

#define SMTC_MODEM_HAL_RP_TRACE_MSG( msg ) SMTC_MODEM_HAL_TRACE_PRINTF( msg )
#define SMTC_MODEM_HAL_RP_TRACE_PRINTF( ... ) SMTC_MODEM_HAL_TRACE_PRINTF( __VA_ARGS__ )

#else /* CONFIG_LORA_BASICS_MODEM_RADIO_PLANNER_LOG_VERBOSE */

#define SMTC_MODEM_HAL_RP_TRACE_MSG( msg )
#define SMTC_MODEM_HAL_RP_TRACE_PRINTF( ... )

#endif /* CONFIG_LORA_BASICS_MODEM_RADIO_PLANNER_LOG_VERBOSE */

#if CONFIG_LORA_BASICS_MODEM_GEOLOCATION_LOG_VERBOSE
#define GNSS_ALMANAC_DEEP_DBG_TRACE MODEM_HAL_FEATURE_ON
#define GNSS_SCAN_DEEP_DBG_TRACE MODEM_HAL_FEATURE_ON
#define GNSS_SEND_DEEP_DBG_TRACE MODEM_HAL_FEATURE_ON
#define WIFI_SCAN_DEEP_DBG_TRACE MODEM_HAL_FEATURE_ON
#define WIFI_SEND_DEEP_DBG_TRACE MODEM_HAL_FEATURE_ON
#endif /* CONFIG_LORA_BASICS_MODEM_GEOLOCATION_LOG_VERBOSE */

#ifdef __cplusplus
}
#endif

#endif /* __SMTC_MODEM_HAL_DBG_TRACE_H__*/
