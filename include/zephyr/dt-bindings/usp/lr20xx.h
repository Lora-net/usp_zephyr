/**
 * @file      lr20xx.h
 *
 * @brief     lr20xx implementation
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

#ifndef ZEPHYR_INCLUDE_DT_BINDINGS_USP_LR20XX_BINDINGS_DEF_H_
#define ZEPHYR_INCLUDE_DT_BINDINGS_USP_LR20XX_BINDINGS_DEF_H_

#define LR20XX_SYSTEM_DIO_5 0x05
#define LR20XX_SYSTEM_DIO_6 0x06
#define LR20XX_SYSTEM_DIO_7 0x07
#define LR20XX_SYSTEM_DIO_8 0x08
#define LR20XX_SYSTEM_DIO_9 0x09
#define LR20XX_SYSTEM_DIO_10 0x0A
#define LR20XX_SYSTEM_DIO_11 0x0B

#define LR20XX_SYSTEM_DIO_FUNC_NONE 0x00
#define LR20XX_SYSTEM_DIO_FUNC_IRQ 0x01
#define LR20XX_SYSTEM_DIO_FUNC_RF_SWITCH 0x02
#define LR20XX_SYSTEM_DIO_FUNC_GPIO_LOW 0x05
#define LR20XX_SYSTEM_DIO_FUNC_GPIO_HIGH 0x06
#define LR20XX_SYSTEM_DIO_FUNC_HF_CLK_OUT 0x07
#define LR20XX_SYSTEM_DIO_FUNC_LF_CLK_OUT 0x08
#define LR20XX_SYSTEM_DIO_FUNC_TX_TRIGGER 0x09
#define LR20XX_SYSTEM_DIO_FUNC_RX_TRIGGER 0x0A

#define LR20XX_SYSTEM_DIO_DRIVE_NONE 0x00
#define LR20XX_SYSTEM_DIO_DRIVE_PULL_DOWN 0x01
#define LR20XX_SYSTEM_DIO_DRIVE_PULL_UP 0x02
#define LR20XX_SYSTEM_DIO_DRIVE_AUTO 0x03 /* Pulls to the last standby value of the DIO */

#define LR20XX_SYSTEM_IRQ_NONE ( 0 << 0 )                          /* No interrupt */
#define LR20XX_SYSTEM_IRQ_FIFO_RX ( 1 << 0 )                       /* RX FIFO threshold reached */
#define LR20XX_SYSTEM_IRQ_FIFO_TX ( 1 << 1 )                       /* TX FIFO threshold reached */
#define LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_REQUEST_VALID ( 1 << 2 ) /* Responder received a valid RTToF request */
#define LR20XX_SYSTEM_IRQ_TX_TIMESTAMP ( 1 << 3 )                  /* Last bit sent timestamp */
#define LR20XX_SYSTEM_IRQ_RX_TIMESTAMP ( 1 << 4 )                  /* Last bit received timestamp */
#define LR20XX_SYSTEM_IRQ_PREAMBLE_DETECTED ( 1 << 5 )             /* Preamble detected */
#define LR20XX_SYSTEM_IRQ_SYNC_WORD_HEADER_VALID ( 1 << 6 )        /* Valid LoRa header received / Sync word received */
#define LR20XX_SYSTEM_IRQ_CAD_DETECTED ( 1 << 7 )                  /* Activity detected during CAD operation */
#define LR20XX_SYSTEM_IRQ_LORA_RX_HEADER_TIMESTAMP                                                                   \
    ( 1 << 8 ) /* Last bit of LoRa header received in explicit mode, asserted after 8 symbols of payload in implicit \
                  mode */
#define LR20XX_SYSTEM_IRQ_LORA_HEADER_ERROR ( 1 << 9 ) /* Erroneous LoRa header received */
#define LR20XX_SYSTEM_IRQ_POWER_SUPPLY_LOW ( 1 << 10 ) /* Power supply level dropped below the threshold */
#define LR20XX_SYSTEM_IRQ_ERROR \
    ( 1 << 16 ) /* Error other than a command error occurred - call lr20xx_system_get_errors */
#define LR20XX_SYSTEM_IRQ_CMD_ERROR ( 1 << 17 )             /* Host command fail/error occurred */
#define LR20XX_SYSTEM_IRQ_RX_DONE ( 1 << 18 )               /* Packet received */
#define LR20XX_SYSTEM_IRQ_TX_DONE ( 1 << 19 )               /* Packet sent */
#define LR20XX_SYSTEM_IRQ_CAD_DONE ( 1 << 20 )              /* CAD operation done */
#define LR20XX_SYSTEM_IRQ_TIMEOUT ( 1 << 21 )               /* Timeout occurred during Rx or Tx operation */
#define LR20XX_SYSTEM_IRQ_CRC_ERROR ( 1 << 22 )             /* Packet received with wrong CRC */
#define LR20XX_SYSTEM_IRQ_LEN_ERROR ( 1 << 23 )             /* Length of the received packet higher than expected */
#define LR20XX_SYSTEM_IRQ_ADDR_ERROR ( 1 << 24 )            /* Received packet discarded - no address match */
#define LR20XX_SYSTEM_IRQ_LR_FHSS_INTRA_PKT_HOP ( 1 << 25 ) /* LR-FHSS intra-packet hopping occurred */
#define LR20XX_SYSTEM_IRQ_LR_FHSS_RDY_FOR_NEW_FREQ_TABLE ( 1 << 26 ) /* A new LR-FHSS frequency table can be loaded */
#define LR20XX_SYSTEM_IRQ_LR_FHSS_RDY_FOR_NEW_PAYLOAD ( 1 << 27 )    /* A new LR-FHSS payload can be loaded */
#define LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_RESPONSE_DONE ( 1 << 28 )  /* Responder sent an RTToF response */
#define LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_REQUEST_DISCARDED \
    ( 1 << 29 ) /* Responder discarded the RTToF request - no address match */
#define LR20XX_SYSTEM_IRQ_RTTOF_INITIATOR_EXCHANGE_VALID \
    ( 1 << 30 ) /* Initiator received a valid RTToF response from a responder */
#define LR20XX_SYSTEM_IRQ_RTTOF_INITIATOR_TIMEOUT \
    ( 1 << 31 ) /* Initiator did not receive an RTToF response from a responder */

#define LR20XX_SYSTEM_IRQ_ALL_MASK                                                                                     \
    ( LR20XX_SYSTEM_IRQ_FIFO_RX | LR20XX_SYSTEM_IRQ_FIFO_TX | LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_REQUEST_VALID |        \
      LR20XX_SYSTEM_IRQ_TX_TIMESTAMP | LR20XX_SYSTEM_IRQ_RX_TIMESTAMP | LR20XX_SYSTEM_IRQ_PREAMBLE_DETECTED |          \
      LR20XX_SYSTEM_IRQ_SYNC_WORD_HEADER_VALID | LR20XX_SYSTEM_IRQ_CAD_DETECTED |                                      \
      LR20XX_SYSTEM_IRQ_LORA_RX_HEADER_TIMESTAMP | LR20XX_SYSTEM_IRQ_LORA_HEADER_ERROR |                               \
      LR20XX_SYSTEM_IRQ_POWER_SUPPLY_LOW | LR20XX_SYSTEM_IRQ_ERROR | LR20XX_SYSTEM_IRQ_CMD_ERROR |                     \
      LR20XX_SYSTEM_IRQ_RX_DONE | LR20XX_SYSTEM_IRQ_TX_DONE | LR20XX_SYSTEM_IRQ_CAD_DONE | LR20XX_SYSTEM_IRQ_TIMEOUT | \
      LR20XX_SYSTEM_IRQ_CRC_ERROR | LR20XX_SYSTEM_IRQ_LEN_ERROR | LR20XX_SYSTEM_IRQ_ADDR_ERROR |                       \
      LR20XX_SYSTEM_IRQ_LR_FHSS_INTRA_PKT_HOP | LR20XX_SYSTEM_IRQ_LR_FHSS_RDY_FOR_NEW_FREQ_TABLE |                     \
      LR20XX_SYSTEM_IRQ_LR_FHSS_RDY_FOR_NEW_PAYLOAD | LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_RESPONSE_DONE |                \
      LR20XX_SYSTEM_IRQ_RTTOF_RESPONDER_REQUEST_DISCARDED | LR20XX_SYSTEM_IRQ_RTTOF_INITIATOR_EXCHANGE_VALID |         \
      LR20XX_SYSTEM_IRQ_RTTOF_INITIATOR_TIMEOUT )

/* This is not present in the radio_drivers */
#define LR20XX_DIO_RF_SWITCH_WHEN_STANDBY ( 1 << 0 )
#define LR20XX_DIO_RF_SWITCH_WHEN_LF_RX ( 1 << 1 )
#define LR20XX_DIO_RF_SWITCH_WHEN_LF_TX ( 1 << 2 )
#define LR20XX_DIO_RF_SWITCH_WHEN_HF_RX ( 1 << 3 )
#define LR20XX_DIO_RF_SWITCH_WHEN_HF_TX ( 1 << 4 )

#define LR20XX_SYSTEM_HF_CLK_SCALING_32_MHZ 0x00   /* Division by 1 - 32 MHz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_16_MHZ 0x01   /* Division by 2 - 16 MHz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_8_MHZ 0x02    /* Division by 4 - 8 MHz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_4_MHZ 0x03    /* Division by 8 - 4 MHz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_2_MHZ 0x04    /* Division by 16 - 2 MHz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_1_MHZ 0x05    /* Division by 32 - 1 MHz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_500_KHZ 0x06  /* Division by 64 - 500 kHz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_250_KHZ 0x07  /* Division by 128 - 250 kHz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_125_KHZ 0x08  /* Division by 256 - 125 kHz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_62500_HZ 0x09 /* Division by 512 - 62500 Hz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_31250_HZ 0x0A /* Division by 1024 - 31250 Hz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_15625_HZ 0x0B /* Division by 2048 - 15625 Hz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_7813_HZ 0x0C  /* Division by 4096 - 7812.5 Hz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_3906_HZ 0x0D  /* Division by 8192 - 3906.25 Hz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_1953_HZ 0x0E  /* Division by 16384 - 1953.125 Hz */
#define LR20XX_SYSTEM_HF_CLK_SCALING_977_MHZ 0x0F  /* Division by 32768 - 976.5625 Hz */

#define LR20XX_SYSTEM_TCXO_CTRL_1_6V 0x00 /* Supply voltage = 1.6v */
#define LR20XX_SYSTEM_TCXO_CTRL_1_7V 0x01 /* Supply voltage = 1.7v */
#define LR20XX_SYSTEM_TCXO_CTRL_1_8V 0x02 /* Supply voltage = 1.8v */
#define LR20XX_SYSTEM_TCXO_CTRL_2_2V 0x03 /* Supply voltage = 2.2v */
#define LR20XX_SYSTEM_TCXO_CTRL_2_4V 0x04 /* Supply voltage = 2.4v */
#define LR20XX_SYSTEM_TCXO_CTRL_2_7V 0x05 /* Supply voltage = 2.7v */
#define LR20XX_SYSTEM_TCXO_CTRL_3_0V 0x06 /* Supply voltage = 3.0v */
#define LR20XX_SYSTEM_TCXO_CTRL_3_3V 0x07 /* Supply voltage = 3.3v */

#define LR20XX_SYSTEM_LFCLK_RC 0x00   /* Use internal RC 32kHz (Default) */
#define LR20XX_SYSTEM_LFCLK_XTAL 0x01 /* Use XTAL 32kHz */
#define LR20XX_SYSTEM_LFCLK_EXT 0x02  /* Use externally provided 32kHz signal on DIO11 */

#define LR20XX_REG_MODE_LDO 0x00  /* Do not switch on the DC-DC converter in any mode */
#define LR20XX_REG_MODE_DCDC 0x02 /* Automatically switch on the DC-DC converter when required */

#endif /* ZEPHYR_INCLUDE_DT_BINDINGS_USP_LR20XX_BINDINGS_DEF_H_*/
