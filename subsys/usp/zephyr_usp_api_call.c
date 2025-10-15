/**
 * @file      zephyr_usp_api_call.c
 *
 * @brief     zephyr_usp_api_call implementation
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
 #include <zephyr/logging/log.h>

 #include <zephyr/lorawan_lbm/lorawan_hal_init.h>

 #include <smtc_rac_api.h>
#if defined(CONFIG_USP_LORA_BASICS_MODEM)
#include <smtc_modem_utilities.h>
#endif
 /*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */
LOG_MODULE_DECLARE(usp, CONFIG_USP_LOG_LEVEL);

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */


/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE Types -----------------------------------------------------------
 */
typedef enum smtc_zephyr_usp_func_id_e
{
    ZHFI_CORE_INIT,
    ZHFI_OPEN_RADIO,
    ZHFI_LORA,
    ZHFI_ABORT_RADIO_REQUEST,
    ZHFI_CLOSE_RADIO,
    ZHFI_MODEM_INIT
} smtc_zephyr_usp_func_id_t;

typedef struct {
    smtc_rac_priority_t priority;
} smtc_zephyr_usp_func_rac_t;

typedef struct {
    uint8_t radio_access_id;
} smtc_zephyr_usp_func_lora_t;

typedef struct {
    uint8_t radio_access_id;
} smtc_zephyr_usp_func_arr_t;

typedef struct {
    uint8_t radio_access_id;
} smtc_zephyr_usp_func_cr_t;

#if defined(CONFIG_USP_LORA_BASICS_MODEM)
typedef struct {
    void (*callback_event)(void);
} smtc_zephyr_usp_func_mi_t;
#endif

typedef union {
    smtc_zephyr_usp_func_rac_t rac;
    smtc_zephyr_usp_func_lora_t lora;
    smtc_zephyr_usp_func_arr_t abort;
    smtc_zephyr_usp_func_cr_t close;
#if defined(CONFIG_USP_LORA_BASICS_MODEM)
    smtc_zephyr_usp_func_mi_t modem_init;
#endif
} smtc_zephyr_usp_func_args_t;

typedef struct {
    uint8_t func_id;
    smtc_zephyr_usp_func_args_t args;
} smtc_zephyr_usp_func_t;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */
K_MSGQ_DEFINE(usp_func_msgq, sizeof(smtc_zephyr_usp_func_t), 10, 4);

 /*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION (for API) ---------------------------------------------
 */


void zephyr_smtc_rac_core_init()
{
    smtc_zephyr_usp_func_t item = {.func_id = (uint8_t)ZHFI_CORE_INIT};
    if (k_msgq_put(&usp_func_msgq, &item, K_NO_WAIT) != 0) { //SSA : manage queue is full + error codes
    }
    smtc_modem_hal_wake_up();
}

uint8_t zephyr_smtc_rac_open_radio(smtc_rac_priority_t priority)
{
    smtc_zephyr_usp_func_t item = {.func_id = (uint8_t)ZHFI_OPEN_RADIO, .args.rac = {.priority = priority}};
    if (k_msgq_put(&usp_func_msgq, &item, K_NO_WAIT) != 0) { //SSA : manage queue is full + error codes
        return 0xFF;
    }
    smtc_modem_hal_wake_up();
    return priority;
}

smtc_rac_return_code_t zephyr_smtc_rac_submit_radio_transaction(uint8_t radio_access_id)
{
    smtc_zephyr_usp_func_t item = {.func_id = (uint8_t)ZHFI_LORA, .args.lora = {.radio_access_id = radio_access_id}};
    if (k_msgq_put(&usp_func_msgq, &item, K_NO_WAIT) != 0) { //SSA : manage queue is full + error codes
        return SMTC_RAC_ERROR;
     }
    smtc_modem_hal_wake_up();
    return SMTC_RAC_SUCCESS;
}

smtc_rac_return_code_t zephyr_smtc_rac_abort_radio_submit(uint8_t radio_access_id)
{
    smtc_zephyr_usp_func_t item = {.func_id = (uint8_t)ZHFI_ABORT_RADIO_REQUEST, .args.abort = {.radio_access_id = radio_access_id}};
    if (k_msgq_put(&usp_func_msgq, &item, K_NO_WAIT) != 0) { //SSA : manage queue is full + error codes
        return SMTC_RAC_ERROR;
    }
    smtc_modem_hal_wake_up();
    return SMTC_RAC_SUCCESS;
}

smtc_rac_return_code_t zephyr_smtc_rac_close_radio(uint8_t radio_access_id)
{
    smtc_zephyr_usp_func_t item = {.func_id = (uint8_t)ZHFI_CLOSE_RADIO, .args.close = {.radio_access_id = radio_access_id}};
    if (k_msgq_put(&usp_func_msgq, &item, K_NO_WAIT) != 0) { //SSA : manage queue is full + error codes
        return SMTC_RAC_ERROR;
    }
    smtc_modem_hal_wake_up();
    return SMTC_RAC_SUCCESS;
}

#if defined(CONFIG_USP_LORA_BASICS_MODEM)
smtc_rac_return_code_t zephyr_smtc_modem_init(void ( *callback_event )( void ))
{
    smtc_zephyr_usp_func_t item = {.func_id = (uint8_t)ZHFI_MODEM_INIT, .args.modem_init = {.callback_event = callback_event}};
    if (k_msgq_put(&usp_func_msgq, &item, K_NO_WAIT) != 0) { //SSA : manage queue is full + error codes
        return SMTC_RAC_ERROR;
    }
    smtc_modem_hal_wake_up();
    return SMTC_RAC_SUCCESS;
}
#endif

 /*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION (for Zephyr USP/RAC internals only) ---------------------------------------------
 */
void zephyr_smtc_manage_func(void)
{
    smtc_zephyr_usp_func_t usp_func;
    while(k_msgq_get(&usp_func_msgq, &usp_func, K_NO_WAIT) == 0)
    {
        if(usp_func.func_id == (uint8_t)ZHFI_CORE_INIT)
        {
            smtc_rac_init();
        }
        else if(usp_func.func_id == (uint8_t)ZHFI_OPEN_RADIO)
        {
            smtc_rac_open_radio(usp_func.args.rac.priority); //SS-TBD : manage error code
        }
        else if(usp_func.func_id == (uint8_t)ZHFI_LORA)
        {
            smtc_rac_submit_radio_transaction(usp_func.args.lora.radio_access_id);  //SS-TBD : manage error code
        }
        else if(usp_func.func_id == (uint8_t)ZHFI_ABORT_RADIO_REQUEST)
        {
            smtc_rac_abort_radio_submit(usp_func.args.abort.radio_access_id);  //SS-TBD : manage error code
        }
        else if(usp_func.func_id == (uint8_t)ZHFI_CLOSE_RADIO)
        {
            smtc_rac_close_radio(usp_func.args.close.radio_access_id);  //SS-TBD : manage error code
        }
#if defined(CONFIG_USP_LORA_BASICS_MODEM)
        else if(usp_func.func_id == (uint8_t)ZHFI_MODEM_INIT)
        {
            smtc_modem_init(usp_func.args.modem_init.callback_event);
        }
#endif
    }
}


/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DEFINITION --------------------------------------------
 */
