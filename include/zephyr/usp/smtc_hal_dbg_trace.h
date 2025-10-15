/**
 * @file      smtc_hal_dbg_trace.h
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

#ifndef SMTC_HAL_DBG_TRACE_H
#define SMTC_HAL_DBG_TRACE_H

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

#include <zephyr/logging/log.h>

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC MACROS -----------------------------------------------------------
 */
#ifndef SMTC_HAL_DBG_TRACE_C
LOG_MODULE_DECLARE( usp, CONFIG_USP_LOG_LEVEL );
#endif

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

#define SMTC_HAL_TRACE_MSG( msg ) SMTC_LOG( LOG_INF, "%s", msg );
#define SMTC_HAL_TRACE_PRINTF( ... ) SMTC_LOG( LOG_INF, __VA_ARGS__ );
#define SMTC_HAL_TRACE_INFO( ... ) SMTC_LOG( LOG_INF, __VA_ARGS__ )
#define SMTC_HAL_TRACE_WARNING( ... ) SMTC_LOG( LOG_WRN, __VA_ARGS__ );
#define SMTC_HAL_TRACE_ERROR( ... ) SMTC_LOG( LOG_ERR, __VA_ARGS__ );
#define SMTC_HAL_TRACE_ARRAY( msg, array, len ) LOG_HEXDUMP_INF( array, len, msg );
#define SMTC_HAL_TRACE_PACKARRAY( msg, array, len ) LOG_HEXDUMP_INF( array, len, msg );

#ifdef __cplusplus
}
#endif

#endif  // SMTC_HAL_DBG_TRACE_H

/* --- EOF ------------------------------------------------------------------ */
