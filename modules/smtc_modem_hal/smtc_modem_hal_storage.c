/**
 * @file      smtc_modem_hal_storage.c
 *
 * @brief     smtc_modem_hal_storage HAL implementation
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
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>

#include <smtc_modem_hal.h>
#include <zephyr/lorawan_lbm/lorawan_hal_init.h>

#ifdef CONFIG_USP
LOG_MODULE_DECLARE( lorawan_hal, CONFIG_USP_LOG_LEVEL );
#elif CONFIG_LORA_BASICS_MODEM
LOG_MODULE_DECLARE( lorawan_hal, CONFIG_LORA_BASICS_MODEM_LOG_LEVEL );
#endif

#ifdef CONFIG_LORA_BASICS_MODEM_USER_STORAGE_IMPL
static struct lorawan_user_storage_cb* user_storage_cb;
#endif

#ifdef CONFIG_LORA_BASICS_MODEM_PROVIDED_STORAGE_IMPL

/* NOTE: That whole storage implementation uses direct flash access on the storage_partition
 * from Zephyr instead of leaving it for NVS.
 * Users are expected to provide `chosen/lora-basics-modem-context-partition` to prevent that.
 *
 * A second, more generic implementation via user-provided callbacks was started, with the goal
 * to use the Zephyr NVS APIs, but the store-and-forward internal code from LBM prevents
 * a simple use of this API, it is thus not complete nor working.
 */

#if DT_HAS_CHOSEN( lora_basics_modem_context_partition )
#define CONTEXT_PARTITION DT_FIXED_PARTITION_ID( DT_CHOSEN( lora_basics_modem_context_partition ) )
#else
#define CONTEXT_PARTITION FIXED_PARTITION_ID( storage_partition )
#endif

const struct flash_area* context_flash_area;

/* ------------ Private variables ------------ */

__noinit static uint8_t          crashlog_buff_noinit[CRASH_LOG_SIZE];
__noinit static volatile uint8_t crashlog_length_noinit;
__noinit static volatile bool    crashlog_available_noinit;

// As we are slightly size-limited by Zephyr default flash partitioning, all context offsets are on the same page.
#define ADDR_LORAWAN_CONTEXT_OFFSET 0
#define ADDR_MODEM_KEY_CONTEXT_OFFSET 256
#define ADDR_MODEM_CONTEXT_OFFSET 512
#define ADDR_SECURE_ELEMENT_CONTEXT_OFFSET 768
// #define ADDR_CRASHLOG_CONTEXT_OFFSET 4096
#define ADDR_FUOTA_CONTEXT_OFFSET 4096
#define ADDR_STORE_AND_FORWARD_CONTEXT_OFFSET 8192

static void flash_init( void )
{
    if( context_flash_area )
    {
        return;
    }
    int err = flash_area_open( CONTEXT_PARTITION, &context_flash_area );

    if( err != 0 )
    {
        LOG_ERR( "Could not open flash area for context (%d)", err );
    }
    LOG_INF( "Opened flash area - size %d bytes", context_flash_area->fa_size );
}

static uint32_t priv_hal_context_address( const modem_context_type_t ctx_type, uint32_t offset )
{
    switch( ctx_type )
    {
    case CONTEXT_LORAWAN_STACK:
        return ADDR_LORAWAN_CONTEXT_OFFSET + offset;
    case CONTEXT_KEY_MODEM:
        return ADDR_MODEM_KEY_CONTEXT_OFFSET + offset;
    case CONTEXT_MODEM:
        return ADDR_MODEM_CONTEXT_OFFSET + offset;
    case CONTEXT_SECURE_ELEMENT:
        return ADDR_SECURE_ELEMENT_CONTEXT_OFFSET + offset;
    case CONTEXT_FUOTA:
        return ADDR_FUOTA_CONTEXT_OFFSET + offset;
    case CONTEXT_STORE_AND_FORWARD:
        return ADDR_STORE_AND_FORWARD_CONTEXT_OFFSET + offset;
    }
    k_oops( );
    CODE_UNREACHABLE;
}

void smtc_modem_hal_context_restore( const modem_context_type_t ctx_type, uint32_t offset, uint8_t* buffer,
                                     const uint32_t size )
{
    const uint32_t real_offset = priv_hal_context_address( ctx_type, offset );

    flash_init( );
    flash_area_read( context_flash_area, real_offset, buffer, size );
}

/* NOTE: We take here the maximum erase size out there to do read-erase-write */
// #define PAGE_BUFFER_SIZE 4096
#define PAGE_BUFFER_SIZE                                     \
    DT_PROP_OR( DT_CHOSEN( zephyr_flash ), erase_block_size, \
                4096 )  // Should match sector (page) size for each different architectures as 'zephyr, flash' is almost
                        // always used

// For the min erase size in bytes, we use write_block_size property
// Defaults to 8 if not present
#define MIN_FLASH_WRITE_SIZE_BYTES DT_PROP_OR( DT_CHOSEN( zephyr_flash ), write_block_size, 8 )

static uint8_t page_buffer[PAGE_BUFFER_SIZE];

/* This API allows safe unaligned writes to multiple pages of flash.
 */

static void flash_read_modify_write( uint32_t offset, const uint8_t* buffer, const uint32_t size )
{
    const struct device*    flash_device;
    struct flash_pages_info info;
    uint32_t                remaining = size;

    flash_device = flash_area_get_device( context_flash_area );

    do
    {
        uint32_t offset_in_data = size - remaining;

        memset( page_buffer, 0xFF, PAGE_BUFFER_SIZE );

        /* Find out the page information in the flash */
        flash_get_page_info_by_offs( flash_device, context_flash_area->fa_off + offset + offset_in_data, &info );
        uint32_t page_offset_in_fa = info.start_offset - context_flash_area->fa_off;

        /* Read the whole page */
        flash_area_read( context_flash_area, page_offset_in_fa, page_buffer, PAGE_BUFFER_SIZE );

        /* Fill the data in the buffer */
        uint32_t offset_in_page = offset + offset_in_data - page_offset_in_fa;
        uint32_t length         = MIN( remaining, PAGE_BUFFER_SIZE - offset_in_page );

        memcpy( page_buffer + offset_in_page, buffer + offset_in_data, length );

        /* Erase before write */
        flash_area_erase( context_flash_area, page_offset_in_fa, PAGE_BUFFER_SIZE );

        /* Write the whole page */
        flash_area_write( context_flash_area, page_offset_in_fa, page_buffer, PAGE_BUFFER_SIZE );
        remaining -= length;

    } while( remaining > 0 );
}

void smtc_modem_hal_context_store( const modem_context_type_t ctx_type, uint32_t offset, const uint8_t* buffer,
                                   const uint32_t size )
{
    const uint32_t real_offset = priv_hal_context_address( ctx_type, offset );

    flash_init( );

    // As we are limited in size, all first contexts are placed on the same page
    // And read_modify_write is needed for FUOTA fragments
    // Implement a mechanism using flash sector (page) size instead of raw value, to accomodate every arch
    if( real_offset < ADDR_STORE_AND_FORWARD_CONTEXT_OFFSET )
    {
        flash_read_modify_write( real_offset, buffer, size );
    }
    else
    {
        /* Workaround because some 4-bytes writes will come while flash supports only 8 */
        if( size < MIN_FLASH_WRITE_SIZE_BYTES )
        {
            // If size is less than supported, we pad with 0xFF.
            uint8_t tmp[MIN_FLASH_WRITE_SIZE_BYTES];
            memcpy( tmp, buffer, size );
            for( uint8_t i = size; i < MIN_FLASH_WRITE_SIZE_BYTES; i++ )
            {
                tmp[i] = 0xFF;
            }
            flash_area_write( context_flash_area, real_offset, tmp, MIN_FLASH_WRITE_SIZE_BYTES );
        }
        else
        {
            // const uint32_t real_size = ROUND_UP( size, 8 );
            // flash_read_modify_write( real_offset, buffer, real_size );
            flash_area_write( context_flash_area, real_offset, buffer, size );
        }
    }
}

/* NOTE: We assume that erases are aligned on sectors */
void smtc_modem_hal_context_flash_pages_erase( const modem_context_type_t ctx_type, uint32_t offset, uint8_t nb_page )
{
    const uint32_t real_offset = priv_hal_context_address( ctx_type, offset );

    flash_init( );
    flash_area_erase( context_flash_area, real_offset, smtc_modem_hal_flash_get_page_size( ) * nb_page );
}

uint16_t smtc_modem_hal_flash_get_page_size( void )
{
    const struct device*    flash_device;
    struct flash_pages_info info;

    flash_init( );
    flash_device = flash_area_get_device( context_flash_area );
    flash_get_page_info_by_offs( flash_device, ADDR_STORE_AND_FORWARD_CONTEXT_OFFSET, &info );
    return info.size;
}

uint16_t smtc_modem_hal_store_and_forward_get_number_of_pages( void )
{
    uint16_t page_size;
    size_t   flash_size;
    uint16_t pages_possible;

    page_size  = smtc_modem_hal_flash_get_page_size( );
    flash_size = context_flash_area->fa_size;

    /* 8192B are taken by contexts before store_and_forward */
    pages_possible = ( flash_size - ADDR_STORE_AND_FORWARD_CONTEXT_OFFSET ) / page_size;

    return pages_possible;
}

/* ------------ crashlog management ------------*/

void smtc_modem_hal_crashlog_store( const uint8_t* crash_string, uint8_t crash_string_length )
{
    crashlog_length_noinit = MIN( crash_string_length, CRASH_LOG_SIZE );
    memcpy( crashlog_buff_noinit, crash_string, crashlog_length_noinit );
    crashlog_available_noinit = true;
}

void smtc_modem_hal_crashlog_restore( uint8_t* crash_string, uint8_t* crash_string_length )
{
    *crash_string_length = ( crashlog_length_noinit > CRASH_LOG_SIZE ) ? CRASH_LOG_SIZE : crashlog_length_noinit;
    memcpy( crash_string, crashlog_buff_noinit, *crash_string_length );
}

void smtc_modem_hal_crashlog_set_status( bool available )
{
    crashlog_available_noinit = available;
}

static volatile bool temp = 0x1;  // solve new behaviour introduce with gcc11 compilo
bool                 smtc_modem_hal_crashlog_get_status( void )
{
    bool temp2 = crashlog_available_noinit & temp;
    return temp2;
}

// static int crashlog_status = -1;

// void smtc_modem_hal_crashlog_store( const uint8_t* crashlog, uint8_t crash_string_length )
// {
//     // CRASHLOG_CONTEXT[0] is crashlog_available_noinit in bare-metal stack
//     // CRASHLOG_CONTEXT[1] is crashlog_length_noinit in bare-metal stack
//     uint8_t crashlog_length = MIN( crash_string_length, CRASH_LOG_SIZE );
//     flash_init( );

//     memset( page_buffer, 0, PAGE_BUFFER_SIZE );
//     page_buffer[0] = 1;  // Crashlog available
//     page_buffer[1] = crashlog_length;
//     memcpy( page_buffer + 2, crashlog, crashlog_length );

//     flash_area_erase( context_flash_area, ADDR_CRASHLOG_CONTEXT_OFFSET, PAGE_BUFFER_SIZE );
//     flash_area_write( context_flash_area, ADDR_CRASHLOG_CONTEXT_OFFSET, page_buffer, PAGE_BUFFER_SIZE );
// }

// void smtc_modem_hal_crashlog_restore( uint8_t* crashlog, uint8_t* crash_string_length )
// {
//     // CRASHLOG_CONTEXT[0] is crashlog_available_noinit in bare-metal stack
//     // CRASHLOG_CONTEXT[1] is crashlog_length_noinit in bare-metal stack
//     flash_init( );
//     flash_area_read( context_flash_area, ADDR_CRASHLOG_CONTEXT_OFFSET, page_buffer, PAGE_BUFFER_SIZE );

//     // uint8_t available       = page_buffer[0];  // Why not check crashlog_status here ?
//     uint8_t crashlog_length = page_buffer[1];

//     // crashlog[0] = 0;

//     if( crashlog_status == 1 )
//     {
//         memcpy( crashlog, page_buffer + 2, crashlog_length );
//         *crash_string_length =
//             ( crashlog_length > CRASH_LOG_SIZE ) ? CRASH_LOG_SIZE : crashlog_length;  // Should not be necessary
//     }
//     else
//     {
//         // If crashlog isn't stored ans still requested, return len = 0 bytes
//         *crash_string_length = 0;
//     }
// }

// void smtc_modem_hal_crashlog_set_status( bool available )
// {
//     // CRASHLOG_CONTEXT[0] is crashlog_available_noinit
//     // CRASHLOG_CONTEXT[1] is crashlog_length_noinit
//     flash_init( );
//     // If not available, only erase page
//     if( !available )
//     {
//         flash_area_erase( context_flash_area, ADDR_CRASHLOG_CONTEXT_OFFSET, smtc_modem_hal_flash_get_page_size( ) );
//     }
//     crashlog_status = available ? 1 : 0;
// }

// bool smtc_modem_hal_crashlog_get_status( void )
// {
//     if( crashlog_status == -1 )
//     {
//         flash_init( );
//         flash_area_read( context_flash_area, ADDR_CRASHLOG_CONTEXT_OFFSET, page_buffer, 1 );
//         /* Any other state might mean uninitialized flash area */
//         bool available = ( page_buffer[0] == 1 );

//         crashlog_status = available ? 1 : 0;
//     }

//     /* Any other state might mean uninitialized flash area */
//     return crashlog_status == 1;
// }

#endif /* CONFIG_LORA_BASICS_MODEM_PROVIDED_STORAGE_IMPL */

#ifdef CONFIG_LORA_BASICS_MODEM_USER_STORAGE_IMPL

/* As said in the comment at the beginning of the file, this implementation is neither
 * finished nor working. This is because the store-and-forward implementation expects
 * raw flash accesses, and "CONTEXT_*" have variable / unsure sizes.
 */

void lorawan_register_user_storage_callbacks( struct lorawan_user_storage_cb* cb )
{
    user_storage_cb = cb;
}

/*
 * CONTEXT_MODEM -> size = 16
 * CONTEXT_KEY_MODEM -> size = 36 bytes
 * CONTEXT_LORAWAN_STACK -> size = 32
 * CONTEXT_FUOTA -> variable size
 * CONTEXT_SECURE_ELEMENT -> size = 24 or 483 (!!)
 * CONTEXT_STORE_AND_FORWARD -> garbage
 */

void smtc_modem_hal_context_restore( const modem_context_type_t ctx_type, uint32_t offset, uint8_t* buffer,
                                     const uint32_t size )
{
    user_storage_cb->context_restore( ctx_type, offset, buffer, size );
}

void smtc_modem_hal_context_store( const modem_context_type_t ctx_type, uint32_t offset, const uint8_t* buffer,
                                   const uint32_t size )
{
    user_storage_cb->context_store( ctx_type, offset, buffer, size );
}

#define CRASH_LOG_ID 0xFE
#define CRASH_LOG_STATUS_ID ( CRASH_LOG_ID + 1 )

void smtc_modem_hal_crashlog_store( const uint8_t* crashlog, uint8_t crash_string_length )
{
    /* We use 0xFF as the ID so we do not overwrite any of the valid contexts */
    user_storage_cb->context_store( CRASH_LOG_ID, 0, crashlog, crash_string_length );
}

void smtc_modem_hal_crashlog_restore( uint8_t* crashlog, uint8_t* crash_string_length )
{
    user_storage_cb->context_restore( CRASH_LOG_ID, 0, crashlog, *crash_string_length );
}

void smtc_modem_hal_crashlog_set_status( bool available )
{
    user_storage_cb->context_store( CRASH_LOG_STATUS_ID, 0, ( uint8_t* ) &available, sizeof( available ) );
}

bool smtc_modem_hal_crashlog_get_status( void )
{
    bool available;

    user_storage_cb->context_restore( CRASH_LOG_STATUS_ID, 0, ( uint8_t* ) &available, sizeof( available ) );
    return available;
}

/* NOTE: only used in store-and-forward */
void smtc_modem_hal_context_flash_pages_erase( const modem_context_type_t ctx_type, uint32_t offset, uint8_t nb_page )
{
}

uint16_t smtc_modem_hal_flash_get_page_size( void )
{
    return 0;
}

uint16_t smtc_modem_hal_store_and_forward_get_number_of_pages( void )
{
    return 0;
}

#endif
