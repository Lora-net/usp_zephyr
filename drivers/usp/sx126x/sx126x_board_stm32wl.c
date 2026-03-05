/**
 * @file      sx126x_board_stm32wl.c
 *
 * @brief     Zephyr device registration and interrupt wiring for the
 *            STM32WL internal SX126x radio.
 *
 * On STM32WL the radio DIO1 interrupt is delivered via NVIC IRQ 50
 * (SUBGHZ_Radio_IRQn). There are no external GPIO pins to configure.
 *
 * This file replaces sx126x_board.c when CONFIG_SEMTECH_SX126X_STM32WL is set.
 *
 * Copyright (c) 2026 Semtech Corporation
 * SPDX-License-Identifier: Apache-2.0
 * Created by Charles-Henri Hallard (charles@evolyenergy.com)
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/irq.h>
#include <zephyr/version.h>

/* STM32WL EXTI / LL helpers for enabling the SubGHz radio interrupt line */
#include <stm32wlxx_ll_exti.h>

#include <zephyr/usp/lora_lbm_transceiver.h>
#include "sx126x_stm32wl_hal_context.h"

LOG_MODULE_REGISTER( lora_sx126x, CONFIG_LORA_BASICS_MODEM_DRIVERS_LOG_LEVEL );

/* Required by DEVICE_DT_INST_GET() inside IRQ_CONNECT() */
#define DT_DRV_COMPAT semtech_sx126x_stm32wl

#define SX126X_STM32WL_SPI_OPERATION \
    ( SPI_WORD_SET( 8 ) | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB )

/* ---------------------------------------------------------------------------
 * Event / interrupt handling
 * ---------------------------------------------------------------------------
 */

static void sx126x_stm32wl_event_callback( struct sx126x_stm32wl_hal_context_data_t* data )
{
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

#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD
static void sx126x_stm32wl_thread( struct sx126x_stm32wl_hal_context_data_t* data )
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
static void sx126x_stm32wl_work_cb( struct k_work* work )
{
    struct sx126x_stm32wl_hal_context_data_t* data =
        CONTAINER_OF( work, struct sx126x_stm32wl_hal_context_data_t, work );

    if( data->event_interrupt_cb )
    {
        data->event_interrupt_cb( data->sx126x_dev );
    }
}
#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD */

/* ISR for the STM32WL SUBGHZ_Radio IRQ (NVIC IRQ 50) */
static void sx126x_stm32wl_radio_isr( const struct device* dev )
{
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    struct sx126x_stm32wl_hal_context_data_t* data = dev->data;

    /* EXTI line 44 (SubGHz radio DIO1) is level-triggered: it stays
     * asserted until the host sends ClearIrqStatus to the radio over SPI,
     * which happens in the USP thread (smtc_modem_run_engine).  If we
     * return from this ISR with DIO1 still HIGH the NVIC immediately
     * re-enters it, creating an IRQ storm that starves the USP thread.
     *
     * Fix: disable IRQ 50 here.  smtc_modem_hal_enable_modem_irq() →
     * lora_transceiver_board_enable_interrupt() will re-enable it after
     * the LBM has cleared the radio IRQ register and DIO1 goes LOW. */
    irq_disable( DT_IRQ_BY_IDX( DT_INST( 0, semtech_sx126x_stm32wl ), 0, irq ) );

    sx126x_stm32wl_event_callback( data );
#endif
}

/* ---------------------------------------------------------------------------
 * Board API (called by the USP RAC layer)
 * ---------------------------------------------------------------------------
 */

void lora_transceiver_board_attach_interrupt( const struct device* dev, event_cb_t cb )
{
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    struct sx126x_stm32wl_hal_context_data_t* data = dev->data;

    data->event_interrupt_cb = cb;
#else
    LOG_ERR( "Event trigger not supported!" );
#endif
}

void lora_transceiver_board_enable_interrupt( const struct device* dev )
{
    irq_enable( DT_IRQ_BY_IDX( DT_INST( 0, semtech_sx126x_stm32wl ), 0, irq ) );
}

void lora_transceiver_board_disable_interrupt( const struct device* dev )
{
    irq_disable( DT_IRQ_BY_IDX( DT_INST( 0, semtech_sx126x_stm32wl ), 0, irq ) );
}

uint32_t lora_transceiver_get_tcxo_startup_delay_ms( const struct device* dev )
{
    const struct sx126x_stm32wl_hal_context_cfg_t* config = dev->config;

    return config->tcxo_cfg.wakeup_time_ms;
}

/* ---------------------------------------------------------------------------
 * Driver init
 * ---------------------------------------------------------------------------
 */

static int sx126x_stm32wl_init( const struct device* dev )
{
    const struct sx126x_stm32wl_hal_context_cfg_t* config = dev->config;
    struct sx126x_stm32wl_hal_context_data_t*      data   = dev->data;
    int                                            ret    = 0;

    if( !device_is_ready( config->spi.bus ) )
    {
        LOG_ERR( "subghzspi bus not ready" );
        return -EINVAL;
    }

    /* Configure external RF switch GPIOs (both idle/low at startup) */
    if( config->tx_rf_switch.port != NULL )
    {
        if( !gpio_is_ready_dt( &config->tx_rf_switch ) )
        {
            LOG_ERR( "TX RF switch GPIO not ready" );
            return -ENODEV;
        }
        gpio_pin_configure_dt( &config->tx_rf_switch, GPIO_OUTPUT_INACTIVE );
    }
    if( config->rx_rf_switch.port != NULL )
    {
        if( !gpio_is_ready_dt( &config->rx_rf_switch ) )
        {
            LOG_ERR( "RX RF switch GPIO not ready" );
            return -ENODEV;
        }
        gpio_pin_configure_dt( &config->rx_rf_switch, GPIO_OUTPUT_INACTIVE );
    }

    data->radio_status               = RADIO_AWAKE;
    data->tx_power_offset_db_current = config->tx_power_offset_db;

#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER
    data->sx126x_dev = dev;

#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD
    data->work.handler = sx126x_stm32wl_work_cb;
#endif
#ifdef CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD
    k_sem_init( &data->gpio_sem, 0, K_SEM_MAX_LIMIT );
    k_thread_create( &data->thread, data->thread_stack,
                     CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_THREAD_STACK_SIZE,
                     ( k_thread_entry_t ) sx126x_stm32wl_thread, data, NULL, NULL,
                     K_PRIO_COOP( CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_THREAD_PRIORITY ),
                     0, K_NO_WAIT );
#endif

    /* Connect the SUBGHZ_Radio IRQ (line 44 in the EXTI block, NVIC IRQ 50).
     * LL_EXTI_LINE_44 enables the wakeup-from-sleep path for the radio. */
    IRQ_CONNECT( DT_IRQ_BY_IDX( DT_INST( 0, semtech_sx126x_stm32wl ), 0, irq ),
                 DT_IRQ_BY_IDX( DT_INST( 0, semtech_sx126x_stm32wl ), 0, priority ),
                 sx126x_stm32wl_radio_isr,
                 DEVICE_DT_INST_GET( 0 ), 0 );

    LL_EXTI_EnableIT_32_63( LL_EXTI_LINE_44 );
    irq_enable( DT_IRQ_BY_IDX( DT_INST( 0, semtech_sx126x_stm32wl ), 0, irq ) );

#endif /* CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER */

    return ret;
}

/* ---------------------------------------------------------------------------
 * PM action (placeholder — power management is handled by LBM itself)
 * ---------------------------------------------------------------------------
 */

#if IS_ENABLED( CONFIG_PM_DEVICE )
static int sx126x_stm32wl_pm_action( const struct device* dev, enum pm_device_action action )
{
    switch( action )
    {
    case PM_DEVICE_ACTION_RESUME:
    case PM_DEVICE_ACTION_SUSPEND:
        break;
    default:
        return -ENOTSUP;
    }
    return 0;
}
#endif

/* ---------------------------------------------------------------------------
 * Device tree instantiation macros
 * ---------------------------------------------------------------------------
 */

#if ZEPHYR_VERSION_CODE < ZEPHYR_VERSION( 4, 3, 0 )
    #define SX126X_STM32WL_SPI_SPEC( node_id ) \
        SPI_DT_SPEC_GET( node_id, SX126X_STM32WL_SPI_OPERATION, 0 )
#else
    #define SX126X_STM32WL_SPI_SPEC( node_id ) \
        SPI_DT_SPEC_GET( node_id, SX126X_STM32WL_SPI_OPERATION )
#endif

/* Derive xosc_cfg: if tcxo-power-startup-delay-ms is absent or 0 → crystal,
 * else TCXO controlled via DIO3. */
#define SX126X_STM32WL_XOSC_CFG( node_id ) \
    ( DT_PROP_OR( node_id, tcxo_power_startup_delay_ms, 0 ) == 0 \
        ? RAL_XOSC_CFG_XTAL \
        : RAL_XOSC_CFG_TCXO_RADIO_CTRL )

#define SX126X_STM32WL_CFG_TCXO( node_id )                                          \
    .tcxo_cfg = {                                                                    \
        .xosc_cfg       = SX126X_STM32WL_XOSC_CFG( node_id ),                       \
        .voltage        = DT_PROP_OR( node_id, dio3_tcxo_voltage, 0 ),               \
        .wakeup_time_ms = DT_PROP_OR( node_id, tcxo_power_startup_delay_ms, 0 ),     \
    }

#define SX126X_STM32WL_CONFIG( node_id )                                              \
    {                                                                                 \
        .spi                  = SX126X_STM32WL_SPI_SPEC( node_id ),                   \
        .tx_rf_switch         = GPIO_DT_SPEC_GET_OR( node_id, tx_enable_gpios, {0} ), \
        .rx_rf_switch         = GPIO_DT_SPEC_GET_OR( node_id, rx_enable_gpios, {0} ), \
        .dio2_tx_enable       = DT_PROP( node_id, dio2_tx_enable ),                   \
        SX126X_STM32WL_CFG_TCXO( node_id ),                                           \
        .capa_xta             = DT_PROP_OR( node_id, xtal_capacitor_value_xta, 0xFF ),\
        .capa_xtb             = DT_PROP_OR( node_id, xtal_capacitor_value_xtb, 0xFF ),\
        /* regulator-ldo is a boolean: present = LDO, absent = DC-DC */               \
        .reg_mode             = DT_PROP( node_id, regulator_ldo )                     \
                                    ? SX126X_REG_MODE_LDO : SX126X_REG_MODE_DCDC,    \
        .tx_power_offset_db   = DT_PROP_OR( node_id, tx_power_offset, 0 ),            \
        .rx_boosted           = DT_PROP_OR( node_id, rx_boosted, false ),             \
        .pa_ramp_time         = DT_PROP_OR( node_id, pa_ramp_time, 0x02 ),            \
        /* DT_ENUM_IDX returns 0 for "rfo-lp" (first enum) and 1 for "rfo-hp" */  \
        .pa_is_lp             = ( DT_ENUM_IDX( node_id, power_amplifier_output ) == 0 ), \
        .pa_lp_max_power_dbm  = DT_PROP_OR( node_id, rfo_lp_max_power, 14 ),          \
        .pa_hp_max_power_dbm  = DT_PROP_OR( node_id, rfo_hp_max_power, 22 ),          \
    }

#define SX126X_STM32WL_DEVICE_INIT( node_id )                                             \
    DEVICE_DT_DEFINE( node_id, sx126x_stm32wl_init, PM_DEVICE_DT_GET( node_id ),          \
                      &sx126x_stm32wl_data_##node_id, &sx126x_stm32wl_config_##node_id,   \
                      POST_KERNEL, CONFIG_LORA_BASICS_MODEM_DRIVERS_INIT_PRIORITY, NULL );

#define SX126X_STM32WL_DEFINE( node_id )                                                          \
    static struct sx126x_stm32wl_hal_context_data_t      sx126x_stm32wl_data_##node_id;           \
    static const struct sx126x_stm32wl_hal_context_cfg_t sx126x_stm32wl_config_##node_id =        \
        SX126X_STM32WL_CONFIG( node_id );                                                         \
    PM_DEVICE_DT_DEFINE( node_id, sx126x_stm32wl_pm_action );                                     \
    SX126X_STM32WL_DEVICE_INIT( node_id )

/* Instantiate one device for every enabled semtech,sx126x-stm32wl node */
DT_FOREACH_STATUS_OKAY( semtech_sx126x_stm32wl, SX126X_STM32WL_DEFINE )
