/**
 * @file      sx126x.h
 *
 * @brief     sx126x implementation
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

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_USP_SX126X_BINDINGS_DEF_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_USP_SX126X_BINDINGS_DEF_H_

/* Those variables are already defined in the radio_drivers from LBM, but duplicated
 * for the device tree preprocessor that can't read enums.
 * See modules/lib/lora_basics_modem/lbm_lib/smtc_modem_core/radio_drivers/sx126x_driver/src/sx126x.h
 */

#define SX126X_REG_MODE_LDO 0x00 /* default */
#define SX126X_REG_MODE_DCDC 0x01

#define SX126X_TCXO_SUPPLY_1_6V 0x00
#define SX126X_TCXO_SUPPLY_1_7V 0x01
#define SX126X_TCXO_SUPPLY_1_8V 0x02
#define SX126X_TCXO_SUPPLY_2_2V 0x03
#define SX126X_TCXO_SUPPLY_2_4V 0x04
#define SX126X_TCXO_SUPPLY_2_7V 0x05
#define SX126X_TCXO_SUPPLY_3_0V 0x06
#define SX126X_TCXO_SUPPLY_3_3V 0x07

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_USP_SX126X_BINDINGS_DEF_H_*/
