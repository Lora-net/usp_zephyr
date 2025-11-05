/**
 * @file      smtc_modem_hal.c
 *
 * @brief     smtc_modem_hal HAL implementation
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

#include <stdarg.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/usp/lora_lbm_transceiver.h>

#include <smtc_modem_hal.h>
#include <smtc_modem_utilities.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>

#if defined(CONFIG_USP)
#include <smtc_rac_api.h>
#endif
#if defined( CONFIG_LORA_BASICS_MODEM )
#include <smtc_modem_utilities.h>
#endif

#ifdef CONFIG_USP
LOG_MODULE_REGISTER(lorawan_hal, CONFIG_USP_LOG_LEVEL);
#elif CONFIG_LORA_BASICS_MODEM
LOG_MODULE_REGISTER(lorawan_hal, CONFIG_LORA_BASICS_MODEM_LOG_LEVEL);
#endif

/* ------------ Local context ------------ */

/* transceiver device pointer */
static const struct device *prv_transceiver_dev;

/* External callbacks */
static lorawan_battery_level_cb_t battery_level_cb;
static lorawan_battery_voltage_cb_t battery_voltage_cb;
static lorawan_temperature_cb_t temperature_cb;
#ifdef CONFIG_LORA_BASICS_MODEM_FUOTA
static struct lorawan_fuota_cb *fuota_cb;
#endif

/* A binary semaphore to notify the main LBM loop */
static K_SEM_DEFINE(lbm_main_loop_sem, 0, 1);

/* context and callback for modem_hal_timer */
static void *prv_smtc_modem_hal_timer_context;
static void (*prv_smtc_modem_hal_timer_callback)(void *context);

/* flag for enabling/disabling timer interrupt. This is set by the libraray during "critical"
 * sections
 */
static bool prv_modem_irq_enabled = true;
static bool prv_modem_irq_pending_while_disabled;
static bool prv_radio_irq_pending_while_disabled;

/* The timer and work used for the modem_hal_timer */
static void prv_smtc_modem_hal_timer_handler(struct k_timer *timer);
static K_TIMER_DEFINE(prv_smtc_modem_hal_timer, prv_smtc_modem_hal_timer_handler, NULL);

/* context and callback for the event pin interrupt */
static void *prv_smtc_modem_hal_radio_irq_context;
static void (*prv_smtc_modem_hal_radio_irq_callback)(void *context);

/* ------------ Initialization ------------
 *
 * This function is defined in lorawan_hal_init.h
 * and is used to set everything up in here.
 */

void lorawan_smtc_modem_hal_init(const struct device *transceiver)
{
    __ASSERT(transceiver, "transceiver must be provided");
    prv_transceiver_dev = transceiver;
#if defined(CONFIG_USP)
    smtc_rac_set_radio_context(prv_transceiver_dev); // Driver HAL implementation
#endif
#if defined( CONFIG_LORA_BASICS_MODEM ) || defined( CONFIG_USP_LORA_BASICS_MODEM )
    smtc_modem_set_radio_context(prv_transceiver_dev);
#endif
}

/* ------------ System management ------------ */

void smtc_modem_hal_reset_mcu(void)
{
    LOG_WRN("Resetting the MCU");
#if defined(CONFIG_LOG)
    log_panic(); /* To flush the logs */
#endif
    k_msleep(100);
    sys_reboot(SYS_REBOOT_COLD);
}

void smtc_modem_hal_reload_wdog(void)
{
    /* This is only provided for internal debugging purposes, so it was
     * decided not to implement it in the Zephyr port.
     */
}

uint32_t smtc_modem_hal_get_time_in_s(void)
{
    return k_uptime_seconds();
}

uint32_t smtc_modem_hal_get_time_in_ms(void)
{
    /* The wrapping every 49 days is expected by the modem lib */
    return k_uptime_get_32();
}

void smtc_modem_hal_set_offset_to_test_wrapping(const uint32_t offset_to_test_wrapping)
{
    /* This aims to add a virtual offset to values returned by smtc_modem_hal_get_time_in_ms.
     * This is only provided for internal development purposes, so it was
     * decided not to implement it in the Zephyr port.
     */
}

void smtc_modem_hal_interruptible_msleep(k_timeout_t timeout)
{
    /* Sleep until we are notified by smtc_modem_hal_wake_up(). */
    k_sem_take(&lbm_main_loop_sem, timeout);
}

void smtc_modem_hal_wake_up(void)
{
    /* Notify the main loop if it's sleeping */
    k_sem_give(&lbm_main_loop_sem);
}

void smtc_modem_hal_user_lbm_irq(void)
{
    smtc_modem_hal_wake_up();
}

struct k_sem * smtc_modem_hal_get_event_sem(void)
{
    return &lbm_main_loop_sem;
}


void smtc_modem_hal_protect_api_call( void )
{
    // Do nothing in case implementation is bare metal
}

void smtc_modem_hal_unprotect_api_call( void )
{
    // Do nothing in case implementation is bare metal
}

/* ------------ Timer management ------------ */

/**
 * @brief Called when the prv_smtc_modem_hal_timer expires.
 *
 * Submits the prv_smtc_modem_hal_timer_work to be handle the callback.
 */
static void prv_smtc_modem_hal_timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    if( prv_modem_irq_enabled )
    {
        prv_smtc_modem_hal_timer_callback(prv_smtc_modem_hal_timer_context);
    }
    else
    {
        prv_modem_irq_pending_while_disabled = true;
    }
};

void smtc_modem_hal_start_timer( const uint32_t milliseconds, void ( *callback )( void* context ), void* context )
{
    prv_smtc_modem_hal_timer_callback = callback;
    prv_smtc_modem_hal_timer_context = context;

    /* start one-shot timer */
    k_timer_start(&prv_smtc_modem_hal_timer, K_MSEC(milliseconds), K_NO_WAIT);
}

void smtc_modem_hal_stop_timer(void)
{
    k_timer_stop(&prv_smtc_modem_hal_timer);
}

/* ------------ IRQ management ------------ */

void smtc_modem_hal_disable_modem_irq(void)
{
    prv_modem_irq_enabled = false;
}

void smtc_modem_hal_enable_modem_irq(void)
{
    prv_modem_irq_enabled = true;
    lora_transceiver_board_enable_interrupt(prv_transceiver_dev);

    if( prv_radio_irq_pending_while_disabled )
    {
        prv_radio_irq_pending_while_disabled = false;
        prv_smtc_modem_hal_radio_irq_callback(prv_smtc_modem_hal_radio_irq_context);
    }
    if( prv_modem_irq_pending_while_disabled )
    {
        prv_modem_irq_pending_while_disabled = false;
        prv_smtc_modem_hal_timer_callback(prv_smtc_modem_hal_timer_context);
    }
}

/* ------------ Panic management ------------ */

void smtc_modem_hal_on_panic(uint8_t *func, uint32_t line, const char *fmt, ...)
{
    const size_t buffer_size = 255;
    uint8_t buffer[buffer_size];
    uint8_t length;
    va_list args;

    /* NOTE: uint8_t *func parameter is actually __func__ casted to uint8_t*,
     * so it can be safely printed with %s
     */
    length = snprintf((char *)buffer, buffer_size, "%s:%u ", func, line);
    va_start(args, fmt);
    length += vsnprintf((char *)buffer + length, buffer_size - length, fmt, args);
    va_end(args);

    LOG_ERR("Modem panic: %s", buffer);

    /* NOTE: smtc_modem_hal_crashlog_set_status(true) is done by
     * smtc_modem_hal_crashlog_store() for simplicity of flash usage
     */
    smtc_modem_hal_crashlog_store( buffer, length );

    smtc_modem_hal_reset_mcu();
}

/* ------------ Random management ------------ */

uint32_t smtc_modem_hal_get_random_nb_in_range(const uint32_t val_1, const uint32_t val_2)
{
    /* Implementation copied from the lbm_examples */
    uint32_t min = MIN(val_1, val_2);
    uint32_t max = MAX(val_1, val_2);
    uint32_t range = (max - min + 1);

    /* Fix cases when val1=0, val2=UINT32_MAX */
    range = range ? range : UINT32_MAX;
    return (uint32_t)((sys_rand32_get() % range) + min);
}

/* ------------ Radio env management ------------ */

/**
 * @brief Called when the transceiver event pin interrupt is triggered.
 *
 * If CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_GLOBAL_THREAD=y,
 * this is called in the system workq.
 * If CONFIG_LORA_BASICS_MODEM_DRIVERS_EVENT_TRIGGER_OWN_THREAD=y,
 * this is called in the transceiver event thread.
 *
 * @param[in] dev The transceiver device.
 */
void prv_transceiver_event_cb(const struct device *dev)
{
    if( prv_modem_irq_enabled )
    {
        /* Due to the way the transceiver driver is implemented,
         * this is called from the system workq.
         */
        prv_smtc_modem_hal_radio_irq_callback(prv_smtc_modem_hal_radio_irq_context);
    }
    else
    {
        prv_radio_irq_pending_while_disabled = true;
    }
}

void smtc_modem_hal_irq_config_radio_irq(void (*callback)(void *context), void *context)
{
    /* save callback function and context */
    prv_smtc_modem_hal_radio_irq_context = context;
    prv_smtc_modem_hal_radio_irq_callback = callback;

    /* enable callback via transceiver driver */
    lora_transceiver_board_attach_interrupt(prv_transceiver_dev, prv_transceiver_event_cb);
    lora_transceiver_board_enable_interrupt(prv_transceiver_dev);
}

void smtc_modem_hal_radio_irq_clear_pending(void)
{
    prv_radio_irq_pending_while_disabled = false;
}

bool smtc_modem_external_stack_currently_use_radio(void)
{
    return false;
}

void smtc_modem_hal_start_radio_tcxo(void)
{
}

void smtc_modem_hal_stop_radio_tcxo(void)
{
}

uint32_t smtc_modem_hal_get_radio_tcxo_startup_delay_ms(void)
{
    /* From the porting guide:
     * If the TCXO is configured by the RAL BSP to start up automatically, then the value used
     * here should be the same as the startup delay used in the RAL BSP.
     * https://github.com/Lora-net/SWL2001/blob/master/lbm_lib/PORTING_GUIDE.md#uint32_t-smtc_modem_hal_get_radio_tcxo_startup_delay_ms-void-
     */
    return lora_transceiver_get_tcxo_startup_delay_ms(prv_transceiver_dev);
}

void smtc_modem_hal_set_ant_switch(bool is_tx_on)
{
    /* NOTE: We only support antenna switches managed by the transceiver.
     */
}

/* ------------ Environment management ------------ */

void lorawan_register_battery_level_callback(lorawan_battery_level_cb_t cb)
{
    battery_level_cb = cb;
}

void lorawan_register_battery_voltage_callback(lorawan_battery_voltage_cb_t cb)
{
    battery_voltage_cb = cb;
}

void lorawan_register_temperature_callback(lorawan_temperature_cb_t cb)
{
    temperature_cb = cb;
}

uint8_t smtc_modem_hal_get_battery_level(void)
{
    if( battery_level_cb )
    {
        return battery_level_cb();
    }
    else
    {
        return 255;
    }
}

uint16_t smtc_modem_hal_get_voltage_mv(void)
{
    if( battery_voltage_cb )
    {
        return battery_voltage_cb();
    }
    else
    {
        return 0;
    }
}

int8_t smtc_modem_hal_get_temperature(void)
{
    if( temperature_cb )
    {
        return temperature_cb();
    }
    else
    {
        return -127;
    }
}

/* ------------ Misc ------------ */

int8_t smtc_modem_hal_get_board_delay_ms(void)
{
    /* The wakeup time is probably closer to 0ms than 1ms,
     * but just to be safe:
     */
#if defined(CONFIG_DT_HAS_SEMTECH_LR1121_ENABLED)
    return 2;
#else
    return 1;
#endif
}

/* ------------ FUOTA ------------ */

#if defined(CONFIG_LORA_BASICS_MODEM_FUOTA)

void lorawan_register_fuota_callbacks(struct lorawan_fuota_cb *cb)
{
    fuota_cb = cb;
}

uint32_t smtc_modem_hal_get_hw_version_for_fuota(void)
{
    if( !fuota_cb || !fuota_cb->get_hw_version )
    {
        LOG_WRN("Call to unimplemented get_hw_version_for_fuota");
        return 0;
    }
    else
    {
        return fuota_cb->get_hw_version();
    }
}

uint32_t smtc_modem_hal_get_fw_version_for_fuota(void)
{
    if( !fuota_cb || !fuota_cb->get_fw_version )
    {
        LOG_WRN("Call to unimplemented get_fw_version_for_fuota");
        return 0;
    }
    else
    {
        return fuota_cb->get_fw_version();
    }
}

uint8_t smtc_modem_hal_get_fw_status_available_for_fuota(void)
{
    if( !fuota_cb || !fuota_cb->get_fw_status_available )
    {
        LOG_WRN("Call to unimplemented get_fw_status_available_for_fuota");
        return 0;
    }
    else
    {
        return fuota_cb->get_fw_status_available();
    }
}

uint32_t smtc_modem_hal_get_next_fw_version_for_fuota(void)
{
    if( !fuota_cb || !fuota_cb->get_next_fw_version )
    {
        LOG_WRN("Call to unimplemented get_next_fw_version_for_fuota");
        return 0;
    }
    else
    {
        return fuota_cb->get_next_fw_version();
    }
}

uint8_t smtc_modem_hal_get_fw_delete_status_for_fuota(uint32_t fw_version)
{
    if( !fuota_cb || !fuota_cb->get_fw_delete_status )
    {
        LOG_WRN("Call to unimplemented get_fw_delete_status_for_fuota");
        return 0;
    }
    else
    {
        return fuota_cb->get_fw_delete_status(fw_version);
    }
}
#endif /* USE_FUOTA */
