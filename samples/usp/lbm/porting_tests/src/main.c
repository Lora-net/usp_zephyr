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

#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <stdlib.h>

#include <smtc_modem_hal.h>
#include <smtc_rac_api.h>

#include <zephyr/usp/lora_lbm_transceiver.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER( porting_tests, 3 );

#if defined( SX128X )
#include <ralf_sx128x.h>
#elif defined( SX126X )
#include <ralf_sx126x.h>
#include <sx126x.h>
#elif defined( LR11XX )
#include <ralf_lr11xx.h>
#include <lr11xx_system.h>
#elif defined( LR20XX )
#include <ralf_lr20xx.h>
#include <lr20xx_system.h>
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/* !! SHOULD BE DEFINED BY USER !! */
#define ENABLE_TEST_FLASH 0 /* Enable flash porting test BUT disable other porting tests */

/* Delay introduced by HAL_LPTIM_TimeOut_Start_IT function of stm32l4xx_hal_lptim.c file */
#define COMPENSATION_IN_MS_STM32L4 0

#define NB_LOOP_TEST_SPI 2
#define NB_LOOP_TEST_CONFIG_RADIO 2

#define FREQ_NO_RADIO 868300000
#define SYNC_WORD_NO_RADIO 0x21

#define MARGIN_GET_TIME_IN_MS 1
#define MARGIN_TIMER_IRQ_IN_MS 2
#define MARGIN_TIME_CONFIG_RADIO_IN_MS 8
#define MARGIN_SLEEP_IN_MS 2

#define PORTING_TEST_MSG_OK( ) LOG_INF( " OK " )
#define PORTING_TEST_MSG_WARN( ... ) LOG_WRN( __VA_ARGS__ )
#define PORTING_TEST_MSG_NOK( ... ) LOG_ERR( __VA_ARGS__ )

#if defined( LR20XX )
#define LR20XX_FW_VERSION_MAJOR 0x01
#define LR20XX_FW_VERSION_MINOR 0x18
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */
/**
 * @brief Return test enumeration
 */
typedef enum return_code_test_e
{
    RC_PORTING_TEST_OK       = 0x00,
    RC_PORTING_TEST_NOK      = 0x01,
    RC_PORTING_TEST_RELAUNCH = 0x02,
} return_code_test_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

#if defined( SX128X )
static ralf_t modem_radio = RALF_SX128X_INSTANTIATE( NULL );
#elif defined( SX126X )
static ralf_t modem_radio = RALF_SX126X_INSTANTIATE( NULL );
#elif defined( LR11XX )
static ralf_t modem_radio = RALF_LR11XX_INSTANTIATE( NULL );
#elif defined( LR20XX )
static ralf_t modem_radio = RALF_LR20XX_INSTANTIATE( NULL );
#elif defined( SX127X )
#include "sx127x.h"
static sx127x_t sx127x;
static ralf_t   modem_radio = RALF_SX127X_INSTANTIATE( &sx127x );
#else
#error "Please select radio board.."
#endif

/* lr11xx radio context and its use in the ralf layer */
static const struct device* transceiver = DEVICE_DT_GET( DT_ALIAS( lora_transceiver ) );

static volatile bool     radio_irq_raised;
static volatile bool     irq_rx_timeout_raised;
static volatile bool     timer_irq_raised;
static volatile uint32_t irq_time_ms;
static volatile uint32_t irq_time_s;

/* LoRa configurations TO NOT receive or transmit */
static ralf_params_lora_t rx_lora_param = { .sync_word                       = SYNC_WORD_NO_RADIO,
                                            .symb_nb_timeout                 = 0,
                                            .rf_freq_in_hz                   = FREQ_NO_RADIO,
                                            .mod_params.cr                   = RAL_LORA_CR_4_5,
                                            .mod_params.sf                   = RAL_LORA_SF12,
                                            .mod_params.bw                   = RAL_LORA_BW_125_KHZ,
                                            .mod_params.ldro                 = 0,
                                            .pkt_params.header_type          = RAL_LORA_PKT_EXPLICIT,
                                            .pkt_params.pld_len_in_bytes     = 255,
                                            .pkt_params.crc_is_on            = false,
                                            .pkt_params.invert_iq_is_on      = true,
                                            .pkt_params.preamble_len_in_symb = 8 };

static ralf_params_lora_t tx_lora_param = { .sync_word                       = SYNC_WORD_NO_RADIO,
                                            .symb_nb_timeout                 = 0,
                                            .rf_freq_in_hz                   = FREQ_NO_RADIO,
                                            .output_pwr_in_dbm               = 14,
                                            .mod_params.cr                   = RAL_LORA_CR_4_5,
                                            .mod_params.sf                   = RAL_LORA_SF12,
                                            .mod_params.bw                   = RAL_LORA_BW_125_KHZ,
                                            .mod_params.ldro                 = 0,
                                            .pkt_params.header_type          = RAL_LORA_PKT_EXPLICIT,
                                            .pkt_params.pld_len_in_bytes     = 50,
                                            .pkt_params.crc_is_on            = true,
                                            .pkt_params.invert_iq_is_on      = false,
                                            .pkt_params.preamble_len_in_symb = 8 };
#if( ENABLE_TEST_FLASH != 0 )
static const char* const name_context_type[] = { "MODEM", "LR1MAC", "DEVNONCE", "SECURE_ELEMENT" };
#endif

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */
static void radio_tx_irq_callback( void* obj );
static void radio_rx_irq_callback( void* obj );
static void radio_irq_callback_get_time_in_s( void* obj );
static void timer_irq_callback( void* obj );

static bool               reset_init_radio( void );
static return_code_test_t test_get_time_in_s( void );
static return_code_test_t test_get_time_in_ms( void );

static bool porting_test_spi( void );
static bool porting_test_radio_irq( void );
static bool porting_test_get_time( void );
static bool porting_test_timer_irq( void );
static bool porting_test_stop_timer( void );
static bool porting_test_disable_enable_irq( void );
static bool porting_test_random( void );
static bool porting_test_config_rx_radio( void );
static bool porting_test_config_tx_radio( void );
static bool porting_test_sleep_ms( void );
static bool porting_test_timer_irq_low_power( void );
#if( ENABLE_TEST_FLASH != 0 )
static bool test_context_store_restore( modem_context_type_t context_type );
static bool porting_test_flash( void );
#endif
/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/**
 * @brief Example to test functionality to be porting by the user according to the MCU hardware
 *
 */
int main( void )
{
    bool ret = true;

    // Initialize Modem HAL
    modem_radio.ral.context = transceiver;
    lorawan_smtc_modem_hal_init( transceiver );

    LOG_INF( "" );
    LOG_INF( "" );
    LOG_INF( "PORTING_TESTS example is starting" );
    LOG_INF( "" );
    LOG_INF( "" );

#if( ENABLE_TEST_FLASH == 0 )

    ret = porting_test_spi( );
    if( ret == false )
    {
        return 1;
    }

    ret = porting_test_radio_irq( );
    if( ret == false )
    {
        return 1;
    }

    ret = porting_test_get_time( );

    ret = porting_test_timer_irq( );
    if( ret == false )
    {
        return 1;
    }

    porting_test_stop_timer( );

    porting_test_disable_enable_irq( );

    porting_test_random( );

    porting_test_config_rx_radio( );

    porting_test_config_tx_radio( );

    porting_test_sleep_ms( );

    porting_test_timer_irq_low_power( );

#else

    ret = porting_test_flash( );
    if( ret == false )
    {
        return ret;
    }

    LOG_INF_COLOR(
        " MCU RESET => relaunch tests and check if read after reset = "
        "write before reset",
        HAL_DBG_TRACE_COLOR_BLUE );

    hal_mcu_set_sleep_for_ms( 2000 );

    hal_mcu_reset( );

#endif

    LOG_INF( "---------------------------------------- PORTING_TESTS END" );

    return 0;
}

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */

#if defined( LR11XX )
static int lr11xx_expected_fw_version( const struct device* transceiver )
{
    switch( lora_transceiver_get_model( transceiver ) )
    {
    case LR11XX_SYSTEM_VERSION_TYPE_LR1110:
        return 0x0401;
    case LR11XX_SYSTEM_VERSION_TYPE_LR1120:
        return 0x0201;
    case LR11XX_SYSTEM_VERSION_TYPE_LR1121:
        return 0x0103;
    default:
        return -1;
    }
}
#endif

/**
 * @brief Test SPI
 *
 * @remark
 * Prerequisite:
 * Radio reset should be implemented:
 * - drive of gpio (hal_gpio_set_value)
 * - mcu wait us (hal_mcu_wait_us)
 *
 * Test processing:
 * - Reset radio
 * - Read data through SPI
 * - Check if data is coherent
 *
 * Ported functions:
 * lr11xx_hal_read
 *      lr11xx_hal_check_device_ready
 *          lr11xx_hal_wait_on_busy
 *              hal_gpio_get_value
 *          hal_gpio_set_value
 *          hal_spi_in_out
 *
 * @return bool True if test is successful
 */
static bool porting_test_spi( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    uint16_t counter_nok = 0;

    /* Reset radio (prerequisite) */
    ral_reset( &( modem_radio.ral ) );

    for( uint16_t i = 0; i < NB_LOOP_TEST_SPI; i++ )
    {
#if defined( LR11XX )

        lr11xx_system_version_t version;
        lr11xx_status_t         status;

        int fw_version_expected = lr11xx_expected_fw_version( transceiver );

        status = lr11xx_system_get_version( transceiver, &version );

        if( status == LR11XX_STATUS_OK )
        {
            if( version.fw == fw_version_expected )
            {
                LOG_INF( " LR11XX firmware version is 0x%04X \n", version.fw );
                break;
            }
            else
            {
                PORTING_TEST_MSG_NOK(
                    " Wrong LR11XX firmware version: expected "
                    "0x%04X / get 0x%04X",
                    fw_version_expected, version.fw );
                counter_nok++;
            }
        }
        else
        {
            PORTING_TEST_MSG_NOK( " Failed to get LR11XX firmware version" );
            counter_nok++;
        }

#elif defined( LR20XX )
        lr20xx_system_version_t version;
        lr20xx_status_t         status;

        status = lr20xx_system_get_version( transceiver, &version );

        if( status == LR20XX_STATUS_OK )
        {
            if( ( version.major != LR20XX_FW_VERSION_MAJOR ) || ( version.minor != LR20XX_FW_VERSION_MINOR ) )
            {
                if( version.major != LR20XX_FW_VERSION_MAJOR )
                {
                    PORTING_TEST_MSG_NOK( " Wrong LR20XX major firmware version: expected 0x%02X / get 0x%02X",
                                          LR20XX_FW_VERSION_MAJOR, version.major );
                }
                if( version.minor != LR20XX_FW_VERSION_MINOR )
                {
                    PORTING_TEST_MSG_NOK( " Wrong LR20XX minor firmware version: expected 0x%02X / get 0x%02X",
                                          LR20XX_FW_VERSION_MINOR, version.minor );
                }
                counter_nok++;
            }
        }
        else
        {
            PORTING_TEST_MSG_NOK( " Failed to get LR20XX firmware version" );
            counter_nok++;
        }

#elif defined( SX126X )
        sx126x_chip_status_t chip_status;
        sx126x_status_t      status;

        status = sx126x_get_status( transceiver, &chip_status );

        if( status == SX126X_STATUS_OK )
        {
            if( chip_status.chip_mode == SX126X_CHIP_MODE_UNUSED )
            {
                PORTING_TEST_MSG_NOK( " Wrong SX126X chip mode, get SX126X_CHIP_MODE_UNUSED" );
                counter_nok++;
            }
        }
        else
        {
            PORTING_TEST_MSG_NOK( " Failed to get SX126X status" );
            counter_nok++;
        }
#else
        PORTING_TEST_MSG_NOK( " Radio is not supported" );
        return false;
#endif
    }

    if( counter_nok == 0 )
    {
        PORTING_TEST_MSG_OK( );
    }
    else
    {
        PORTING_TEST_MSG_WARN( " Failed test = %u / %u", counter_nok, NB_LOOP_TEST_SPI );
        return false;
    }

    return true;
}

/**
 * @brief Reset and init radio
 *
 * @remark
 * Test processing:
 * - Reset radio
 * - Init radio
 * - Set radio in sleep mode
 *
 * Ported functions:
 * ral_reset:
 * lr11xx_hal_reset
 *     hal_gpio_set_value
 *     hal_mcu_wait_us
 * ral_init:
 * ral_lr11xx_init
 *     ral_lr11xx_bsp_get_crc_state
 *     ral_lr11xx_bsp_get_reg_mode
 *     ral_lr11xx_bsp_get_rf_switch_cfg
 *     ral_lr11xx_bsp_get_xosc_cfg
 *     lr11xx_system_set_tcxo_mode
 *         lr11xx_hal_write
 *     lr11xx_system_calibrate
 *         lr11xx_hal_write
 * ral_set_sleep:
 * ral_lr11xx_set_sleep
 *     lr11xx_system_set_sleep
 *         lr11xx_hal_write
 *
 * @return bool True if test is successful
 */
static bool reset_init_radio( void )
{
    ral_status_t status = RAL_STATUS_ERROR;

    /* Reset, init radio and put it in sleep mode */
    ral_reset( &( modem_radio.ral ) );

    status = ral_init( &( modem_radio.ral ) );
    if( status != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_init() function failed" );
        return false;
    }

    status = ral_set_sleep( &( modem_radio.ral ), true );
    /* k_msleep(2000); */
    smtc_modem_hal_set_ant_switch( false );
    if( status != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_set_sleep() function failed" );
        return false;
    }

    return true;
}

/**
 * @brief Test radio irq
 *
 * @remark
 * Test processing:
 * - Reset and init radio
 * - Configure radio irq
 * - Configure radio with bad parameters to receive a rx timeout irq
 * - Configure radio in reception mode with a timeout
 * - Wait
 * - Check if rx timeout irq is raised
 *
 * Ported functions:
 * smtc_modem_hal_irq_config_radio_irq:
 *     hal_gpio_irq_attach
 *
 * lr11xx_hal_write
 *     lr11xx_hal_check_device_ready
 *         lr11xx_hal_wait_on_busy
 *         hal_gpio_get_value
 *     hal_gpio_set_value
 *     hal_spi_in_out
 *     hal_gpio_set_value
 *     hal_mcu_wait_us
 *
 * @return bool True if test is successful
 */
static bool porting_test_radio_irq( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    bool     ret              = true;
    uint32_t rx_timeout_in_ms = 500;

    radio_irq_raised = false;

    /* Reset, init radio and put it in sleep mode */
    ret = reset_init_radio( );
    if( ret == false )
    {
        PORTING_TEST_MSG_NOK( " Could not reset radio" );
        return ret;
    }

    /* Setup radio and relative irq */
    smtc_modem_hal_irq_config_radio_irq( radio_rx_irq_callback, NULL );
    smtc_modem_hal_start_radio_tcxo( );
    smtc_modem_hal_set_ant_switch( false );
    if( ralf_setup_lora( &modem_radio, &rx_lora_param ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ralf_setup_lora() function failed" );
        return false;
    }

    if( ral_set_dio_irq_params( &( modem_radio.ral ), RAL_IRQ_RX_DONE | RAL_IRQ_RX_TIMEOUT | RAL_IRQ_RX_HDR_ERROR |
                                                          RAL_IRQ_RX_CRC_ERROR ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_set_dio_irq_params() function failed" );
        return false;
    }

    /* Configure radio in reception mode */
    if( ral_set_rx( &( modem_radio.ral ), rx_timeout_in_ms ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_set_rx() function failed" );
        return false;
    }

    /* Wait 2 * timeout */
    k_usleep( ( rx_timeout_in_ms * 2 ) * 1000 );

    if( radio_irq_raised == true )
    {
        PORTING_TEST_MSG_OK( );
    }
    else
    {
        PORTING_TEST_MSG_NOK( " Timeout, radio irq not received" );
        return false;
    }
    return true;
}

/**
 * @brief Test get time in s
 *
 *
 * @remark
 *  Test processing:
 * - Reset, init and configure radio
 * - Configure radio in reception mode with a timeout
 * - Get start time
 * - Wait radio irq (get stop time in irq callback)
 * - Check if time is coherent with the configured timeout radio irq
 * Note: if radio irq received different of rx timeout irq -> relaunch test
 *
 * Ported functions:
 * smtc_modem_hal_get_time_in_s
 *      hal_rtc_get_time_s
 *
 * @return return_code_test_t   RC_PORTING_TEST_OK
 *                              RC_PORTING_TEST_NOK
 *                              RC_PORTING_TEST_RELAUNCH
 */
static return_code_test_t test_get_time_in_s( void )
{
    LOG_INF( " * Get time in second: " );

    bool     ret              = true;
    uint32_t rx_timeout_in_ms = 5000;

    radio_irq_raised      = false;
    irq_rx_timeout_raised = false;

    rx_lora_param.symb_nb_timeout = 0;

    /* Reset, init radio and put it in sleep mode */
    ret = reset_init_radio( );
    if( ret == false )
    {
        return RC_PORTING_TEST_NOK;
    }

    /* Setup radio and relative irq */
    smtc_modem_hal_irq_config_radio_irq( radio_irq_callback_get_time_in_s, NULL );

    smtc_modem_hal_start_radio_tcxo( );
    smtc_modem_hal_set_ant_switch( false );
    if( ralf_setup_lora( &modem_radio, &rx_lora_param ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ralf_setup_lora() function failed" );
        return RC_PORTING_TEST_NOK;
    }
    if( ral_set_dio_irq_params( &( modem_radio.ral ), RAL_IRQ_RX_DONE | RAL_IRQ_RX_TIMEOUT | RAL_IRQ_RX_HDR_ERROR |
                                                          RAL_IRQ_RX_CRC_ERROR ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_set_dio_irq_params() function failed" );
        return RC_PORTING_TEST_NOK;
    }

    /* Configure radio in reception mode */
    if( ral_set_rx( &( modem_radio.ral ), rx_timeout_in_ms ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_set_rx() function failed" );
        return RC_PORTING_TEST_NOK;
    }
    uint32_t start_time_s = smtc_modem_hal_get_time_in_s( );

    while( radio_irq_raised == false )
    {
        /* Do nothing */
    }

    if( irq_rx_timeout_raised == false )
    {
        PORTING_TEST_MSG_WARN( "Radio irq received but not RAL_IRQ_RX_TIMEOUT -> relaunched test" );
        return RC_PORTING_TEST_RELAUNCH;
    }

    uint32_t time = irq_time_s - start_time_s;

    if( time == ( rx_timeout_in_ms / 1000 ) )
    {
        PORTING_TEST_MSG_OK( );
        LOG_INF( " Time expected %us / get %us (no margin)", ( rx_timeout_in_ms / 1000 ), time );
    }
    else
    {
        PORTING_TEST_MSG_NOK( " Time is not coherent: expected %us / get %us (no margin)", ( rx_timeout_in_ms / 1000 ),
                              time );
        return RC_PORTING_TEST_NOK;
    }

    return RC_PORTING_TEST_OK;
}

/**
 * @brief Test get time in ms
 *
 *
 * @remark
 *  Test processing:
 * - Reset, init and configure radio (with a timeout symbol number)
 * - Get start time
 * - Configure radio in reception mode
 * - Wait radio irq (get stop time in irq callback)
 * - Check if time is coherent with the configured timeout symbol number
 * Note: if radio irq received different of rx timeout irq -> relaunch test
 *
 * Ported functions:
 * smtc_modem_hal_get_time_in_ms
 *      hal_rtc_get_time_ms
 *
 * @return return_code_test_t   RC_PORTING_TEST_OK
 *                              RC_PORTING_TEST_NOK
 *                              RC_PORTING_TEST_RELAUNCH
 */
static return_code_test_t test_get_time_in_ms( void )
{
    LOG_INF( " * Get time in millisecond: " );

    bool    ret           = true;
    uint8_t wait_start_ms = 5;

    radio_irq_raised      = false;
    irq_rx_timeout_raised = false;

    /* To avoid misalignment between symb timeout and real timeout for all radio, a number of
     * symbols smaller than 63 is to be used.
     */
    rx_lora_param.symb_nb_timeout = 62;
    rx_lora_param.mod_params.sf   = RAL_LORA_SF12;
    rx_lora_param.mod_params.bw   = RAL_LORA_BW_125_KHZ;

    /* Warning: to be updated if previous parameters (SF and BW) are changed */
    uint32_t symb_time_ms =
        ( uint32_t ) ( rx_lora_param.symb_nb_timeout * ( ( 1 << 12 ) / 125.0 ) ); /* 2^(SF) / BW * symb_nb_timeout */

    /* Reset, init radio and put it in sleep mode */
    ret = reset_init_radio( );
    if( ret == false )
    {
        return RC_PORTING_TEST_NOK;
    }

    /* Setup radio and relative irq */
    smtc_modem_hal_irq_config_radio_irq( radio_rx_irq_callback, NULL );

    smtc_modem_hal_start_radio_tcxo( );
    smtc_modem_hal_set_ant_switch( false );
    if( ralf_setup_lora( &modem_radio, &rx_lora_param ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ralf_setup_lora() function failed" );
        return RC_PORTING_TEST_NOK;
    }
    if( ral_set_dio_irq_params( &( modem_radio.ral ), RAL_IRQ_RX_DONE | RAL_IRQ_RX_TIMEOUT | RAL_IRQ_RX_HDR_ERROR |
                                                          RAL_IRQ_RX_CRC_ERROR ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_set_dio_irq_params() function failed" );
        return RC_PORTING_TEST_NOK;
    }

    /* Wait 5ms to start */
    uint32_t start_time_ms = smtc_modem_hal_get_time_in_ms( ) + wait_start_ms;

    while( smtc_modem_hal_get_time_in_ms( ) < start_time_ms )
    {
        /* Do nothing */
    }

    /* Configure radio in reception mode */
    if( ral_set_rx( &( modem_radio.ral ), 0 ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_set_rx() function failed" );
        return RC_PORTING_TEST_NOK;
    }

    while( radio_irq_raised == false )
    {
        /* Do nothing */
    }

    if( irq_rx_timeout_raised == false )
    {
        PORTING_TEST_MSG_WARN( " Radio irq received but not RAL_IRQ_RX_TIMEOUT -> relaunched test" );
        return RC_PORTING_TEST_RELAUNCH;
    }

    uint32_t time = irq_time_ms - start_time_ms - smtc_modem_hal_get_radio_tcxo_startup_delay_ms( );
    if( abs( time - symb_time_ms ) <= MARGIN_GET_TIME_IN_MS )
    {
        PORTING_TEST_MSG_OK( );
        LOG_INF( " Time expected %ums / get %ums (margin +/-%ums)", ( uint32_t ) symb_time_ms, time,
                 MARGIN_GET_TIME_IN_MS );
    }
    else
    {
        PORTING_TEST_MSG_NOK(
            " Time is not coherent with radio irq : expected %ums / get "
            "%ums (margin +/-%ums)",
            ( uint32_t ) symb_time_ms, time, MARGIN_GET_TIME_IN_MS );
        return RC_PORTING_TEST_NOK;
    }

    return RC_PORTING_TEST_OK;
}

/**
 * @brief Test time (Get time in s and in ms)
 *
 * @remark See test_get_time_in_s() and test_get_time_in_ms() functions
 *
 * @return bool True if test is successful
 */
static bool porting_test_get_time( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    return_code_test_t ret = RC_PORTING_TEST_OK;

    do
    {
        ret = test_get_time_in_s( );
        if( ret == RC_PORTING_TEST_NOK )
        {
            return false;
        }
    } while( ret == RC_PORTING_TEST_RELAUNCH );

    do
    {
        ret = test_get_time_in_ms( );
        if( ret == RC_PORTING_TEST_NOK )
        {
            return false;
        }
    } while( ret == RC_PORTING_TEST_RELAUNCH );

    return true;
}

/**
 * @brief Test timer IRQ
 *
 * @warning smtc_modem_hal_start_timer() function takes ~4ms for STM32L4 (see
 * HAL_LPTIM_TimeOut_Start_IT())
 *
 * @remark
 * Test processing:
 * - Get start time
 * - Configure and start timer
 * - Wait timer irq (get stop time in irq callback)
 * - Check the time elapsed between timer start and timer IRQ reception
 *
 * Ported functions:
 * smtc_modem_hal_start_timer
 *      hal_lp_timer_start
 *
 * @return bool True if test is successful
 */
static bool porting_test_timer_irq( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    uint32_t timer_ms      = 3000;
    uint8_t  wait_start_ms = 5;
    uint16_t timeout_ms    = 2000;

    timer_irq_raised = false;

    smtc_modem_hal_stop_timer( );

    /* Wait 5ms to start */
    uint32_t start_time_ms = smtc_modem_hal_get_time_in_ms( ) + wait_start_ms;

    while( smtc_modem_hal_get_time_in_ms( ) < start_time_ms )
    {
        k_msleep( 1 );
        /* Do nothing */
    }

    smtc_modem_hal_start_timer( timer_ms, timer_irq_callback,
                                NULL ); /* Warning this function takes ~3,69 ms for STM32L4 */

    /* Timeout if irq not raised */
    while( ( timer_irq_raised == false ) &&
           ( ( smtc_modem_hal_get_time_in_ms( ) - start_time_ms ) < ( timer_ms + timeout_ms ) ) )
    {
        k_msleep( 1 );
        /* Do nothing */
    }

    if( timer_irq_raised == false )
    {
        PORTING_TEST_MSG_NOK( " Timeout: timer irq not received" );
        return false;
    }

    uint32_t time = irq_time_ms - start_time_ms - COMPENSATION_IN_MS_STM32L4;

    if( ( time >= timer_ms ) && ( time <= timer_ms + MARGIN_TIMER_IRQ_IN_MS ) )
    {
        PORTING_TEST_MSG_OK( );
        LOG_INF( " Timer irq configured with %ums / get %ums (margin +%ums)", timer_ms, time, MARGIN_TIMER_IRQ_IN_MS );
    }
    else
    {
        PORTING_TEST_MSG_NOK(
            " Timer irq delay is not coherent: expected %ums / get %ums "
            "(margin +%ums)",
            timer_ms, time, MARGIN_TIMER_IRQ_IN_MS );
        return false;
    }
    return true;
}

/**
 * @brief Test stop timer
 *
 * @remark
 * Test processing:
 * - Configure and start timer
 * - Wait
 * - Stop timer
 * - Wait the end of timer
 * - Check if timer IRQ is not received
 *
 * Ported functions:
 * smtc_modem_hal_stop_timer
 *      hal_lp_timer_stop
 *
 * @return bool True if test is successful
 */
static bool porting_test_stop_timer( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    uint32_t timer_ms = 1000;

    timer_irq_raised = false;

    smtc_modem_hal_start_timer( timer_ms, timer_irq_callback, NULL );

    /* Wait half of timer */
    uint32_t time = smtc_modem_hal_get_time_in_ms( );

    while( ( smtc_modem_hal_get_time_in_ms( ) - time ) < ( timer_ms / 2 ) )
    {
        /* Do nothing */
    }

    smtc_modem_hal_stop_timer( );

    /* Wait a little more than the end of timer */
    time = smtc_modem_hal_get_time_in_ms( );
    while( ( smtc_modem_hal_get_time_in_ms( ) - time ) < ( timer_ms + 500 ) )
    {
        /* Do nothing */
    }

    if( timer_irq_raised == false )
    {
        PORTING_TEST_MSG_OK( );
    }
    else
    {
        PORTING_TEST_MSG_NOK( " Timer irq raised while timer is stopped" );
        return false;
    }
    return true;
}

/**
 * @brief Test enable/disable irq
 *
 * @remark
 * Test processing:
 * - Disable irq
 * - Start timer with irq
 * - Wait the end of timer
 * - Check if timer irq is not raised
 * - Enable irq
 * - Check if timer irq is raised
 *
 * Ported functions:
 * smtc_modem_hal_disable_modem_irq
 * smtc_modem_hal_enable_modem_irq
 *
 * @return bool True if test is successful
 */
static bool porting_test_disable_enable_irq( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    uint32_t timer_ms = 3000;

    timer_irq_raised = false;

    smtc_modem_hal_disable_modem_irq( );

    smtc_modem_hal_start_timer( timer_ms, timer_irq_callback, NULL );

    /* Wait a little more than the end of timer */
    uint32_t time = smtc_modem_hal_get_time_in_ms( );

    while( ( smtc_modem_hal_get_time_in_ms( ) - time ) < ( timer_ms + 1000 ) )
    {
        /* Do nothing */
    }

    if( timer_irq_raised == true )
    {
        PORTING_TEST_MSG_NOK( " Timer irq raised while irq is disabled" );
        return false;
    }

    smtc_modem_hal_enable_modem_irq( );

    if( timer_irq_raised == true )
    {
        PORTING_TEST_MSG_OK( );
    }
    else
    {
        PORTING_TEST_MSG_NOK( " Timer irq not received while irq is reenabled" );
        return false;
    }

    return true;
}

/**
 * @brief Test get random numbers
 *
 * @remark
 * Test processing:
 * 1) - Get 2 random numbers in full range
 * - Check if numbers are not equals to 0 and are different
 * 2) - Get 2 random numbers in a defined range
 * - Check if numbers are different and in the defined range
 * 3) - Get random draw of numbers between in a defined range
 * - Check if draw of each value is equivalent
 *
 * Ported functions:
 * smtc_modem_hal_get_random_nb_in_range
 *      hal_rng_get_random_in_range
 *
 * @return bool True if test is successful
 */
static bool porting_test_random( void )
{
    bool ret = true;

    LOG_INF( "---------------------------------------- %s :", __func__ );

    LOG_INF( " * Get random nb : " );
    uint32_t rdom1 = smtc_modem_hal_get_random_nb_in_range( 0, 0xFFFFFFFF );
    uint32_t rdom2 = smtc_modem_hal_get_random_nb_in_range( 0, 0xFFFFFFFF );

    if( ( rdom1 != 0 ) && ( rdom2 != 0 ) && ( rdom1 != rdom2 ) )
    {
        PORTING_TEST_MSG_OK( );
        LOG_INF( " random1 = %u, random2 = %u", rdom1, rdom2 );
    }
    else
    {
        PORTING_TEST_MSG_WARN( " => random1 = %u, random2 = %u", rdom1, rdom2 );
        ret = false;
    }

    LOG_INF( " * Get random nb in range : " );
    uint32_t range_min = 1;
    uint32_t range_max = 42;

    rdom1 = smtc_modem_hal_get_random_nb_in_range( range_min, range_max );
    rdom2 = smtc_modem_hal_get_random_nb_in_range( range_min, range_max );

    if( ( rdom1 >= range_min ) && ( rdom1 <= range_max ) && ( rdom2 >= range_min ) && ( rdom2 <= range_max ) &&
        ( rdom1 != rdom2 ) )
    {
        PORTING_TEST_MSG_OK( );
        LOG_INF( " random1 = %u, random2 = %u in range [%u;%u]", rdom1, rdom2, range_min, range_max );
    }
    else
    {
        PORTING_TEST_MSG_WARN( " => random1 = %u, random2 = %u, expected range [%u;%u]", rdom1, rdom2, range_min,
                               range_max );
        ret = false;
    }

    LOG_INF( " * Get random draw : " );
    range_min                       = 1;
    range_max                       = 10;
    uint32_t tab_counter_random[10] = { 0 };
    uint32_t nb_draw                = 100000;
    uint32_t probability_draw       = nb_draw / ( range_max - range_min + 1 );
    /* Warning to be update if probability_draw is changed */
    int16_t margin = ( probability_draw * 5 ) / 100; /* error margin = 5% of probability_draw */

    for( uint32_t i = 0; i < nb_draw; i++ )
    {
        rdom1 = smtc_modem_hal_get_random_nb_in_range( range_min, range_max );
        tab_counter_random[rdom1 - 1]++;
    }

    uint8_t tab_size = sizeof( tab_counter_random ) / sizeof( uint32_t );

    for( uint16_t i = 0; i < tab_size; i++ )
    {
        if( abs( probability_draw - tab_counter_random[i] ) > margin )
        {
            PORTING_TEST_MSG_WARN(
                " => The number %u has been drawned %u times, "
                "Expected [%u;%u] times",
                ( i + 1 ), tab_counter_random[i], ( probability_draw - margin ), ( probability_draw + margin ) );
            ret = false;
        }
    }

    if( ret == true )
    {
        PORTING_TEST_MSG_OK( );
    }
    else
    {
        PORTING_TEST_MSG_WARN(
            " Warning smtc_modem_hal_get_random_nb_in_range error "
            "margin > 5%%" );
    }

    LOG_INF( " Random draw of %u numbers between [%u;%u] range", nb_draw, range_min, range_max );

    return ret;
}

/**
 * @brief Test time to configure rx radio
 *
 * @remark
 * Test processing:
 * - Init radio
 * - Configure radio irq
 * - Get start time
 * - Configure rx radio
 * - Get stop time
 * - Check configuration time
 *
 * @return bool True if test is successful
 */
static bool porting_test_config_rx_radio( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    bool ret = true;
    /* uint32_t rx_timeout_in_ms = 500; */
    uint16_t counter_nok = 0;

    radio_irq_raised = false;

    /* Reset, init and put it in sleep mode radio
     * Setup radio and relative irq
     */
    ret = reset_init_radio( );
    k_msleep( 500 );
    if( ret == false )
    {
        return ret;
    }

    smtc_modem_hal_irq_config_radio_irq( radio_rx_irq_callback, NULL );

    for( uint16_t i = 0; i < NB_LOOP_TEST_CONFIG_RADIO; i++ )
    {
        radio_irq_raised = false;

        uint32_t start_time_ms = smtc_modem_hal_get_time_in_ms( );

        /* Setup radio and relative irq */
        smtc_modem_hal_start_radio_tcxo( );
        smtc_modem_hal_set_ant_switch( false );
        if( ralf_setup_lora( &modem_radio, &rx_lora_param ) != RAL_STATUS_OK )
        {
            PORTING_TEST_MSG_NOK( " ralf_setup_lora() function failed" );
            return false;
        }
        if( ral_set_dio_irq_params( &( modem_radio.ral ), RAL_IRQ_RX_DONE | RAL_IRQ_RX_TIMEOUT | RAL_IRQ_RX_HDR_ERROR |
                                                              RAL_IRQ_RX_CRC_ERROR ) != RAL_STATUS_OK )
        {
            PORTING_TEST_MSG_NOK( " ral_set_dio_irq_params() function failed" );
            return false;
        }

        uint32_t time = smtc_modem_hal_get_time_in_ms( ) - start_time_ms;

        if( time >= MARGIN_TIME_CONFIG_RADIO_IN_MS )
        {
            PORTING_TEST_MSG_NOK( " Configuration of rx radio is too long: %ums (margin +%ums)", time,
                                  MARGIN_TIME_CONFIG_RADIO_IN_MS );
            counter_nok++;
        }

        smtc_modem_hal_stop_radio_tcxo( );
    }

    if( counter_nok == 0 )
    {
        PORTING_TEST_MSG_OK( );
    }
    else
    {
        PORTING_TEST_MSG_WARN( " => Failed test = %u / %u", counter_nok, NB_LOOP_TEST_CONFIG_RADIO );
    }

    return true;
}

/**
 * @brief Test time to configure tx radio
 *
 * @remark
 * Test processing:
 * - Init radio
 * - Configure radio irq
 * - Get start time
 * - Configure tx radio
 * - Get stop time
 * - Check configuration time
 *
 * @return bool True if test is successful
 */
static bool porting_test_config_tx_radio( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    uint16_t payload_size = 50;
    uint8_t  payload[50]  = { 0 };
    uint16_t counter_nok  = 0;

    radio_irq_raised = false;

    /* Reset, init and put it in sleep mode radio */
    bool ret = reset_init_radio( );

    if( ret == false )
    {
        return ret;
    }

    /* Setup radio and relative irq */
    smtc_modem_hal_irq_config_radio_irq( radio_tx_irq_callback, NULL );

    for( uint16_t i = 0; i < NB_LOOP_TEST_CONFIG_RADIO; i++ )
    {
        radio_irq_raised = false;

        uint32_t start_time_ms = smtc_modem_hal_get_time_in_ms( );

        smtc_modem_hal_start_radio_tcxo( );
        smtc_modem_hal_set_ant_switch( true );
        if( ralf_setup_lora( &modem_radio, &tx_lora_param ) != RAL_STATUS_OK )
        {
            PORTING_TEST_MSG_NOK( " ralf_setup_lora() function failed" );
            return false;
        }
        if( ral_set_dio_irq_params( &( modem_radio.ral ), RAL_IRQ_TX_DONE ) != RAL_STATUS_OK )
        {
            PORTING_TEST_MSG_NOK( " ral_set_dio_irq_params() function failed" );
            return false;
        }

        if( ral_set_pkt_payload( &( modem_radio.ral ), payload, payload_size ) != RAL_STATUS_OK )
        {
            PORTING_TEST_MSG_NOK( " ral_set_pkt_payload() function failed" );
            return false;
        }

        uint32_t time = smtc_modem_hal_get_time_in_ms( ) - start_time_ms;

        if( time >= MARGIN_TIME_CONFIG_RADIO_IN_MS )
        {
            PORTING_TEST_MSG_NOK( " Configuration of tx radio is too long: %ums (margin +%ums)", time,
                                  MARGIN_TIME_CONFIG_RADIO_IN_MS );
            counter_nok++;
        }

        smtc_modem_hal_stop_radio_tcxo( );
    }

    if( counter_nok == 0 )
    {
        PORTING_TEST_MSG_OK( );
    }
    else
    {
        PORTING_TEST_MSG_WARN( " => Failed test = %u / %u", counter_nok, NB_LOOP_TEST_CONFIG_RADIO );
    }

    return true;
}

/**
 * @brief Test sleep time
 *
 * @remark
 * Test processing:
 * - Get start time
 * - Set sleep for ms
 * - Get stop time
 * - Check sleep time
 *
 * Ported functions:
 * hal_mcu_set_sleep_for_ms
 *      hal_watchdog_reload
 *      hal_rtc_wakeup_timer_set_ms
 *      lpm_handler
 *      hal_rtc_wakeup_timer_stop
 *
 * @return bool True if test is successful
 */
static bool porting_test_sleep_ms( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    bool    ret           = true;
    int32_t sleep_ms      = 2000;
    uint8_t wait_start_ms = 5;

    /* Wait 5ms to start */
    uint32_t start_time_ms = smtc_modem_hal_get_time_in_ms( ) + wait_start_ms;

    while( smtc_modem_hal_get_time_in_ms( ) < start_time_ms )
    {
        /* Do nothing */
    }

    k_msleep( sleep_ms );

    uint32_t stop_time_ms = smtc_modem_hal_get_time_in_ms( );
    uint32_t time         = stop_time_ms - start_time_ms;

    if( abs( time - sleep_ms ) <= MARGIN_SLEEP_IN_MS )
    {
        PORTING_TEST_MSG_OK( );
        LOG_INF( " Sleep time expected %ums / get %ums (margin +/-%ums)", sleep_ms, time, MARGIN_SLEEP_IN_MS );
    }
    else
    {
        PORTING_TEST_MSG_WARN(
            " => Sleep time is not coherent: expected %ums / get %ums "
            "(margin +/-%ums)",
            sleep_ms, time, MARGIN_SLEEP_IN_MS );
    }
    return ret;
}

/**
 * @brief Test timer IRQ in low power
 *
 * @warning smtc_modem_hal_start_timer() function takes ~4ms for STM32L4 (see
 * HAL_LPTIM_TimeOut_Start_IT())
 *
 * @remark
 * Test processing:
 * - Get start time
 * - Configure and start timer
 * - Wait timer irq
 * - Get stop time
 * - Check the time elapsed between timer start and timer IRQ reception
 *
 * Ported functions:
 * smtc_modem_hal_start_timer
 *      hal_lp_timer_start
 *
 * @return bool True if test is successful
 */
static bool porting_test_timer_irq_low_power( void )
{
    LOG_INF( "---------------------------------------- %s :", __func__ );

    uint32_t timer_ms      = 3000;
    int32_t  sleep_ms      = timer_ms + 5000;
    uint8_t  wait_start_ms = 5;

    timer_irq_raised = false;

    smtc_modem_hal_stop_timer( );

    /* Wait 5ms to start */
    uint32_t start_time_ms = smtc_modem_hal_get_time_in_ms( ) + wait_start_ms;

    while( smtc_modem_hal_get_time_in_ms( ) < start_time_ms )
    {
        /* Do nothing */
    }

    smtc_modem_hal_start_timer( timer_ms, timer_irq_callback,
                                NULL ); /* Warning this function takes ~3,69 ms for STM32L4 */

    k_msleep( sleep_ms );

    if( timer_irq_raised == false )
    {
        PORTING_TEST_MSG_NOK( " Timeout: timer irq not received" );
        return false;
    }

    /* Warning to compensate delay introduced by smtc_modem_hal_start_timer for STM32L4 */
    uint32_t time = irq_time_ms - start_time_ms - COMPENSATION_IN_MS_STM32L4;

    if( ( time >= timer_ms ) && ( time <= timer_ms + MARGIN_TIMER_IRQ_IN_MS ) )
    {
        PORTING_TEST_MSG_OK( );
        LOG_INF( " Timer irq configured with %ums / get %ums (margin +%ums)", timer_ms, time, MARGIN_TIMER_IRQ_IN_MS );
    }
    else
    {
        PORTING_TEST_MSG_NOK(
            " Timer irq delay is not coherent: expected %ums / get %ums "
            "(margin +%ums)",
            timer_ms, time, MARGIN_TIMER_IRQ_IN_MS );
        return false;
    }
    return true;
}

/*
 * -----------------------------------------------------------------------------
 * --- FLASH PORTING TESTS -----------------------------------------------------
 */
#if( ENABLE_TEST_FLASH != 0 )

/**
 * @brief Test read/write context in flash
 *
 * @remark
 * Test processing:
 * - Read context in flash
 * - Write a different context in flash
 * - Read context in flash
 * - Check if read context is equal to written context
 *
 * Ported functions:
 * smtc_modem_hal_context_restore
 *     hal_flash_read_buffer
 * smtc_modem_hal_context_store
 *     hal_flash_write_buffer
 *
 * @param [in]  context_type   The context type
 *
 * @return bool True if test is successful
 */
static bool test_context_store_restore( modem_context_type_t context_type )
{
    bool    ret             = true;
    uint8_t read_buffer[8]  = { 0 };
    uint8_t write_buffer[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    bool    cmp             = true;

    LOG_INF( " * Context %s :", name_context_type[context_type] );

    smtc_modem_hal_context_restore( context_type, 0, read_buffer, sizeof( read_buffer ) );

    LOG_INF( " Read:  { " );
    for( uint8_t i = 0; i < sizeof( read_buffer ); i++ )
    {
        LOG_INF( "%u", read_buffer[i] );
        if( i != ( sizeof( read_buffer ) - 1 ) )
        {
            LOG_INF( ", " );
        }
    }
    LOG_INF( " }" );

    for( uint8_t i = 0; i < sizeof( write_buffer ); i++ )
    {
        if( read_buffer[i] == write_buffer[i] )
        {
            write_buffer[i] = ( read_buffer[i] + 1 ) % 256;
        }
    }

    LOG_INF( " Write: { " );
    for( uint8_t i = 0; i < sizeof( write_buffer ); i++ )
    {
        LOG_INF( "%u", write_buffer[i] );
        if( i != ( sizeof( write_buffer ) - 1 ) )
        {
            LOG_INF( ", " );
        }
    }
    LOG_INF( " }" );

    smtc_modem_hal_context_store( context_type, 0, write_buffer, sizeof( write_buffer ) );

    memset( read_buffer, 0, sizeof( read_buffer ) );
    smtc_modem_hal_context_restore( context_type, 0, read_buffer, sizeof( read_buffer ) );

    LOG_INF( " Read:  { " );
    for( uint8_t i = 0; i < sizeof( read_buffer ); i++ )
    {
        LOG_INF( "%u", read_buffer[i] );
        if( i != ( sizeof( read_buffer ) - 1 ) )
        {
            LOG_INF( ", " );
        }
    }
    LOG_INF( " }" );

    for( uint8_t i = 0; i < sizeof( write_buffer ); i++ )
    {
        if( read_buffer[i] != write_buffer[i] )
        {
            cmp = false;
        }
    }
    if( cmp == true )
    {
        LOG_INF( " Store/restore without MCU reset :" );
        PORTING_TEST_MSG_OK( );
    }
    else
    {
        PORTING_TEST_MSG_NOK( " Store or restore context failed (without MCU reset)" );
        return false;
    }

    return ret;
}

/**
 * @brief Test read/write context in flash
 *
 * @remark
 * Test processing:
 * - See test_context_store_restore() function
 * - Reset MCU
 * - RELAUNCH this test after mcu reset
 * - Check if read after reset = write before reset
 *
 * @return bool True if test is successful
 */
static bool porting_test_flash( void )
{
    bool ret = true;

    LOG_INF( "---------------------------------------- %s :", __func__ );
    LOG_INF_COLOR( " !! TEST TO BE LAUNCH TWICE !! To check writing after MCU reset", HAL_DBG_TRACE_COLOR_BLUE );

    /* LORAWAN */
    ret = test_context_store_restore( CONTEXT_LORAWAN_STACK );
    if( ret == false )
    {
        return ret;
    }

    /* MODEM */
    ret = test_context_store_restore( CONTEXT_MODEM );
    if( ret == false )
    {
        return ret;
    }

    /* MODEM KEY */
    ret = test_context_store_restore( CONTEXT_KEY_MODEM );
    if( ret == false )
    {
        return ret;
    }

    /* SECURE ELEMENT */
    ret = test_context_store_restore( CONTEXT_SECURE_ELEMENT );
    if( ret == false )
    {
        return ret;
    }

    return ret;
}
#endif
/*
 * -----------------------------------------------------------------------------
 * --- IRQ CALLBACK DEFINITIONS ---------------------------------------------------------
 */

/**
 * @brief Radio tx irq callback
 */
static void radio_tx_irq_callback( void* obj )
{
    UNUSED( obj );

    irq_time_ms = smtc_modem_hal_get_time_in_ms( );

    radio_irq_raised = true;

    if( ral_clear_irq_status( &( modem_radio.ral ), RAL_IRQ_ALL ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_clear_irq_status() function failed" );
    }
}

/**
 * @brief Radio rx irq callback (get time in ms)
 */
static void radio_rx_irq_callback( void* obj )
{
    UNUSED( obj );

    ral_irq_t radio_irq = 0;

    irq_time_ms      = smtc_modem_hal_get_time_in_ms( );
    radio_irq_raised = true;

    if( ral_get_irq_status( &( modem_radio.ral ), &radio_irq ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( "ral_get_irq_status() function failed" );
    }

    if( ( radio_irq & RAL_IRQ_RX_TIMEOUT ) == RAL_IRQ_RX_TIMEOUT )
    {
        irq_rx_timeout_raised = true;
    }

    if( ral_clear_irq_status( &( modem_radio.ral ), RAL_IRQ_ALL ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_clear_irq_status() function failed" );
    }

    /* Shut Down the TCXO */
    smtc_modem_hal_stop_radio_tcxo( );
}

/**
 * @brief Radio irq callback (get time in s)
 */
static void radio_irq_callback_get_time_in_s( void* obj )
{
    UNUSED( obj );
    ral_irq_t radio_irq = 0;

    irq_time_s       = smtc_modem_hal_get_time_in_s( );
    radio_irq_raised = true;

    if( ral_get_irq_status( &( modem_radio.ral ), &radio_irq ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_get_irq_status() function failed" );
    }

    if( ( radio_irq & RAL_IRQ_RX_TIMEOUT ) == RAL_IRQ_RX_TIMEOUT )
    {
        irq_rx_timeout_raised = true;
    }

    if( ral_clear_irq_status( &( modem_radio.ral ), RAL_IRQ_ALL ) != RAL_STATUS_OK )
    {
        PORTING_TEST_MSG_NOK( " ral_clear_irq_status() function failed" );
    }

    /* Shut Down the TCXO */
    smtc_modem_hal_stop_radio_tcxo( );
}

/**
 * @brief Timer irq callback
 */
static void timer_irq_callback( void* obj )
{
    UNUSED( obj );
    irq_time_ms      = smtc_modem_hal_get_time_in_ms( );
    timer_irq_raised = true;
}

/* --- EOF ------------------------------------------------------------------ */
