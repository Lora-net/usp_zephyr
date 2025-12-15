/**
 * @file      lora_lbm_transceiver.h
 *
 * @brief     lora_lbm_transceiver implementation
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

#ifndef LORA_LBM_TRANSCEIVER_H
#define LORA_LBM_TRANSCEIVER_H

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback upon firing event trigger
 *
 */
typedef void ( *event_cb_t )( const struct device* dev );

/**
 * @brief Attach interrupt cb to event pin.
 *
 * @param dev context
 * @param cb cb function
 */
void lora_transceiver_board_attach_interrupt( const struct device* dev, event_cb_t cb );

/**
 * @brief Enable interrupt on event pin.
 *
 * @param dev context
 */
void lora_transceiver_board_enable_interrupt( const struct device* dev );

/**
 * @brief Disable interrupt on event pin.
 *
 * @param dev context
 */
void lora_transceiver_board_disable_interrupt( const struct device* dev );

/**
 * @brief Helper to get the tcxo startup delay for any model of transceiver
 *
 * @param dev context
 */
uint32_t lora_transceiver_get_tcxo_startup_delay_ms( const struct device* dev );

/**
 * @brief Returns lr11xx_system_version_type_t or -1
 *
 * @param dev context
 */

int32_t lora_transceiver_get_model( const struct device* dev );

/**
 * @brief Set the Tx power offset in dB
 *
 * @param [in] context Chip implementation context
 * @param [in] tx_pwr_offset_db
 */
void radio_utilities_set_tx_power_offset( const void* context, uint8_t tx_pwr_offset_db );

/**
 * @brief Get the Tx power offset in dB
 *
 * @param [in] context Chip implementation context
 *
 * @return Tx power offset in dB
 */
uint8_t radio_utilities_get_tx_power_offset( const void* context );

#ifdef __cplusplus
}
#endif

#endif /* LORA_LBM_TRANSCEIVER_H */
