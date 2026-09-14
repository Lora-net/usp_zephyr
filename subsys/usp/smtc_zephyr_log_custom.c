/**
 * @file      zephyr_log_custom.c
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
#include <time.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/logging/log_output_custom.h>
#include <zephyr/usp/smtc_zephyr_log_custom.h>
#if( CONFIG_USP_LORA_BASICS_MODEM )
#include <smtc_modem_api.h>
#endif
#include <smtc_modem_hal.h>


/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */


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
#if( CONFIG_USP_LORA_BASICS_MODEM )
static uint8_t stack_id = 0;
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

 /**
 * @brief User timestamp formatter for logging
 *
 *  This function is called every time a timestamp need to be formated by the logging module.
 *  @param output A pointer to the log output instance @see log_output
 *  @param timestamp The timestamp value @see log_timestamp_t
 *  @param printer Prototype of a printer function that can print the given timestamp @see log_timestamp_printer_t
 */
 static int custom_timestamp_formatter( const struct log_output* output, const log_timestamp_t timestamp,
                                const log_timestamp_printer_t printer );

/**
 * @brief User get timestamp for logging
 *
 *  This function is called when a timestamp is requested by the logging module.
 *  @return a timestamp value @see log_timestamp_t
 */
static log_timestamp_t log_timestamp( void );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void custom_log_init( uint8_t stack_id )
{
#if( CONFIG_USP_LORA_BASICS_MODEM )
    stack_id = stack_id;
#endif

#ifdef CONFIG_LOG_TIMESTAMP_64BIT
    log_set_timestamp_func( log_timestamp, 1000 );
#else
    log_set_timestamp_func( log_timestamp, 1 );
#endif
    log_custom_timestamp_set( custom_timestamp_formatter );
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTION DEFINITIONS --------------------------------------------
 */

 static int custom_timestamp_formatter( const struct log_output* output, const log_timestamp_t timestamp,
                                const log_timestamp_printer_t printer )
{
#ifdef CONFIG_LOG_TIMESTAMP_64BIT
    uint64_t   timestamp_ms = ( uint64_t ) timestamp;
    uint32_t   seconds      = timestamp_ms / 1000;
    uint32_t   milliseconds = timestamp_ms % 1000;
    time_t     unix_time    = seconds;
    struct tm* timeinfo     = gmtime( &unix_time );

    if( timeinfo != NULL )
    {
        return printer( output, "[%04d-%02d-%02d %02d:%02d:%02d.%03u] ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                        timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, milliseconds );
    }
    else
    {
        return printer( output, "[%u.%03u] ", seconds, milliseconds );
    }
#else
    uint32_t   seconds      = (uint32_t)timestamp;
    time_t     unix_time    = seconds;
    struct tm* timeinfo     = gmtime( &unix_time );

    if( timeinfo != NULL )
    {
        return printer( output, "[%04d-%02d-%02d %02d:%02d:%02d] ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                        timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec );
    }    
    else
    {
        return printer( output, "[%u.%03u] ", seconds, 0 );
    }
#endif

}

static log_timestamp_t log_timestamp( void )
{
    log_timestamp_t ret              = 0;

#if( CONFIG_USP_LORA_BASICS_MODEM )
    uint32_t        gps_time_s       = 0;
    uint32_t        gps_fractional_s = 0;
    if( smtc_modem_get_lorawan_mac_time( stack_id, &gps_time_s, &gps_fractional_s ) == SMTC_MODEM_RC_OK )
    {
        ret = ( log_timestamp_t ) gps_time_s;
        ret += UNIX_GPS_EPOCH_OFFSET;
#ifdef CONFIG_LOG_TIMESTAMP_64BIT
        ret *= 1000;
        ret += ( gps_fractional_s );
#endif
    }
    else
    {
#endif  // CONFIG_USP_LORA_BASICS_MODEM
        ret = ( log_timestamp_t ) UNIX_GPS_EPOCH_OFFSET;
#ifdef CONFIG_LOG_TIMESTAMP_64BIT
        ret *= 1000;
        ret += ( log_timestamp_t ) ( smtc_modem_hal_get_time_in_ms( ) );
#else
        ret += ( log_timestamp_t ) ( smtc_modem_hal_get_time_in_ms( ) / 1000 );
#endif

#if( CONFIG_USP_LORA_BASICS_MODEM )
    }
#endif  // CONFIG_USP_LORA_BASICS_MODEM

    return ret;
}

/* --- EOF ------------------------------------------------------------------ */
