/*!
 * \file      flrp_menu.c
 *
 * \brief     Menu functions for FLRP API example
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
#include <stdint.h>   // C99 types
#include <stdbool.h>  // bool type
#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#if defined( CONFIG_PM_POLICY_CUSTOM )
#include <zephyr/pm/policy.h>
#include <zephyr/pm/state.h>
#include <zephyr/pm/device.h>
#endif

#include "flrp_defs.h"
#include "flrp_api.h"
#include "flrp_api_menu.h"
#include "main.h"
#include "flrp_display.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

LOG_MODULE_REGISTER( flrp_api_menu, LOG_LEVEL_INF );

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

#define FLRP_API_MENU_KEY_BYTE_SIZE 16

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

static bool    flrp_api_menu_listening = false;
static bool    is_device_eui_set       = false;  // Indicates that device EUI has not been set yet
static uint8_t flrp_api_menu_device_eui[SMTC_FLRP_EUI_LENGTH]        = { 0x00 };
static uint8_t flrp_api_menu_target_device_eui[SMTC_FLRP_EUI_LENGTH] = { 0x00 };
static uint8_t flrp_api_menu_key[FLRP_API_MENU_KEY_BYTE_SIZE]        = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static bool    flrp_api_menu_crypto_enabled                          = true;
static bool    flrp_api_menu_is_low_frequency                        = true;
/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

static bool flrp_api_menu_get_array_from_hex_string( const char* hex_string, uint8_t* array, size_t array_size, bool accept_single_byte );
static void flrp_api_menu_display_device( void );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

void flrp_api_menu_apply_startup_test_defaults( void )
{
    static const uint8_t default_device_eui[SMTC_FLRP_EUI_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
    static const uint8_t default_target_eui[SMTC_FLRP_EUI_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    if( is_device_eui_set == true )
    {
        return;
    }
    memcpy( ( void* ) flrp_api_menu_device_eui, default_device_eui, SMTC_FLRP_EUI_LENGTH );
    memcpy( ( void* ) flrp_api_menu_target_device_eui, default_target_eui, SMTC_FLRP_EUI_LENGTH );
    flrp_api_menu_crypto_enabled = false;
    flrp_api_menu_listening      = true;
    is_device_eui_set = flrp_api_init( flrp_api_menu_device_eui, flrp_api_menu_key, flrp_api_menu_crypto_enabled,
                                       flrp_api_menu_is_low_frequency, flrp_api_menu_listening );
    flrp_api_menu_display_device( );
}

bool flrp_api_menu_is_set( void )
{
    return is_device_eui_set;
}

uint8_t* flrp_api_menu_get_device_eui( void )
{
    return flrp_api_menu_device_eui;
}

uint8_t* flrp_api_menu_get_target_device_eui( void )
{
    return flrp_api_menu_target_device_eui;
}

uint8_t* flrp_api_menu_get_key( void )
{
    return flrp_api_menu_key;
}

bool flrp_api_menu_init( uint8_t* device_eui, uint8_t* key, bool crypto_enabled, bool is_low_frequency, bool listening )
{
    memcpy( ( void* ) flrp_api_menu_device_eui, ( void* ) device_eui, SMTC_FLRP_EUI_LENGTH );
    memcpy( ( void* ) flrp_api_menu_key, ( void* ) key, FLRP_API_MENU_KEY_BYTE_SIZE );
    memset( ( void* ) flrp_api_menu_target_device_eui, 0x00, SMTC_FLRP_EUI_LENGTH );
    flrp_api_menu_listening        = listening;
    flrp_api_menu_crypto_enabled   = crypto_enabled;
    flrp_api_menu_is_low_frequency = is_low_frequency;
    is_device_eui_set = flrp_api_init( flrp_api_menu_device_eui, flrp_api_menu_key, crypto_enabled, is_low_frequency,
                                       flrp_api_menu_listening );
    flrp_api_menu_display_device( );

    return is_device_eui_set;
}

bool flrp_api_menu_initiate_transfer( bool transmit, uint8_t* target_device_eui )
{
    bool ret = false;

    if( is_device_eui_set == true )
    {
        memcpy( ( void* ) flrp_api_menu_target_device_eui, ( void* ) target_device_eui, SMTC_FLRP_EUI_LENGTH );
        flrp_api_menu_display_device( );
        if( transmit == true )
        {
            ret = flrp_api_initiate_tx( target_device_eui );
        }
        else
        {
            ret = flrp_api_initiate_rx( target_device_eui );
        }
    }

    return ret;
}

bool flrp_api_menu_start_listening( void )
{
    bool ret = false;

    if( is_device_eui_set == true )
    {
        ret = flrp_api_start_listening( );
        if( ret == true )
        {
            flrp_api_menu_listening = true;
        }
    }

    return ret;
}

bool flrp_api_menu_stop_listening( void )
{
    bool ret = false;

    if( is_device_eui_set == true )
    {
        ret = flrp_api_stop_listening( );
        if( ret == true )
        {
            flrp_api_menu_listening = false;
        }
    }

    return ret;
}
/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

/**
 * @brief Shell command: Set device EUI and start flrp
 */
static int cmd_set_device_eui( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( is_device_eui_set == true )
    {
        shell_print( sh, "Device EUI has already been set to 0x%02X%02X%02X%02X%02X%02X%02X%02X",
                     flrp_api_menu_device_eui[0], flrp_api_menu_device_eui[1], flrp_api_menu_device_eui[2],
                     flrp_api_menu_device_eui[3], flrp_api_menu_device_eui[4], flrp_api_menu_device_eui[5],
                     flrp_api_menu_device_eui[6], flrp_api_menu_device_eui[7] );
    }
    else if( argc != 3 )
    {
        shell_error( sh,
                     "Usage: device_eui <%d-byte device_eui in hexadecimal format> <%d-byte target_device_eui in "
                     "hexadecimal format>",
                     SMTC_FLRP_EUI_LENGTH, SMTC_FLRP_EUI_LENGTH );
        ret = -EINVAL;
    }
    else
    {
        if( flrp_api_menu_get_array_from_hex_string( argv[1], flrp_api_menu_device_eui, SMTC_FLRP_EUI_LENGTH, true ) ==
            false )
        {
            shell_error( sh,
                         "Invalid device EUI format. Please ensure the EUI is in hexadecimal format and has the "
                         "correct length." );
            ret = -EINVAL;
            memset( ( void* ) flrp_api_menu_device_eui, 0x00, SMTC_FLRP_EUI_LENGTH );
        }
        else
        {
            if( flrp_api_menu_get_array_from_hex_string( argv[2], flrp_api_menu_target_device_eui,
                                                         SMTC_FLRP_EUI_LENGTH, true ) == false )
            {
                shell_error( sh,
                             "Invalid target device EUI format. Please ensure the EUI is in hexadecimal format and has "
                             "the correct length." );
                ret = -EINVAL;
                memset( ( void* ) flrp_api_menu_device_eui, 0x00, SMTC_FLRP_EUI_LENGTH );
                memset( ( void* ) flrp_api_menu_target_device_eui, 0x00, SMTC_FLRP_EUI_LENGTH );
            }
            else
            {
                shell_print( sh, "Device ID set to 0x%02X%02X%02X%02X%02X%02X%02X%02X", flrp_api_menu_device_eui[0],
                             flrp_api_menu_device_eui[1], flrp_api_menu_device_eui[2], flrp_api_menu_device_eui[3],
                             flrp_api_menu_device_eui[4], flrp_api_menu_device_eui[5], flrp_api_menu_device_eui[6],
                             flrp_api_menu_device_eui[7] );
                shell_print( sh, "Target Device ID set to 0x%02X%02X%02X%02X%02X%02X%02X%02X",
                             flrp_api_menu_target_device_eui[0], flrp_api_menu_target_device_eui[1],
                             flrp_api_menu_target_device_eui[2], flrp_api_menu_target_device_eui[3],
                             flrp_api_menu_target_device_eui[4], flrp_api_menu_target_device_eui[5],
                             flrp_api_menu_target_device_eui[6], flrp_api_menu_target_device_eui[7] );
                is_device_eui_set       = true;
                flrp_api_menu_listening = true;
                flrp_api_init( flrp_api_menu_device_eui, flrp_api_menu_key, flrp_api_menu_crypto_enabled,
                               flrp_api_menu_is_low_frequency, flrp_api_menu_listening );
                flrp_api_menu_display_device( );
            }
        }
    }

    return ret;
}

/**
 * @brief Shell command: Set target device eui
 */
static int cmd_set_target_device_eui( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( argc != 2 )
    {
        shell_error( sh, "Usage: target_device_eui <%d-byte target_device_eui in hexadecimal format>",
                     SMTC_FLRP_EUI_LENGTH );
        ret = -EINVAL;
    }
    else
    {
        if( flrp_api_menu_get_array_from_hex_string( argv[1], flrp_api_menu_target_device_eui, SMTC_FLRP_EUI_LENGTH, true ) ==
            false )
        {
            shell_error( sh,
                         "Invalid target device EUI format. Please ensure the EUI is in hexadecimal format and has the "
                         "correct length." );
            ret = -EINVAL;
            memset( ( void* ) flrp_api_menu_target_device_eui, 0x00, SMTC_FLRP_EUI_LENGTH );
        }
        else
        {
            shell_print( sh, "Target Device EUI set to 0x%02X%02X%02X%02X%02X%02X%02X%02X",
                         flrp_api_menu_target_device_eui[0], flrp_api_menu_target_device_eui[1],
                         flrp_api_menu_target_device_eui[2], flrp_api_menu_target_device_eui[3],
                         flrp_api_menu_target_device_eui[4], flrp_api_menu_target_device_eui[5],
                         flrp_api_menu_target_device_eui[6], flrp_api_menu_target_device_eui[7] );
        }
        flrp_api_menu_display_device( );
    }

    return ret;
}

/**
 * @brief Shell command: Launch transmission
 */
static int cmd_launch_tx( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( argc != 1 )
    {
        shell_error( sh, "Usage: launch_tx" );
        ret = -EINVAL;
    }
    else
    {
        if( is_device_eui_set == false )
        {
            shell_error( sh, "Device EUI is not set yet. Please set it using the 'device_eui' command." );
            ret = -EINVAL;
        }
        else if( flrp_api_initiate_tx( flrp_api_menu_target_device_eui ) != true )
        {
            shell_error( sh, "Failed to initiate transmission" );
            ret = -EFAULT;
        }
    }

    return ret;
}

/**
 * @brief Shell command: Launch reception
 */
static int cmd_launch_rx( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( argc != 1 )
    {
        shell_error( sh, "Usage: launch_rx" );
        ret = -EINVAL;
    }
    else
    {
        if( is_device_eui_set == false )
        {
            shell_error( sh, "Device EUI is not set yet. Please set it using the 'device_eui' command." );
            ret = -EINVAL;
        }
        else if( flrp_api_initiate_rx( flrp_api_menu_target_device_eui ) != true )
        {
            shell_error( sh, "Failed to initiate reception" );
            ret = -EFAULT;
        }
    }

    return ret;
}

/**
 * @brief Shell command: Set the encryption key
 */
static int cmd_set_key( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( argc != 2 )
    {
        shell_error( sh, "Usage: set_key <%d-byte key in hexadecimal format>", FLRP_API_MENU_KEY_BYTE_SIZE );
        ret = -EINVAL;
    }
    else
    {
        if( flrp_api_menu_get_array_from_hex_string( argv[1], flrp_api_menu_key, FLRP_API_MENU_KEY_BYTE_SIZE, true ) == true )
        {
            if( flrp_api_set_key( flrp_api_menu_key ) == true )
            {
                shell_print( sh, "Key set successfully" );
            }
            else
            {
                shell_error( sh, "Failed to set key" );
                ret = -EFAULT;
            }
        }
        else
        {
            shell_error(
                sh, "Invalid key format. Please ensure the key is in hexadecimal format and has the correct length." );
            ret = -EINVAL;
        }
    }

    return ret;
}

/**
 * @brief Shell command: Display the current configuration
 */
static int cmd_display_config( const struct shell* sh, size_t argc, char** argv )
{
    int     ret              = 0;
    uint8_t burst_target_per = 0;

    if( is_device_eui_set == false )
    {
        shell_error( sh, "Device EUI is not set yet. Please set it using the 'device_eui' command." );
        ret = -EINVAL;
    }

    if( argc != 1 )
    {
        shell_error( sh, "Usage: display_config" );
        ret = -EINVAL;
    }
    else
    {
        flrp_api_get_burst_target_per( &burst_target_per );
        shell_print( sh, "Device EUI: 0x%02X%02X%02X%02X%02X%02X%02X%02X", flrp_api_menu_device_eui[0],
                     flrp_api_menu_device_eui[1], flrp_api_menu_device_eui[2], flrp_api_menu_device_eui[3],
                     flrp_api_menu_device_eui[4], flrp_api_menu_device_eui[5], flrp_api_menu_device_eui[6],
                     flrp_api_menu_device_eui[7] );
        shell_print( sh, "Target Device EUI: 0x%02X%02X%02X%02X%02X%02X%02X%02X", flrp_api_menu_target_device_eui[0],
                     flrp_api_menu_target_device_eui[1], flrp_api_menu_target_device_eui[2],
                     flrp_api_menu_target_device_eui[3], flrp_api_menu_target_device_eui[4],
                     flrp_api_menu_target_device_eui[5], flrp_api_menu_target_device_eui[6],
                     flrp_api_menu_target_device_eui[7] );
        shell_print( sh, "Burst Target PER: %d%% %s", burst_target_per,
                     ( flrp_api_menu_listening == true ) ? "(listening enabled)" : "(listening disabled)" );
        shell_print( sh, "Low Frequency: %s", ( flrp_api_menu_is_low_frequency == true ) ? "Enabled" : "Disabled" );
        if( flrp_api_menu_crypto_enabled == true )
        {
            shell_print( sh, "Encryption: Enabled" );
            shell_print( sh, "Encryption Key: 0x%02X%02X%02X%02X%02X%02X%02X%02X", flrp_api_menu_key[0],
                         flrp_api_menu_key[1], flrp_api_menu_key[2], flrp_api_menu_key[3], flrp_api_menu_key[4],
                         flrp_api_menu_key[5], flrp_api_menu_key[6], flrp_api_menu_key[7] );
            shell_print( sh, "                  %02X%02X%02X%02X%02X%02X%02X%02X", flrp_api_menu_key[8],
                         flrp_api_menu_key[9], flrp_api_menu_key[10], flrp_api_menu_key[11], flrp_api_menu_key[12],
                         flrp_api_menu_key[13], flrp_api_menu_key[14], flrp_api_menu_key[15] );
        }
        else
        {
            shell_print( sh, "Encryption: Disabled" );
        }
    }

    return ret;
}

/**
 * @brief Shell command: Set per
 */
static int cmd_set_per( const struct shell* sh, size_t argc, char** argv )
{
    int   ret = 0;
    char* end = NULL;

    if( is_device_eui_set == false )
    {
        shell_error( sh, "Device EUI is not set yet. Please set it using the 'device_eui' command." );
        ret = -EINVAL;
    }
    else if( argc != 2 )
    {
        shell_error( sh, "Usage: set_per <burst_target_per>" );
        ret = -EINVAL;
    }
    else
    {
        uint8_t burst_target_per = ( uint8_t ) strtoul( argv[1], &end, 0 );
        if( *end != '\0' )
        {
            shell_error( sh, "Invalid Burst Target PER '%s'. Please provide a valid number (0 - 100).", argv[1] );
            ret = -EINVAL;
        }
        else
        {
            if( burst_target_per > 100 )
            {
                shell_error( sh, "Invalid Burst Target PER '%s'. Please provide a valid number (0 - 100).", argv[1] );
                ret = -EINVAL;
            }
            else if( flrp_api_set_burst_target_per( burst_target_per, flrp_api_menu_listening ) == true )
            {
                shell_print( sh, "Burst Target PER set to %d", burst_target_per );
            }
            else
            {
                shell_error( sh, "Failed to set Burst Target PER" );
                ret = -EFAULT;
            }
        }
    }

    return ret;
}

/**
 * @brief Shell command: Enable or disable encryption
 */
static int cmd_set_crypto_enabled( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( is_device_eui_set == true )
    {
        shell_error( sh, "Device EUI is already set. Crypto settings cannot be changed." );
        ret = -EINVAL;
    }
    else if( argc != 2 )
    {
        shell_error( sh, "Usage: set_crypto_enabled <0|1>" );
        ret = -EINVAL;
    }
    else
    {
        if( ( strcmp( argv[1], "0" ) == 0 ) || ( strcmp( argv[1], "1" ) == 0 ) )
        {
            flrp_api_menu_crypto_enabled = ( strcmp( argv[1], "1" ) == 0 );
        }
        else
        {
            shell_error( sh, "Invalid argument '%s'. Please provide 0 to disable encryption or 1 to enable it.",
                         argv[1] );
            ret = -EINVAL;
        }
    }

    return ret;
}

/**
 * @brief Shell command: Set low frequency mode
 */
static int cmd_set_low_frequency( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( is_device_eui_set == true )
    {
        shell_error( sh, "Device EUI is already set. Low frequency mode cannot be changed." );
        ret = -EINVAL;
    }
    else if( argc != 2 )
    {
        shell_error( sh, "Usage: set_low_frequency <0|1>" );
        ret = -EINVAL;
    }
    else
    {
        if( ( strcmp( argv[1], "0" ) == 0 ) || ( strcmp( argv[1], "1" ) == 0 ) )
        {
            flrp_api_menu_is_low_frequency = ( strcmp( argv[1], "1" ) == 0 );
        }
        else
        {
            shell_error( sh, "Invalid argument '%s'. Please provide 0 to disable low frequency mode or 1 to enable it.",
                         argv[1] );
            ret = -EINVAL;
        }
    }

    return ret;
}

/**
 * @brief Shell command: Start periodic listening with the current configuration
 */
static int cmd_start_listening( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( argc != 1 )
    {
        shell_error( sh, "Usage: start_listening" );
        ret = -EINVAL;
    }
    else
    {
        if( flrp_api_start_listening( ) == true )
        {
            flrp_api_menu_listening = true;
            shell_print( sh, "Started periodic listening" );
        }
        else
        {
            shell_error( sh, "Failed to start periodic listening" );
            ret = -EFAULT;
        }
    }

    return ret;
}

/**
 * @brief Shell command: Stop periodic listening with the current configuration
 */
static int cmd_stop_listening( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( argc != 1 )
    {
        shell_error( sh, "Usage: stop_listening" );
        ret = -EINVAL;
    }
    else
    {
        if( flrp_api_stop_listening( ) == true )
        {
            flrp_api_menu_listening = false;
            shell_print( sh, "Stopped periodic listening" );
        }
        else
        {
            shell_error( sh, "Failed to stop periodic listening" );
            ret = -EFAULT;
        }
    }

    return ret;
}

/**
 * @brief Shell command: Display the RX statistics
 */
static int cmd_display_rx_stats( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( argc != 1 )
    {
        shell_error( sh, "Usage: display_rx_stats" );
        ret = -EINVAL;
    }
    else
    {
        flrp_api_display_rx_stats( );
    }

    return ret;
}

#if defined( CONFIG_USP_LORA_BASICS_MODEM )
/**
 * @brief Shell command: Send a keepalive message
 */
static int cmd_send_keepalive( const struct shell* sh, size_t argc, char** argv )
{
    int ret = 0;

    if( argc != 1 )
    {
        shell_error( sh, "Usage: uplink" );
        ret = -EINVAL;
    }
    else
    {
        main_loop_set_event( FLRP_API_EVENT_KEEPALIVE );
        shell_print( sh, "Request keepalive sent" );
    }

    return ret;
}
SHELL_CMD_REGISTER( uplink, NULL, "Request to send LoRaWAN keepalive", cmd_send_keepalive );
#endif

// FLRP  shell subcommands
SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_flrp, SHELL_CMD( device_eui, NULL, "Set the device eui and target device eui", cmd_set_device_eui ),
    SHELL_CMD( target_device_eui, NULL, "Set the target device eui", cmd_set_target_device_eui ),
    SHELL_CMD( launch_tx, NULL, "Launch transmission", cmd_launch_tx ),
    SHELL_CMD( launch_rx, NULL, "Launch reception", cmd_launch_rx ),
    SHELL_CMD( set_key, NULL, "Set the encryption key", cmd_set_key ),
    SHELL_CMD( display_config, NULL, "Display the current configuration", cmd_display_config ),
    SHELL_CMD( display_rx_stats, NULL, "Display the RX statistics", cmd_display_rx_stats ),
    SHELL_CMD( set_per, NULL, "Set the burst target PER", cmd_set_per ),
    SHELL_CMD( set_crypto_enabled, NULL, "Enable or disable encryption", cmd_set_crypto_enabled ),
    SHELL_CMD( set_low_frequency, NULL, "Enable or disable low frequency mode", cmd_set_low_frequency ),
    SHELL_CMD( start_listening, NULL, "Start periodic listening", cmd_start_listening ),
    SHELL_CMD( stop_listening, NULL, "Stop periodic listening", cmd_stop_listening ), SHELL_SUBCMD_SET_END );

// Main shell commands
SHELL_CMD_REGISTER( flrp, &sub_flrp, "FLRP commands", NULL );

static bool flrp_api_menu_get_array_from_hex_string( const char* hex_string, uint8_t* array, size_t array_size, bool accept_single_byte )
{
    bool ret = true;

    if( ( strlen( hex_string ) == array_size * 2 ) || ( accept_single_byte == true && strlen( hex_string ) == 2 ) )
    {
        for( size_t i = 0; i < array_size; i++ )
        {
            char    byte_str[3];
            byte_str[0] = ( strlen( hex_string ) == 2 )?hex_string[0]:hex_string[2 * i];
            byte_str[1] = ( strlen( hex_string ) == 2 )?hex_string[1]:hex_string[2 * i + 1];
            byte_str[2] = '\0';
            char*   end         = NULL;
            uint8_t byte        = ( uint8_t ) strtoul( byte_str, &end, 16 );
            if( *end != '\0' )
            {
                ret = false;
                break;
            }
            else
            {
                array[i] = byte;
            }
        }
    }
    else
    {
        ret = false;
    }

    return ret;
}

static void flrp_api_menu_display_device( void )
{
    flrp_display_message( 0, 0, FLRP_DISPLAY_FONT_SIZE_SMALL, "FLRP %02X%02X%02X%02X%02X%02X%02X%02X",
                          flrp_api_menu_device_eui[0], flrp_api_menu_device_eui[1], flrp_api_menu_device_eui[2],
                          flrp_api_menu_device_eui[3], flrp_api_menu_device_eui[4], flrp_api_menu_device_eui[5],
                          flrp_api_menu_device_eui[6], flrp_api_menu_device_eui[7] );
    flrp_display_message( 0, 1, FLRP_DISPLAY_FONT_SIZE_SMALL, "  -> %02X%02X%02X%02X%02X%02X%02X%02X",
                          flrp_api_menu_target_device_eui[0], flrp_api_menu_target_device_eui[1],
                          flrp_api_menu_target_device_eui[2], flrp_api_menu_target_device_eui[3],
                          flrp_api_menu_target_device_eui[4], flrp_api_menu_target_device_eui[5],
                          flrp_api_menu_target_device_eui[6], flrp_api_menu_target_device_eui[7] );
}

/*
 * Custom PM policy to prevent deep sleep while keeping UART operational.
 * This allows LPTIM to work (requires CONFIG_PM=y) but prevents the system
 * from entering STOP modes that would suspend the UART.
 * A proper implementation would involve to wake up on UART rx pin activity and switch between UART and GPIO pinctrl
 * states.
 */
#ifdef CONFIG_PM_POLICY_CUSTOM
const struct pm_state_info* pm_policy_next_state( uint8_t cpu, int32_t ticks )
{
    ARG_UNUSED( cpu );
    ARG_UNUSED( ticks );

    /* Return NULL to prevent any sleep - keep CPU running for UART responsiveness.
     * This is the safest approach for the modem bridge application.
     * The LPTIM will still provide accurate timekeeping.
     */
    return NULL;
}
#endif

/* --- EOF ------------------------------------------------------------------ */