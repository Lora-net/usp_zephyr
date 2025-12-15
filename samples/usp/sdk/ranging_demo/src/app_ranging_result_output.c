/**
 * @file      app_ranging_result_output.c
 *
 * @brief     Print the ranging results
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
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "smtc_rac_log.h"
#include "apps_configuration.h"
#include "smtc_hal_dbg_trace.h"
#include "app_ranging_result_output.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */
#define RANGING_LOG_RESULT printf

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

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void app_ranging_radio_settings_output( smtc_rac_radio_lora_params_t* lora_settings,
                                        ranging_params_settings_t*    ranging_settings )
{
    RANGING_LOG_RESULT( "\r\n" );
    RANGING_LOG_RESULT( "{\r\n" );
    RANGING_LOG_RESULT( "\"SF\": \"%s\",\r\n", ral_lora_sf_to_str( lora_settings->sf ) );
    RANGING_LOG_RESULT( "\"BW\": \"%s\",\r\n", ral_lora_bw_to_str( lora_settings->bw ) );
}

void app_ranging_results_output( bool is_manager, ranging_global_result_t* result, int count )
{
    uint32_t freq;

    RANGING_LOG_RESULT( "\"ROLE\": \"%s\",\r\n", ( is_manager == true ) ? "MANAGER" : "SUBORDINATE" );
    RANGING_LOG_RESULT( "\"LoRa RSSI\": \"%d dBm\",\r\n", result->rssi_value );
    RANGING_LOG_RESULT( "\"LoRa SNR\": %d,\r\n", result->snr_value );

    if( result->cnt_packet_rx_ok == 0 )
    {
        RANGING_LOG_RESULT( "\"RngResult\": {\r\n\t\"Num\": %d\r\n\t}\r\n},\r\n", result->cnt_packet_rx_ok );
        return;
    }

    RANGING_LOG_RESULT( "\"RngResult\": {\r\n\t\"Num\": %d,\r\n\t\"Results\": [\r\n", result->cnt_packet_rx_ok );
    for( int i = 0; i < count; i++ )
    {
        if( result->rng_result[i].distance_m == 0 && result->rng_result[i].rssi == 0 )
            continue;

        // Get the channel frequency according to index
        freq = get_ranging_hopping_channels( i );

        RANGING_LOG_RESULT( "\t\t{\"FreqIndex\": \"%d\", ", i );
        RANGING_LOG_RESULT( "\"Freq\": \"%" PRIu32 ".%02" PRIu32 " MHz\", ", freq / 1000000, ( freq / 10000 ) % 100 );
        RANGING_LOG_RESULT( "\"RawDistance\": \"0x%08" PRIx32 "\", ", result->rng_result[i].raw_distance );
        RANGING_LOG_RESULT( "\"Distance\": \"%" PRId32 " m\", ", result->rng_result[i].distance_m );

        if( i < count - 1 )
        {
            RANGING_LOG_RESULT( "\"RSSI\": \"%d dBm\" },\r\n", result->rng_result[i].rssi );
        }
        else
        {
            RANGING_LOG_RESULT( "\"RSSI\": \"%d dBm\" }\r\n", result->rng_result[i].rssi );
        }
    }

    RANGING_LOG_RESULT( "\t\t],\r\n" );
    RANGING_LOG_RESULT( "\t\"DistanceRng\": \"%" PRId32 " m\",\r\n", result->rng_distance );
    RANGING_LOG_RESULT( "\t\"PER\": \"%d %%\"\r\n\t}\r\n", result->rng_per );
    RANGING_LOG_RESULT( "},\r\n" );
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTION DEFINITIONS --------------------------------------------
 */

/* --- EOF ------------------------------------------------------------------ */
