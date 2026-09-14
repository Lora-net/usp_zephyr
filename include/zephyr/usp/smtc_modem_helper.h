/**
 * @file      smtc_modem_helper.h
 *
 * @brief     smtc_modem_helper implementation
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2026. All rights reserved.
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

#ifndef SMTC_MODEM_HELPER_H
#define SMTC_MODEM_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined( CONFIG_USP_LORA_BASICS_MODEM )

#include <smtc_modem_api.h>
#include <smtc_modem_utilities.h>

#if defined( CONFIG_LORA_BASICS_MODEM_RELAY_TX )
#include <smtc_modem_relay_api.h>
#endif

/**
 * @brief Helper macro that returned a human-friendly message if a command does not return
 * SMTC_MODEM_RC_OK
 *
 * @remark The macro is implemented to be used with functions returning a @ref
 * smtc_modem_return_code_t
 *
 * @param[in] rc  Return code
 */
void assert_smtc_modem_rc( const char* file, const char* func, int line, smtc_modem_return_code_t rc );

#define ASSERT_SMTC_MODEM_RC( rc_func ) assert_smtc_modem_rc( __FILE__, __func__, __LINE__, rc_func )

#if DT_NODE_HAS_PROP( DT_PATH( zephyr_user ), user_lorawan_region )

/**
 * @def STACK_ID
 * @brief Define the stack id value (multistacks modem is not yet available)
 */
#define STACK_ID 0

#define DT_MODEM_REGION( region ) DT_CAT( SMTC_MODEM_REGION_, region )
#define MODEM_REGION DT_MODEM_REGION( DT_STRING_UNQUOTED( DT_PATH( zephyr_user ), user_lorawan_region ) )

/**
 * @brief User callback for modem event
 *
 *  This callback is called every time an event ( see smtc_modem_event_t ) appears in the modem.
 *  Several events may have to be read from the modem when this callback is called.
 */
void modem_event_callback( void );

/**
 * @brief Optional modem event hook.
 *
 * Default implementation is weak and can be overridden by a strong implementation
 * in application code.
 *
 * @param[in] event_type Modem event type.
 */
void smtc_modem_helper_event( smtc_modem_event_type_t event_type );

/**
 * @brief Function to get device EUI or chip EUI depending on the configuration
 *
 * @return Pointer to the EUI
 */
uint8_t* smtc_modem_helper_get_eui( void );

#if !defined( CONFIG_LORA_BASICS_MODEM_CRYPTOGRAPHY_LR11XX_WITH_CREDENTIALS )
uint8_t* smtc_modem_helper_get_join_eui( void );
uint8_t* smtc_modem_helper_get_gen_app_key( void );
uint8_t* smtc_modem_helper_get_app_key( void );
#else
uint8_t* smtc_modem_helper_get_pin( void );
#endif

#if defined( CONFIG_LORA_BASICS_MODEM_RELAY_TX )
smtc_modem_relay_tx_config_t* smtc_modem_helper_get_relay_config( void );
#endif

uint8_t*                  smtc_modem_helper_get_rx_payload( void );
uint8_t*                  smtc_modem_helper_get_rx_payload_size( void );
smtc_modem_dl_metadata_t* smtc_modem_helper_get_rx_metadata( void );
uint8_t*                  smtc_modem_helper_get_rx_remaining( void );

#endif  // DT_NODE_HAS_PROP( DT_PATH( zephyr_user ), user_lorawan_region )

#endif  // CONFIG_USP_LORA_BASICS_MODEM

#ifdef __cplusplus
}
#endif

#endif  // SMTC_MODEM_HELPER_H
