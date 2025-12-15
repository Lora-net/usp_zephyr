/**
 * @file      smtc_modem_hal_dbg_trace.c
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

#include <ctype.h>

#include <zephyr/logging/log.h>

/* Prevent LOG_MODULE_DECLARE and LOG_MODULE_REGISTER being called together */
#define SMTC_MODEM_HAL_DBG_TRACE_C
#include <smtc_modem_hal_dbg_trace.h>

#ifdef CONFIG_USP
LOG_MODULE_REGISTER( lorawan, CONFIG_USP_LOG_LEVEL );
#elif CONFIG_LORA_BASICS_MODEM
LOG_MODULE_REGISTER( lorawan, CONFIG_LORA_BASICS_MODEM_LOG_LEVEL );
#endif

void smtc_str_trim_end( char* text )
{
    /* Find first trailing space */
    char* end = text + strlen( text ) - 1;
    while( end > text && isspace( ( unsigned char ) *end ) )
    {
        end--;
    }

    /* Write new null terminator character */
    end[1] = '\0';
}
