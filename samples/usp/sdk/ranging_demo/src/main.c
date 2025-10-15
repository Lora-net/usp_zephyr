/**
 * @file      main.c
 *
 * @brief     Application main
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

#include <stdint.h>   // C99 types
#include <stdbool.h>  // bool type

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>


#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>

#include <apps_configuration.h>
#include <app_ranging_hopping.h>
#include <app_ranging_result_output.h>
#include <main_ranging_demo.h>

#define SMTC_HAL_DBG_TRACE_C
#include <smtc_hal_dbg_trace.h>
#include <smtc_zephyr_usp_api.h>
#include <smtc_sw_platform_helper.h>

#include <zephyr/lorawan_lbm/lorawan_hal_init.h>
#include <smtc_modem_hal.h>

#if DT_HAS_CHOSEN( zephyr_display )
#include "oled_display.h"
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

LOG_MODULE_REGISTER(usp, LOG_LEVEL_INF);

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/**
 * @brief Watchdog counter reload value during sleep (The period must be lower than MCU watchdog period (here 32s))
 */
#define WATCHDOG_RELOAD_PERIOD_MS 20000


/**
 * @brief Blue user button or BUTTON 1 on nrf52840-dk
 */

#define USER_BUTTON_NODE DT_ALIAS(smtc_user_button)
#if !DT_NODE_HAS_STATUS(USER_BUTTON_NODE, okay)
#error "Unsupported board: smtc-user-button devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(USER_BUTTON_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */
static volatile bool user_button_is_press = false; // Flag for button status

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 * Defines application-level variables for manager/subordinate mode,
 * periodic transmission configuration, and payload buffer.
 */

 // Set is_manager based on compile-time macro (see main_ranging_demo.h)
#if defined( RANGING_DEVICE_MODE ) && ( RANGING_DEVICE_MODE == RANGING_DEVICE_MODE_MANAGER )
static const bool is_manager = true;
#elif defined( RANGING_DEVICE_MODE ) && ( RANGING_DEVICE_MODE == RANGING_DEVICE_MODE_SUBORDINATE )
static const bool is_manager = false;
#else
#error Application must define RANGING_DEVICE_MODE
#endif

static smtc_rac_context_t* periodic_tx_config      = NULL;   // Context for periodic uplink
static uint8_t              periodic_tx_payload[33] = { 0 };  // Payload buffer for periodic uplink
static uint8_t              periodic_tx_handle;               // Radio access handle for periodic uplink

char* bw_str[] = { "007K", "010K", "015K", "020K", "031K", "041K",   "062K",  "125K",
                   "200K", "250K", "400K", "500K", "800K", "1000_K", "1600_K" };

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 * Prototypes for internal helper functions.
 */


/**
 * @brief User callback for button EXTI
 *
 * @param context Define by the user at the init
 */
static void user_button_callback(const void *context);

/**
 * @brief Configure User Button
 *
 */
static int configure_user_button(void);

static void periodic_tx_handle_callback( rp_status_t status );
static void periodic_tx_handle_start( void );
static void periodic_tx_handle_config( void );

static void results_callback( smtc_rac_radio_lora_params_t* radio_lora_params,
                              ranging_params_settings_t*    ranging_params_settings,
                              ranging_global_result_t* ranging_global_results, const char* region );

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/* A binary semaphore to notify the main LBM loop */
K_SEM_DEFINE(periodical_uplink_event_sem, 0, 1);

/* for zephyr compat*/
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    printk("%s", __func__);
    user_button_callback(dev);
}


int main(void)
{
    if(configure_user_button() != 0) {
        LOG_ERR("Issue when configuring user button, aborting\n");
        return 1;
    }
#if DT_HAS_CHOSEN( zephyr_display )
    oled_display_init( );
    oled_cls();
#endif

    SMTC_HAL_TRACE_INFO( "===== ranging and frequency hopping example =====\r\n" );

    SMTC_SW_PLATFORM_INIT( );
    SMTC_SW_PLATFORM_VOID( smtc_rac_init( ) );

    init_leds( );
    set_led( SMTC_PF_LED_SCAN, true );
    if( is_manager == true )
    {
#if DT_HAS_CHOSEN( zephyr_display )
        oled_show_str( 4 * 30, 0, "M", 1 );
#endif
        set_led( SMTC_PF_LED_TX, true );
        set_led( SMTC_PF_LED_RX, false );
        SMTC_HAL_TRACE_INFO( "Running in ranging manager mode\n" );
        app_radio_ranging_params_init( is_manager, RAC_HIGH_PRIORITY );
        app_radio_ranging_set_user_callback( results_callback );
#if defined( CONTINUOUS_RANGING ) && ( CONTINUOUS_RANGING == true )
        start_ranging_exchange( 0, is_manager );
#endif
    }
    else
    {
#if DT_HAS_CHOSEN( zephyr_display )
        oled_show_str( 4 * 30, 0, "S", 1 );
        oled_show_str( 0, 2, "   Joining...", 2 );
#endif
        set_led( SMTC_PF_LED_TX, false );
        set_led( SMTC_PF_LED_RX, true );
        SMTC_HAL_TRACE_INFO( "Running in ranging subordinate mode\n" );
        app_radio_ranging_params_init( is_manager, RAC_HIGH_PRIORITY );
        app_radio_ranging_set_user_callback( results_callback );
        start_ranging_exchange( 0, is_manager );
    }
    // If periodic uplink is enabled, configure and start periodic transmissions
    if( PERIODIC_UPLINK_ENABLED == true )
    {
        periodic_tx_handle = SMTC_SW_PLATFORM(smtc_rac_open_radio( RAC_VERY_HIGH_PRIORITY ));
        periodic_tx_config = smtc_rac_get_context( periodic_tx_handle );
        periodic_tx_config->scheduler_config.callback_post_radio_transaction = periodic_tx_handle_callback;
        periodic_tx_handle_config( );
        periodic_tx_handle_start( );
    }

    // Main application loop
    while (true) {
        // Check button
        if (user_button_is_press == true)
        {
            user_button_is_press = false;

            // Code to be added on button event
            start_ranging_exchange( 0, is_manager );
        }

#if !defined(CONFIG_USP_MAIN_THREAD)
        smtc_rac_run_engine();
        if (smtc_rac_is_irq_flag_pending()) {
            continue;
        }
        // Allows waking up on radio event or push-button press
        struct k_sem *sems[] = { smtc_modem_hal_get_event_sem(),  &periodical_uplink_event_sem};
        (void)wait_on_sems(sems, 2, K_MSEC( WATCHDOG_RELOAD_PERIOD_MS));
#else
        if (user_button_is_press == false) {
            k_sem_take(&periodical_uplink_event_sem, K_MSEC(WATCHDOG_RELOAD_PERIOD_MS));
        }
#endif
    }
    return 0;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

static int configure_user_button(void)
{
    int ret = 0;
    if (!gpio_is_ready_dt(&button)) {
        printk("Error: button device %s is not ready\n", button.port->name);
        return 1;
    }

    ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (ret != 0) {
        printk("Error %d: failed to configure %s pin %d\n",
               ret, button.port->name, button.pin);
        return 1;
    }

    ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret != 0) {
        printk("Error %d: failed to configure interrupt on %s pin %d\n",
               ret, button.port->name, button.pin);
        return 1;
    }

    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    return 0;
}

static void user_button_callback(const void* context )
{
    LOG_INF( "Button pushed\n" );

    ( void ) context;  // Not used in the example - avoid warning

    static uint32_t last_press_timestamp_ms = 0;

    // Debounce the button press, avoid multiple triggers
    if( ( int32_t ) ( smtc_modem_hal_get_time_in_ms( ) - last_press_timestamp_ms ) > 500 )
    {
        last_press_timestamp_ms = smtc_modem_hal_get_time_in_ms( );
        user_button_is_press    = true;
    }
    // Wake up the main thread
    k_sem_give(&periodical_uplink_event_sem);
}

/**
 * @brief Callback for periodic uplink transmission.
 *
 * @param status Not used in this example.
 */
static void periodic_tx_handle_callback( rp_status_t status )
{
    UNUSED( status );
    periodic_tx_handle_start( );
}


/**
 * @brief Configures the periodic uplink transmission parameters.
 *
 * Sets up the radio parameters, payload, and scheduling for periodic transmissions.
 */
static void periodic_tx_handle_config( void )
{
    periodic_tx_config->modulation_type                                 = SMTC_RAC_MODULATION_LORA;
    periodic_tx_config->radio_params.lora.frequency_in_hz               = 868000000;
    periodic_tx_config->radio_params.lora.tx_power_in_dbm               = 14;
    periodic_tx_config->radio_params.lora.preamble_len_in_symb          = 8;
    periodic_tx_config->radio_params.lora.header_type                   = RAL_LORA_PKT_EXPLICIT;
    periodic_tx_config->radio_params.lora.invert_iq_is_on               = 0;
    periodic_tx_config->radio_params.lora.crc_is_on                     = 1;
    periodic_tx_config->radio_params.lora.sync_word                     = LORA_PUBLIC_NETWORK_SYNCWORD;
    periodic_tx_config->radio_params.lora.sf                            = RAL_LORA_SF12;
    periodic_tx_config->radio_params.lora.bw                            = RAL_LORA_BW_125_KHZ;
    periodic_tx_config->radio_params.lora.cr                            = RAL_LORA_CR_4_5;
    periodic_tx_config->radio_params.lora.is_tx                         = true;
    periodic_tx_config->smtc_rac_data_buffer_setup.tx_payload_buffer    = periodic_tx_payload;
    periodic_tx_config->radio_params.lora.tx_size                       = sizeof( periodic_tx_payload );
    periodic_tx_config->scheduler_config.scheduling                     = SMTC_RAC_ASAP_TRANSACTION;
    periodic_tx_config->scheduler_config.start_time_ms                  = smtc_modem_hal_get_time_in_ms( );
    periodic_tx_config->scheduler_config.callback_pre_radio_transaction = NULL;
}

/**
 * @brief Start the periodic uplink transmission.
 *
 * Schedules the next periodic uplink and sends the current payload.
 */
static void periodic_tx_handle_start( void )
{
    periodic_tx_config->scheduler_config.start_time_ms = smtc_modem_hal_get_time_in_ms( ) + TX_PERIODICITY_IN_MS;
    SMTC_SW_PLATFORM( smtc_rac_submit_radio_transaction( periodic_tx_handle ) );
    SMTC_HAL_TRACE_INFO( "Periodic tx done\n" );
}

static void results_callback( smtc_rac_radio_lora_params_t* radio_lora_params, ranging_params_settings_t* ranging_params_settings,
    ranging_global_result_t* ranging_global_results, const char* region )
{
    app_ranging_radio_settings_output( radio_lora_params, ranging_params_settings );
    app_ranging_results_output( ranging_global_results, RANGING_HOPPING_CHANNELS_MAX );

#if DT_HAS_CHOSEN( zephyr_display )
    uint8_t        output_str[16];
    static uint8_t dash_x = 0;
    static uint32_t loop_count = 0;
    char *dots[] = { ".   ", "..  ", "... ", "...." };

    oled_show_str( 4 * 24, 0, (char *)region, 1 );

    snprintf( output_str, 4, "%d", radio_lora_params->sf );
    oled_show_str( 0, 6, "SF:", 1 );
    oled_show_str( 4 * 6, 6, output_str, 1 );

    snprintf( output_str, 6, "%d ", ranging_global_results->rssi_value );
    oled_show_str( 4 * 16, 6, "RSSI:", 1 );
    oled_show_str( 4 * 26, 6, output_str, 1 );

    oled_show_str( 0, 7, "BW:", 1 );
    oled_show_str( 4 * 6, 7, bw_str[( int ) radio_lora_params->bw], 1 );

    snprintf( output_str, 6, "%d ", ranging_global_results->snr_value );
    oled_show_str( 4 * 16, 7, "SNR :", 1 );
    oled_show_str( 4 * 26, 7, output_str, 1 );

    if(is_manager == true)
    {
        if( dash_x >= 128 )
            dash_x = 0;

        if( dash_x == 0 )
        {
            oled_show_str( 0, 2, " -", 1 );
            oled_show_str( 31, 2, " ", 1 );
        }
        else
        {
            oled_show_str( dash_x, 2, " -", 1 );
        }
        dash_x += 8;

        oled_show_str( 0, 0, "Distance:", 2 );
        snprintf( output_str, 16, "   %d meters  ", ranging_global_results->rng_distance );
        oled_show_str( 0, 3, output_str, 2 );
    }
    else
    {
        loop_count++;
        if (loop_count == 1)  // overwrite just once
            oled_show_str( 0, 2, "   Running   ", 2 );
        oled_show_str( 8 * 10, 2, dots[loop_count % 4], 2 );
    }
#endif
}

/* --- EOF ------------------------------------------------------------------ */
