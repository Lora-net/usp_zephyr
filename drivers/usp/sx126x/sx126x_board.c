/**
 * @file      sx126x_board.c
 *
 * @brief     sx126x_board implementation
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

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#include <zephyr/usp/lora_lbm_transceiver.h>
#include "sx126x_hal_context.h"

LOG_MODULE_REGISTER( lora_sx126x, CONFIG_LORA_BASICS_MODEM_DRIVERS_LOG_LEVEL );

#define SX126X_SPI_OPERATION ( SPI_WORD_SET( 8 ) | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB )

/**
 * @brief Event pin callback handler.
 *
 * @param dev
 * @param cb
 * @param pins
 */
static void sx126x_board_event_callback( const struct device* dev, struct gpio_callback* cb, uint32_t pins,
                                         struct sx126x_hal_context_data_t* data )
{
    /* This code expects to always use EDGE interrupt triggers
     * (so no possible duplicate triggers)
     */

    /* Call provided callback */
#if defined( CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD )
    k_sem_give( &data->gpio_sem );
#elif defined( CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD )
    k_work_submit( &data->work );
#elif defined( CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_NO_THREAD )
    if( data->event_interrupt_cb )
    {
        data->event_interrupt_cb( data->sx126x_dev );
    }
#endif
}

#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
static void sx126x_board_dio1_callback( const struct device* dev, struct gpio_callback* cb, uint32_t pins )
{
    struct sx126x_hal_context_data_t* data = CONTAINER_OF( cb, struct sx126x_hal_context_data_t, dio1_cb );

    return sx126x_board_event_callback( dev, cb, pins, data );
}
static void sx126x_board_dio2_callback( const struct device* dev, struct gpio_callback* cb, uint32_t pins )
{
    struct sx126x_hal_context_data_t* data = CONTAINER_OF( cb, struct sx126x_hal_context_data_t, dio2_cb );

    return sx126x_board_event_callback( dev, cb, pins, data );
}
static void sx126x_board_dio3_callback( const struct device* dev, struct gpio_callback* cb, uint32_t pins )
{
    struct sx126x_hal_context_data_t* data = CONTAINER_OF( cb, struct sx126x_hal_context_data_t, dio3_cb );

    return sx126x_board_event_callback( dev, cb, pins, data );
}
#endif

#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD
static void sx126x_thread( struct sx126x_hal_context_data_t* data )
{
    while( 1 )
    {
        k_sem_take( &data->gpio_sem, K_FOREVER );
        if( data->event_interrupt_cb )
        {
            data->event_interrupt_cb( data->sx126x_dev );
        }
    }
}
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD */

#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD
static void sx126x_work_cb( struct k_work* work )
{
    struct sx126x_hal_context_data_t* data = CONTAINER_OF( work, struct sx126x_hal_context_data_t, work );
    if( data->event_interrupt_cb )
    {
        data->event_interrupt_cb( data->sx126x_dev );
    }
}
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD */

void lora_transceiver_board_attach_interrupt( const struct device* dev, event_cb_t cb )
{
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    struct sx126x_hal_context_data_t* data = dev->data;

    data->event_interrupt_cb = cb;
#else
    LOG_ERR( "Event trigger not supported!" );
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER */
}

void lora_transceiver_board_enable_interrupt( const struct device* dev )
{
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    const struct sx126x_hal_context_cfg_t* config = dev->config;

    if( config->dio1.port )
    {
        gpio_pin_interrupt_configure_dt( &config->dio1, GPIO_INT_EDGE_TO_ACTIVE );
    }
    if( config->dio2.port )
    {
        gpio_pin_interrupt_configure_dt( &config->dio2, GPIO_INT_EDGE_TO_ACTIVE );
    }
    if( config->dio3.port )
    {
        gpio_pin_interrupt_configure_dt( &config->dio3, GPIO_INT_EDGE_TO_ACTIVE );
    }
#else
    LOG_ERR( "Event trigger not supported!" );
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER */
}

void lora_transceiver_board_disable_interrupt( const struct device* dev )
{
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    const struct sx126x_hal_context_cfg_t* config = dev->config;

    if( config->dio1.port )
    {
        gpio_pin_interrupt_configure_dt( &config->dio1, GPIO_INT_DISABLE );
    }
    if( config->dio2.port )
    {
        gpio_pin_interrupt_configure_dt( &config->dio2, GPIO_INT_DISABLE );
    }
    if( config->dio3.port )
    {
        gpio_pin_interrupt_configure_dt( &config->dio3, GPIO_INT_DISABLE );
    }
#else
    LOG_ERR( "Event trigger not supported!" );
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER */
}

uint32_t lora_transceiver_get_tcxo_startup_delay_ms( const struct device* dev )
{
    const struct sx126x_hal_context_cfg_t* config = dev->config;

    return config->tcxo_cfg.wakeup_time_ms;
}

static int sx126x_init( const struct device* dev )
{
    const struct sx126x_hal_context_cfg_t* config = dev->config;
    struct sx126x_hal_context_data_t*      data   = dev->data;
    int                                    ret;

    if( !device_is_ready( config->spi.bus ) )
    {
        LOG_ERR( "Could not find SPI device" );
        return -EINVAL;
    }

    /* Reset pin */
    ret = gpio_pin_configure_dt( &config->reset, GPIO_OUTPUT_INACTIVE );
    if( ret < 0 )
    {
        LOG_ERR( "Could not configure reset gpio" );
        return ret;
    }

    /* Busy pin */
    ret = gpio_pin_configure_dt( &config->busy, GPIO_INPUT );
    if( ret < 0 )
    {
        LOG_ERR( "Could not configure busy gpio" );
        return ret;
    }

    /* DIO1 event pin */
    if( config->dio1.port )
    {
        ret = gpio_pin_configure_dt( &config->dio1, GPIO_INPUT );
        if( ret < 0 )
        {
            LOG_ERR( "Could not configure DIO1 event gpio" );
            return ret;
        }
    }
    /* DIO2 event pin */
    if( config->dio2.port )
    {
        ret = gpio_pin_configure_dt( &config->dio2, GPIO_INPUT );
        if( ret < 0 )
        {
            LOG_ERR( "Could not configure DIO2 event gpio" );
            return ret;
        }
    }
    /* DIO3 event pin */
    if( config->dio3.port )
    {
        ret = gpio_pin_configure_dt( &config->dio3, GPIO_INPUT );
        if( ret < 0 )
        {
            LOG_ERR( "Could not configure DIO3 event gpio" );
            return ret;
        }
    }

    data->radio_status               = RADIO_AWAKE;
    data->tx_power_offset_db_current = config->tx_power_offset_db;

    /* Event pin trigger config */
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    data->sx126x_dev = dev;
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD
    data->work.handler = sx126x_work_cb;
#elif CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD
    k_sem_init( &data->trig_sem, 0, K_SEM_MAX_LIMIT );
    k_thread_create( &data->thread, data->thread_stack,
                     CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_THREAD_STACK_SIZE,
                     ( k_thread_entry_t ) sx126x_thread, data, NULL, NULL,
                     K_PRIO_COOP( CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_THREAD_PRIORITY ), 0, K_NO_WAIT );
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD */

    if( config->dio1.port )
    {
        /* Init callback */
        gpio_init_callback( &data->dio1_cb, sx126x_board_dio1_callback, BIT( config->dio1.pin ) );
        /* Add callback */
        if( gpio_add_callback( config->dio1.port, &data->dio1_cb ) )
        {
            LOG_ERR( "Could not set dio1 pin callback" );
            return -EIO;
        }
    }
    if( config->dio2.port )
    {
        /* Init callback */
        gpio_init_callback( &data->dio2_cb, sx126x_board_dio2_callback, BIT( config->dio2.pin ) );
        /* Add callback */
        if( gpio_add_callback( config->dio2.port, &data->dio2_cb ) )
        {
            LOG_ERR( "Could not set dio2 pin callback" );
            return -EIO;
        }
    }
    if( config->dio3.port )
    {
        /* Init callback */
        gpio_init_callback( &data->dio3_cb, sx126x_board_dio3_callback, BIT( config->dio3.pin ) );
        /* Add callback */
        if( gpio_add_callback( config->dio3.port, &data->dio3_cb ) )
        {
            LOG_ERR( "Could not set dio3 pin callback" );
            return -EIO;
        }
    }
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER */

    return ret;
}

#if IS_ENABLED( CONFIG_PM_DEVICE )
/**
 * @brief Power management action define.
 * Not implemented as LoRa Basics Modem handles this on its side.
 *
 * @param dev
 * @param action
 * @return int
 */
static int sx126x_pm_action( const struct device* dev, enum pm_device_action action )
{
    int ret = 0;

    switch( action )
    {
    case PM_DEVICE_ACTION_RESUME:
        /* Put the lr11xx into normal operation mode */
        break;
    case PM_DEVICE_ACTION_SUSPEND:
        /* Put the lr11xx into sleep mode */
        break;
    default:
        return -ENOTSUP;
    }
    return ret;
}
#endif /* IS_ENABLED(CONFIG_PM_DEVICE) */

/*
 * Device creation macro.
 */

#define DIO2_CONFLICT( node_id ) \
    ( DT_NODE_HAS_PROP( node_id, dio2_gpios ) && DT_NODE_HAS_PROP( node_id, dio2_as_rf_switch ) ) ||

#if DT_FOREACH_STATUS_OKAY( semtech_sx1261_new, DIO2_CONFLICT ) DT_FOREACH_STATUS_OKAY( \
    semtech_sx1262_new, DIO2_CONFLICT ) DT_FOREACH_STATUS_OKAY( semtech_sx1268_new, DIO2_CONFLICT ) 0
#error Device tree properties dio2-gpios and dio2-as-rf-switch are conflicting, \
    please delete one.
#endif

#define DIO3_CONFLICT( node_id ) \
    ( DT_NODE_HAS_PROP( node_id, dio3_gpios ) && DT_NODE_HAS_PROP( node_id, dio3_as_tcxo_control ) ) ||

#if DT_FOREACH_STATUS_OKAY( semtech_sx1261_new, DIO3_CONFLICT ) DT_FOREACH_STATUS_OKAY( \
    semtech_sx1262_new, DIO3_CONFLICT ) DT_FOREACH_STATUS_OKAY( semtech_sx1268_new, DIO3_CONFLICT ) 0
#error Device tree properties dio3-gpios and dio3-as-tcxo-control are conflicting, \
    please delete one.
#endif

#define CONFIGURE_GPIO_IF_IN_DT( node_id, name, dt_prop ) \
    COND_CODE_1( DT_NODE_HAS_PROP( node_id, dt_prop ), (.name = GPIO_DT_SPEC_GET( node_id, dt_prop ), ), ( ) )

#define SX126X_XOSC_CFG( node_id )                                                                          \
    COND_CODE_1( DT_PROP( node_id, tcxo_wakeup_time ) == 0, ( RAL_XOSC_CFG_XTAL ),                          \
                 ( COND_CODE_1( DT_PROP( node_id, dio3_as_tcxo_control ), ( RAL_XOSC_CFG_TCXO_RADIO_CTRL ), \
                                ( RAL_XOSC_CFG_TCXO_EXT_CTRL ) ) ) )

/* Derive dio3-as-tcxo-control to know xosc_cfg */
#define SX126X_CFG_TCXO( node_id )                              \
    .tcxo_cfg = {                                               \
        .xosc_cfg       = SX126X_XOSC_CFG( node_id ),           \
        .voltage        = DT_PROP( node_id, tcxo_voltage ),     \
        .wakeup_time_ms = DT_PROP( node_id, tcxo_wakeup_time ), \
    }

#define SX126X_CONFIG( node_id )                                                                                  \
    {                                                                                                             \
        .spi   = SPI_DT_SPEC_GET( node_id, SX126X_SPI_OPERATION, 0 ),                                             \
        .reset = GPIO_DT_SPEC_GET( node_id, reset_gpios ),                                                        \
        .busy  = GPIO_DT_SPEC_GET( node_id, busy_gpios ),                                                         \
        CONFIGURE_GPIO_IF_IN_DT( node_id, dio1, dio1_gpios ) CONFIGURE_GPIO_IF_IN_DT( node_id, dio2, dio2_gpios ) \
            CONFIGURE_GPIO_IF_IN_DT( node_id, dio3, dio3_gpios )                                                  \
                .dio2_as_rf_switch = DT_PROP( node_id, dio2_as_rf_switch ),                                       \
        SX126X_CFG_TCXO( node_id ),                                                                               \
        .capa_xta           = DT_PROP_OR( node_id, xtal_capacitor_value_xta, 0xFF ),                              \
        .capa_xtb           = DT_PROP_OR( node_id, xtal_capacitor_value_xtb, 0xFF ),                              \
        .reg_mode           = DT_PROP( node_id, reg_mode ),                                                       \
        .tx_power_offset_db = DT_PROP_OR( node_id, tx_power_offset, 0 ),                                          \
        .rx_boosted         = DT_PROP_OR( node_id, rx_boosted, false ),                                           \
        .pa_ramp_time       = DT_PROP_OR( node_id, pa_ramp_time, 0x02 ),                                          \
    }

#define SX126X_DEVICE_INIT( node_id )                                                            \
    DEVICE_DT_DEFINE( node_id, sx126x_init, PM_DEVICE_DT_GET( node_id ), &sx126x_data_##node_id, \
                      &sx126x_config_##node_id, POST_KERNEL, CONFIG_LORA_BASICS_MODEM_DRIVERS_INIT_PRIORITY, NULL );

#define SX126X_DEFINE( node_id )                                                                     \
    static struct sx126x_hal_context_data_t      sx126x_data_##node_id;                              \
    static const struct sx126x_hal_context_cfg_t sx126x_config_##node_id = SX126X_CONFIG( node_id ); \
    PM_DEVICE_DT_DEFINE( node_id, sx126x_pm_action );                                                \
    SX126X_DEVICE_INIT( node_id )

DT_FOREACH_STATUS_OKAY( semtech_sx1261_new, SX126X_DEFINE )
DT_FOREACH_STATUS_OKAY( semtech_sx1262_new, SX126X_DEFINE )
DT_FOREACH_STATUS_OKAY( semtech_sx1268_new, SX126X_DEFINE )
