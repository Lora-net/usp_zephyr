/*!
 * \file      flrp_api.c
 *
 * \brief     Test functions for FLRP API example
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2021. All rights reserved.
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
#include <stdint.h>   // C99 types
#include <stdbool.h>  // bool type
#include <stdarg.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "flrp_display.h"
#if DT_HAS_CHOSEN( zephyr_display )
#include "oled_display.h"
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

LOG_MODULE_REGISTER( flrp_display, LOG_LEVEL_INF );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

#define DISPLAY_MESSAGE_MAX_LEN 22

typedef struct flrp_display_str_s
{
    char                     text[DISPLAY_MESSAGE_MAX_LEN];
    flrp_display_font_size_t font_size;
} flrp_display_str_t;

typedef struct flrp_display_message_s
{
    uint8_t x;
    uint8_t y;
    bool    is_icon;
    union
    {
        flrp_display_str_t     str;
        flrp_display_icon_id_t icon_id;
    };
} flrp_display_message_t;

typedef struct flrp_display_icon_s
{
    uint8_t width;
    uint8_t height;
    const uint8_t ( *data )[];
} flrp_display_icon_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define DISPLAY_QUEUE_DEPTH 8
#define DISPLAY_THREAD_STACK 1024

#if DT_HAS_CHOSEN( zephyr_display )
static const flrp_display_icon_t icons[ICON_MAX] = {
    {
        // ICON_RADIO_NOT_JOINED,
        .width  = 16,
        .height = 1,
        .data   = ( const uint8_t ( * )[] ) &
                ( const uint8_t[1][16] ){
                    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
                },
    },
    {
        // ICON_RADIO_JOINED,
        .width  = 16,
        .height = 1,
        .data   = ( const uint8_t ( * )[] ) &
                ( const uint8_t[1][16] ){
                    { 0x00, 0x00, 0x3C, 0x42, 0xBD, 0x42, 0x00, 0x18, 0x18, 0x00, 0x42, 0xBD, 0x42, 0x3C, 0x00, 0x00 },
                },
    },
};
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

K_MSGQ_DEFINE( flrp_api_display_msgq, sizeof( flrp_display_message_t ), DISPLAY_QUEUE_DEPTH, 4 );
K_THREAD_STACK_DEFINE( flrp_api_display_thread_stack, DISPLAY_THREAD_STACK );
static struct k_thread flrp_api_display_thread;
static bool            flrp_api_display_thread_started = false;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

static void display_task( void* p1, void* p2, void* p3 );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void flrp_display_init( void )
{
    if( flrp_api_display_thread_started == false )
    {
        k_thread_create( &flrp_api_display_thread, flrp_api_display_thread_stack,
                         K_THREAD_STACK_SIZEOF( flrp_api_display_thread_stack ), display_task, NULL, NULL, NULL,
                         CONFIG_DISPLAY_THREAD_PRIORITY, 0, K_NO_WAIT );
        flrp_api_display_thread_started = true;
    }

    flrp_display_message( 0, 0, FLRP_DISPLAY_FONT_SIZE_SMALL, "FLRP                 " );
    flrp_display_message( 0, 1, FLRP_DISPLAY_FONT_SIZE_SMALL, "  ->                 " );

    flrp_display_message( 0, 4, FLRP_DISPLAY_FONT_SIZE_BIG, " Rx   : ....." );
    flrp_display_message( 0, 6, FLRP_DISPLAY_FONT_SIZE_BIG, " Tx   : ....." );
}

void flrp_display_message( uint8_t x, uint8_t y, flrp_display_font_size_t font_size, const char* fmt, ... )
{
    flrp_display_message_t message = {
        .x             = x,
        .y             = y,
        .is_icon       = false,
        .str.font_size = font_size,
    };

    va_list args;
    va_start( args, fmt );
    vsnprintf( message.str.text, sizeof( message.str.text ), fmt, args );
    va_end( args );
    if( k_msgq_put( &flrp_api_display_msgq, &message, K_NO_WAIT ) != 0 )
    {
        LOG_WRN( "Display queue full, dropping message: %s", message.str.text );
    }
}

void flrp_display_icon( uint8_t x, uint8_t y, flrp_display_icon_id_t icon_id )
{
    flrp_display_message_t message = {
        .x       = x,
        .y       = y,
        .is_icon = true,
        .icon_id = icon_id,
    };

    if( k_msgq_put( &flrp_api_display_msgq, &message, K_NO_WAIT ) != 0 )
    {
        LOG_WRN( "Display queue full, dropping icon %d", icon_id );
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static void display_task( void* p1, void* p2, void* p3 )
{
    ARG_UNUSED( p1 );
    ARG_UNUSED( p2 );
    ARG_UNUSED( p3 );

    flrp_display_message_t message;

#if DT_HAS_CHOSEN( zephyr_display )
    oled_display_init( );
    oled_cls( );
#endif

    while( true )
    {
        k_msgq_get( &flrp_api_display_msgq, &message, K_FOREVER );
#if DT_HAS_CHOSEN( zephyr_display )
        if( message.is_icon == false )
        {
            oled_show_str( message.x, message.y, message.str.text, ( uint8_t ) message.str.font_size );
        }
        else
        {
            if( message.icon_id < ICON_MAX )
            {
                oled_write_bitmap( message.x, message.y, ( uint8_t* ) icons[message.icon_id].data,
                                   icons[message.icon_id].width, icons[message.icon_id].height );
            }
        }
#endif
        if( message.is_icon == false )
        {
            LOG_INF( "Display %s", message.str.text );
        }
        else
        {
            LOG_INF( "Display icon %d", message.icon_id );
        }
    }
}

/* --- EOF ------------------------------------------------------------------ */