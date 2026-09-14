/**
 * @file      flrp_remote.h
 *
 * @brief     FLRP remote over lorwaran commands
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

#ifndef FLRP_REMOTE_H
#define FLRP_REMOTE_H

#include <stdint.h>
#include "flrp_defs.h"

/** @def FLRP_REMOTE_PORT
  @brief Define the lorawan port used to send FLRP remote control commands
*/
#define FLRP_REMOTE_PORT 12

/** @def FLRP_REMOTE_PORT
  @brief Define the size of the encryption key
*/
#define FLRP_REMOTE_KEY_SIZE 16

/** @enum flrp_remote_cmd_id_t
  @brief Define the command id values
*/
typedef enum flrp_remote_cmd_id_e
{
    FLRP_REMOTE_CMD_ID_RESET           = 0x00, /**< Request reset command */
    FLRP_REMOTE_CMD_ID_INIT            = 0x01, /**< Request init command */
    FLRP_REMOTE_CMD_ID_INIT_TRANSFER   = 0x02, /**< Request init transfer command */
    FLRP_REMOTE_CMD_ID_STOP_LISTENING  = 0x03, /**< Request stop listening command */
    FLRP_REMOTE_CMD_ID_START_LISTENING = 0x04, /**< Request start listening command */
} flrp_remote_cmd_id;
typedef uint8_t flrp_remote_cmd_id_t;

/** @struct flrp_remote_init_options_t
  @brief Define the initialization options structure
*/
typedef struct __attribute__( ( packed ) ) flrp_remote_init_options_s
{
    uint8_t crypto_enabled : 1;   /**< Indicates if encryption must be enabled for the transfer */
    uint8_t is_low_frequency : 1; /**< Indicates if the transfer will be done in low frequency band (868MHz) or high
                                     frequency band (2.4GHz) */
    uint8_t listening : 1;        /**< Indicates if the device must start periodical listening or not */
    uint8_t reserved : 5;         /**< Reserved bits */
} flrp_api_flrp_remote_init_options_t;

/** @struct flrp_remote_init_cmd_t
  @brief Define the initialization command structure
*/
typedef struct __attribute__( ( packed ) ) flrp_remote_init_cmd_s
{
    flrp_remote_cmd_id_t cmd_id;                           /**< Command ID @see flrp_remote_cmd_id_t */
    uint8_t              device_eui[SMTC_FLRP_EUI_LENGTH]; /**< Device EUI */
    uint8_t              key[FLRP_REMOTE_KEY_SIZE];        /**< Encryption key */
    flrp_api_flrp_remote_init_options_t
        init_options; /**< Initialization options @see flrp_api_flrp_remote_init_options_t */
} flrp_remote_init_cmd_t;

/** @struct flrp_remote_transmit_cmd_t
  @brief Define the transmit command structure
*/
typedef struct __attribute__( ( packed ) ) flrp_remote_transmit_cmd_s
{
    flrp_remote_cmd_id_t cmd_id;                                  /**< Command ID @see flrp_remote_cmd_id_t */
    uint8_t              target_device_eui[SMTC_FLRP_EUI_LENGTH]; /**< Target Device EUI */
    uint8_t              transmit : 1; /**< Indicates that the command is a transmit or receive command */
    uint8_t              reserved : 7; /**< Reserved bits */
} flrp_remote_transmit_cmd_t;

void flrp_remote_send_keep_alive( void );

#endif  // FLRP_REMOTE_H