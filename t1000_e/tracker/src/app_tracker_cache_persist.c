/*
 * Copyright (C) 2026 ViVoSofT
 *
 * See app_tracker_cache_persist.h for the design rationale.
 */

#include <string.h>
#include "fds.h"
#include "smtc_hal.h"
#include "app_at_fds_datas.h"
#include "app_tracker_cache.h"
#include "app_tracker_cache_persist.h"

/* Slot record layout in flash: [len:1 byte][data:len bytes], FDS pads
 * to a whole number of words itself. */
#define SLOT_BUF_SIZE   ( 1 + TRACKER_CACHE_MAX_SIZE )

static bool fds_write_slot( uint16_t slot, const uint8_t *data, uint8_t len )
{
    uint8_t buf[SLOT_BUF_SIZE];
    buf[0] = len;
    memcpy( buf + 1, data, len );

    fds_record_t record =
    {
        .file_id = CACHE_CKPT_FILE,
        .key     = slot,
        .data.p_data       = ( char * )buf,
        .data.length_words = ( 1 + len + 3 ) / 4,
    };

    fds_record_desc_t desc = { 0 };
    fds_find_token_t  tok  = { 0 };
    ret_code_t rc;

    if( fds_record_find( CACHE_CKPT_FILE, slot, &desc, &tok ) == NRF_SUCCESS )
    {
        rc = fds_record_update( &desc, &record );
    }
    else
    {
        rc = fds_record_write( &desc, &record );
    }

    /* Matches the wait used by write_record_by_desc()/
     * update_record_by_desc() in app_at_fds_datas.c for the same
     * reason -- give the async FDS event a moment to land. Fine here:
     * this only ever runs once, from app_user_power_off(), well after
     * app_lora_stack_suspend() has already cleared the modem alarm and
     * left the network -- not from a modem event callback or any
     * periodic/ticking context. See app_tracker_cache_persist.h. */
    hal_mcu_wait_ms( 8 );

    return rc == NRF_SUCCESS;
}

static void fds_delete_slot( uint16_t slot )
{
    fds_record_desc_t desc = { 0 };
    fds_find_token_t  tok  = { 0 };

    if( fds_record_find( CACHE_CKPT_FILE, slot, &desc, &tok ) == NRF_SUCCESS )
    {
        fds_record_delete( &desc );
        hal_mcu_wait_ms( 8 );
    }
}

void cache_persist_restore( void )
{
    uint8_t buf[SLOT_BUF_SIZE];

    for( uint16_t slot = 0; slot < CACHE_PERSIST_MAX_SLOTS; slot++ )
    {
        fds_record_desc_t  desc = { 0 };
        fds_find_token_t   tok  = { 0 };
        fds_flash_record_t flash_record = { 0 };

        if( fds_record_find( CACHE_CKPT_FILE, slot, &desc, &tok ) != NRF_SUCCESS )
        {
            break;  /* first gap -- end of the persisted run */
        }
        if( fds_record_open( &desc, &flash_record ) != NRF_SUCCESS )
        {
            break;
        }

        uint16_t byte_len = flash_record.p_header->length_words * sizeof( uint32_t );
        if( byte_len > sizeof( buf ))
        {
            byte_len = sizeof( buf );
        }
        memcpy( buf, flash_record.p_data, byte_len );
        fds_record_close( &desc );

        uint8_t entry_len = buf[0];
        if( entry_len > 0 && entry_len <= TRACKER_CACHE_MAX_SIZE )
        {
            tracker_cache_save( buf + 1, entry_len );
        }
    }
}

void cache_persist_checkpoint( void )
{
    uint16_t count = tracker_cache_count( );
    uint16_t persist_count = ( count < CACHE_PERSIST_MAX_SLOTS ) ? count : CACHE_PERSIST_MAX_SLOTS;

    waste_detect_recycle( );

    for( uint16_t i = 0; i < persist_count; i++ )
    {
        uint8_t  entry_len = 0;
        uint32_t entry_ts  = 0;
        uint8_t *entry = tracker_cache_get( i, &entry_len, &entry_ts );

        if( entry == NULL || entry_len == 0 )
        {
            continue;
        }
        if( !fds_write_slot( i, entry, entry_len ))
        {
            /* Out of flash space -- stop here. Slots already written
             * (the oldest entries, written first) stay protected. */
            return;
        }
    }

    /* Clean up any slots beyond what we just wrote -- leftovers from a
     * previous checkpoint that held more entries than this one does. */
    for( uint16_t i = persist_count; i < CACHE_PERSIST_MAX_SLOTS; i++ )
    {
        fds_delete_slot( i );
    }
}
