/*
 * Copyright (C) 2026 ViVoSofT
 *
 * See app_tracker_cache_persist.h for the design rationale.
 */

#include <string.h>
#include "fds.h"

/* sd_app_evt_wait() -- same conditional include app_at_fds_datas.c
 * uses to get it, for the same reason. */
#ifdef SOFTDEVICE_PRESENT
#include "nrf_soc.h"
#endif

#include "smtc_hal.h"
#include "app_at_fds_datas.h"
#include "app_tracker_cache.h"
#include "app_tracker_cache_persist.h"

/* Slot record layout in flash: [len:1 byte][data:len bytes], FDS pads
 * to a whole number of words itself. */
#define SLOT_BUF_SIZE   ( 1 + TRACKER_CACHE_MAX_SIZE )

/* fds_record_write()/update()/delete() return NRF_SUCCESS when the
 * operation is successfully *queued* -- completion is asynchronous,
 * confirmed only via an FDS event. app_at_fds_datas.c's fds_evt_handler
 * already tracks this, but its fds_opt_status is static (file-private),
 * so this module registers its own handler rather than reaching into
 * that one. Bounded wait, matching the pattern waste_detect_recycle()
 * already uses for GC completion (sd_app_evt_wait() loop) -- not the
 * fixed-delay-then-hope pattern, which is exactly what let writes
 * silently not land in the first version of this file. */
#define FDS_OP_WAIT_MAX_ITER   1000

static bool volatile s_op_done   = false;
static bool volatile s_op_result = false;
static bool          s_handler_registered = false;

static void cache_persist_fds_evt_handler( fds_evt_t const *p_evt )
{
    switch( p_evt->id )
    {
        case FDS_EVT_WRITE:
        case FDS_EVT_UPDATE:
        case FDS_EVT_DEL_RECORD:
            s_op_result = ( p_evt->result == NRF_SUCCESS );
            s_op_done   = true;
            break;

        default:
            break;
    }
}

static bool fds_wait_for_op( void )
{
    uint32_t guard = 0;
    while( !s_op_done && guard < FDS_OP_WAIT_MAX_ITER )
    {
        sd_app_evt_wait( );
        guard++;
    }
    return s_op_done && s_op_result;
}

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

    s_op_done   = false;
    s_op_result = false;

    if( fds_record_find( CACHE_CKPT_FILE, slot, &desc, &tok ) == NRF_SUCCESS )
    {
        rc = fds_record_update( &desc, &record );
    }
    else
    {
        rc = fds_record_write( &desc, &record );
    }
    if( rc != NRF_SUCCESS )
    {
        return false;  /* failed to even queue -- e.g. genuinely out of space */
    }

    return fds_wait_for_op( );
}

static void fds_delete_slot( uint16_t slot )
{
    fds_record_desc_t desc = { 0 };
    fds_find_token_t  tok  = { 0 };

    if( fds_record_find( CACHE_CKPT_FILE, slot, &desc, &tok ) == NRF_SUCCESS )
    {
        s_op_done   = false;
        s_op_result = false;
        if( fds_record_delete( &desc ) == NRF_SUCCESS )
        {
            fds_wait_for_op( );
        }
    }
}

void cache_persist_restore( void )
{
    uint8_t buf[SLOT_BUF_SIZE];

    /* Register once, here, at boot -- long before cache_persist_checkpoint()
     * could ever be called (that only happens at power-off). FDS supports
     * multiple registered handlers (FDS_MAX_USERS in sdk_config.h); this
     * doesn't disturb app_at_fds_datas.c's own registration. */
    if( !s_handler_registered )
    {
        fds_register( cache_persist_fds_evt_handler );
        s_handler_registered = true;
    }

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
            /* Write failed to queue (e.g. out of flash space) or timed
             * out waiting for confirmation -- stop here either way.
             * Slots already written (the oldest entries, written
             * first) stay protected. */
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
