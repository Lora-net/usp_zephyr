/**
 * @file      smtc_zephyr_usp_api.h
 *
 * @brief     smtc_zephyr_usp_api implementation
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

#ifndef SUBSYS_USP_ZEPHYR_USP_API_H
#define SUBSYS_USP_ZEPHYR_USP_API_H

#include <stdint.h>  // C99 types

#include <zephyr/types.h>

#include <smtc_rac_api.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ensure the RAC thread is initialized. Can block the called thread.
 *
 */
extern void zephyr_usp_initialization_wait( void );

/**
 * @brief Initialize RAC
 *
 * @param event_callback The callback that will be called each time an modem event is raised internally
 * @param hal_cb User provided callbacks for Device Management
 */
extern void zephyr_smtc_rac_init( void );

/*!
 * \brief Send a message queue to Open Radio.
 *
 * Registers a callback to be called when the radio is available at the given priority.
 *
 * \param [in] callback Function pointer for the callback to be called when the radio is free.
 * \param [in] priority Priority of the request.
 *
 * \return Return the radio_access id.
 */
extern uint8_t zephyr_smtc_rac_open_radio_open_radio( smtc_rac_priority_t priority );

/*!
 * \brief Send a message queue to Enqueue a LoRa or ranging task transaction.
 *
 * \param [in] radio_access_id The radio access ID obtained from smtc_rac_open_radio.
 * \param [in] rac_config Pointer to the complete transaction context.
 *
 * \return Return code indicating success (SMTC_RAC_SUCCESS) or error (SMTC_RAC_ERROR).
 */
extern smtc_rac_return_code_t zephyr_smtc_rac_submit_radio_transaction( uint8_t radio_access_id );

/*!
 * \brief Send a message queue to Abort the pending Transaction.
 *
 * \param [in] radio_access_id The radio access ID obtained from smtc_rac_open_radio.
 *
 * \return Return code indicating success (SMTC_RAC_SUCCESS) or error (SMTC_RAC_ERROR).
 */
extern smtc_rac_return_code_t zephyr_smtc_rac_abort_radio_submit( uint8_t radio_access_id );

/*!
 * \brief Send a message queue to Close Radio.
 *
 * \param [in] radio_access_id The radio access ID obtained from smtc_rac_open_radio.
 *
 * \return Return code indicating success (SMTC_RAC_SUCCESS) or error (SMTC_RAC_ERROR).
 */
extern smtc_rac_return_code_t zephyr_smtc_rac_close_radio( uint8_t radio_access_id );

#if defined( CONFIG_USP_LORA_BASICS_MODEM )
/**
 * \brief Send a message queue to Init Modem
 *
 * \param [in] callback_event Called when a new modem event occurs
 *
 * \return Return code indicating success (SMTC_RAC_SUCCESS) or error (SMTC_RAC_ERROR).
 */
extern smtc_rac_return_code_t zephyr_smtc_modem_init( void ( *callback_event )( void ) );
#endif

#ifdef __cplusplus
}
#endif

#endif /* SUBSYS_USP_ZEPHYR_USP_API_H */
