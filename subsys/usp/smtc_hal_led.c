/**
 * @file      smtc_hal_led.c
 *
 * @brief     LED Hardware Abstraction Layer implementation for Linux
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

#include "smtc_hal_led.h"
#include "smtc_sw_platform_helper.h"

static smtc_led_pin_e hal_led_to_pf_led( hal_led_id_t hal_led )
{
    switch( hal_led )
    {
    case HAL_LED_RX:
        return SMTC_PF_LED_RX;
    case HAL_LED_TX:
        return SMTC_PF_LED_TX;
    case HAL_LED_SCAN:
        return SMTC_PF_LED_SCAN;
    case HAL_LED_COUNT:
        return SMTC_PF_LED_MAX;
    }
    return SMTC_PF_LED_MAX;
}

void hal_led_init( void )
{
    init_leds( );
}

void hal_led_set( hal_led_id_t led, bool state )
{
    set_led( hal_led_to_pf_led( led ), state );
}

void hal_led_toggle( hal_led_id_t led )
{
    // smtc_sw_platform_helper.h does not provide this functionality (issue 140)
    ( void ) led;
}

void hal_led_toggle_tx_rx( void )
{
    toggle_led( );
}
