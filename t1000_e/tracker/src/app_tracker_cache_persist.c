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

/* fds_record_write()/update()/delete() return NRF_SUCCESS when the
 * operation is successfully *queued* -- completion is asynchronous,
 * confirmed only via an FDS event. app_at_fds_datas.c's fds_evt_handler
 * already tracks this, but its fds_opt_status is static (file-private),
 * so this module registers its own handler rather than reaching into
 * that one.
 *
 * A prior version of this file waited via a bounded sd_app_evt_wait()
 * loop, mirroring how waste_detect_recycle() waits for GC completion.
 * Bench-tested and confirmed NOT to work here: checkpointed entries
 * never came back after a power cycle. app_at_fds_datas.c's own proven
 * write path -- write_record_by_desc()/update_record_by_desc(), used
 * for the config record that has reliably persisted across many
 * versions of this firmware -- does NOT use sd_app_evt_wait() at all.
 * It uses a fixed hal_mcu_wait_ms(8) delay plus a flag check, and its
 * caller (write_lfs_file()) does a second, independent verification
 * layer: an explicit read-back and memcmp against what was intended.
 * This file now matches that exact two-layer pattern instead of the
 * theoretically-cleaner-but-empirically-broken blocking wait. Best
 * guess at why sd_app_evt_wait() didn't work here specifically:
 * app_lora_stack_suspend() (called earlier in app_user_power_off(),
 * before any checkpoint write) already suspends radio communications,
 * which may reduce or change how SoftDevice events get dispatched at
 * this point in the shutdown sequence -- not confirmed, just the most
 * plausible explanation for a working pattern used elsewhere in this
 * codebase not working in this specific call context. */
#define FDS_OP_WAIT_MS   20

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
    bool is_update = ( fds_record_find( CACHE_CKPT_FILE, slot, &desc, &tok ) == NRF_SUCCESS );

    s_op_done   = false;
    s_op_result = false;

    rc = is_update ? fds_record_update( &desc, &record ) : fds_record_write( &desc, &record );
    PRINTF( "cache_persist: slot %d %s queue rc=0x%02x\r\n", slot, is_update ? "update" : "write", rc );
    if( rc != NRF_SUCCESS )
    {
        return false;  /* failed to even queue -- e.g. genuinely out of space */
    }

    /* Layer 1: fixed delay + flag check, matching
     * write_record_by_desc()/update_record_by_desc(). */
    hal_mcu_wait_ms( FDS_OP_WAIT_MS );
    PRINTF( "cache_persist: slot %d event done=%d result=%d\r\n", slot, s_op_done, s_op_result );
    if( !s_op_done || !s_op_result )
    {
        return false;
    }

    /* Layer 2: independent read-back and byte-for-byte compare,
     * matching write_lfs_file()'s verification -- don't just trust
     * the event flag, confirm the bytes actually landed. */
    fds_record_desc_t  verify_desc = { 0 };
    fds_find_token_t   verify_tok  = { 0 };
    fds_flash_record_t flash_record = { 0 };

    if( fds_record_find( CACHE_CKPT_FILE, slot, &verify_desc, &verify_tok ) != NRF_SUCCESS )
    {
        PRINTF( "cache_persist: slot %d verify find FAILED\r\n", slot );
        return false;
    }
    if( fds_record_open( &verify_desc, &flash_record ) != NRF_SUCCESS )
    {
        PRINTF( "cache_persist: slot %d verify open FAILED\r\n", slot );
        return false;
    }

    uint16_t expect_bytes = 1 + len;
    uint16_t actual_bytes = flash_record.p_header->length_words * sizeof( uint32_t );
    bool verified = ( actual_bytes >= expect_bytes ) &&
                     ( memcmp( flash_record.p_data, buf, expect_bytes ) == 0 );

    fds_record_close( &verify_desc );
    PRINTF( "cache_persist: slot %d verify expect=%d actual=%d match=%d\r\n",
             slot, expect_bytes, actual_bytes, verified );
    return verified;
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
            hal_mcu_wait_ms( FDS_OP_WAIT_MS );
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

    uint16_t restored = 0;
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
            restored++;
        }
    }

    PRINTF( "cache_persist: restore found and replayed %d entries\r\n", restored );
}

void cache_persist_checkpoint( void )
{
    uint16_t count = tracker_cache_count( );
    uint16_t persist_count = ( count < CACHE_PERSIST_MAX_SLOTS ) ? count : CACHE_PERSIST_MAX_SLOTS;

    PRINTF( "cache_persist: checkpoint start, cache count=%d, persisting=%d\r\n", count, persist_count );

    waste_detect_recycle( );

    uint16_t written = 0;
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
            PRINTF( "cache_persist: checkpoint stopped at slot %d, %d written\r\n", i, written );
            return;
        }
        written++;
    }

    PRINTF( "cache_persist: checkpoint complete, %d entries written\r\n", written );

    /* Clean up any slots beyond what we just wrote -- leftovers from a
     * previous checkpoint that held more entries than this one does. */
    for( uint16_t i = persist_count; i < CACHE_PERSIST_MAX_SLOTS; i++ )
    {
        fds_delete_slot( i );
    }
}
