/**
 * @file      lr11xx.h
 *
 * @brief     lr11xx implementation
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

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_USP_LR11XX_BINDINGS_DEF_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_USP_LR11XX_BINDINGS_DEF_H_

#define LR11XX_DIO5 ( 1 << 0 )
#define LR11XX_DIO6 ( 1 << 1 )
#define LR11XX_DIO7 ( 1 << 2 )
#define LR11XX_DIO8 ( 1 << 3 )
#define LR11XX_DIO9 ( 1 << 4 )

/* Those variables are already defined in the radio_drivers from LBM, but duplicated
 * for the device tree preprocessor that can't read enums.
 * See modules/lib/lora_basics_modem/lbm_lib/smtc_modem_core/radio_drivers/lr11xx_driver/src/lr11xx_system_types.h
 */

#define LR11XX_SYSTEM_RFSW0_HIGH LR11XX_DIO5
#define LR11XX_SYSTEM_RFSW1_HIGH LR11XX_DIO6
#define LR11XX_SYSTEM_RFSW2_HIGH LR11XX_DIO7
#define LR11XX_SYSTEM_RFSW3_HIGH LR11XX_DIO8

/* Only the low frequency low power path is placed. */
#define LR11XX_TX_PATH_LF_LP 0
/* Only the low frequency high power power path is placed. */
#define LR11XX_TX_PATH_LF_HP 1
/* Both the low frequency low power and low frequency high power paths are placed. */
#define LR11XX_TX_PATH_LF_LP_HP 2

#define LR11XX_TCXO_SUPPLY_1_6V 0x00 /* Supply voltage = 1.6v */
#define LR11XX_TCXO_SUPPLY_1_7V 0x01 /* Supply voltage = 1.7v */
#define LR11XX_TCXO_SUPPLY_1_8V 0x02 /* Supply voltage = 1.8v */
#define LR11XX_TCXO_SUPPLY_2_2V 0x03 /* Supply voltage = 2.2v */
#define LR11XX_TCXO_SUPPLY_2_4V 0x04 /* Supply voltage = 2.4v */
#define LR11XX_TCXO_SUPPLY_2_7V 0x05 /* Supply voltage = 2.7v */
#define LR11XX_TCXO_SUPPLY_3_0V 0x06 /* Supply voltage = 3.0v */
#define LR11XX_TCXO_SUPPLY_3_3V 0x07 /* Supply voltage = 3.3v */

#define LR11XX_LFCLK_RC 0x00   /* Use 32.768kHz RC oscillator */
#define LR11XX_LFCLK_XTAL 0x01 /* Use 32.768kHz crystal oscillator */
#define LR11XX_LFCLK_EXT 0x02  /* Use externally provided 32.768kHz signal on DIO11 pin */

#define LR11XX_REG_MODE_LDO 0x00  /* Do not switch on the DC-DC converter in any mode */
#define LR11XX_REG_MODE_DCDC 0x01 /* Automatically switch on the DC-DC converter when required */

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_USP_LR11XX_BINDINGS_DEF_H_*/
