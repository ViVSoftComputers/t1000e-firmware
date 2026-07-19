/*
 * Copyright (C) 2024-2026 ViVoSofT
 *
 * Ring-buffer cache for T1000-E LoRaWAN tracker.
 */

#include "app_tracker_cache.h"
#include "smtc_hal.h"
#include <stddef.h>
/* Attribute to prevent linker --gc-sections from discarding public functions */
#define KEEP __attribute__((used))

/* -------------------------------------------------------------------------- */
/* --- RING BUFFER STATE                                                  --- */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint8_t  data[TRACKER_CACHE_MAX_SIZE];
    uint8_t  len;
    uint32_t timestamp;   /* RTC seconds when cached */
} cache_entry_t;

static cache_entry_t cache_ring[TRACKER_CACHE_MAX_DEPTH];
static uint8_t       cache_head = 0;   /* next write position */
static uint8_t       cache_tail = 0;   /* oldest unread position */
static uint8_t       cache_count = 0;  /* number of valid entries */

/* -------------------------------------------------------------------------- */
/* --- PUBLIC FUNCTIONS                                                   --- */
/* -------------------------------------------------------------------------- */

KEEP void tracker_cache_save( const uint8_t *data, uint8_t len )
{
    if( data == NULL || len == 0 || len > TRACKER_CACHE_MAX_SIZE )
    {
        return;
    }

    /* Copy data into the head slot */
    cache_entry_t *entry = &cache_ring[cache_head];
    for( uint8_t i = 0; i < len; i++ )
    {
        entry->data[i] = data[i];
    }
    entry->len       = len;
    entry->timestamp = hal_rtc_get_time_s( );

    /* Advance head; if full, also advance tail (overwrite oldest) */
    if( cache_count < TRACKER_CACHE_MAX_DEPTH )
    {
        cache_count++;
    }
    else
    {
        cache_tail = ( cache_tail + 1 ) % TRACKER_CACHE_MAX_DEPTH;
    }
    cache_head = ( cache_head + 1 ) % TRACKER_CACHE_MAX_DEPTH;
}

KEEP uint8_t tracker_cache_count( void )
{
    return cache_count;
}

KEEP uint8_t *tracker_cache_get( uint8_t idx, uint8_t *len, uint32_t *ts )
{
    if( idx >= cache_count || len == NULL || ts == NULL )
    {
        return NULL;
    }

    /* Compute the ring index for the requested position (0 = oldest) */
    uint8_t pos = ( cache_tail + idx ) % TRACKER_CACHE_MAX_DEPTH;
    *len = cache_ring[pos].len;
    *ts  = cache_ring[pos].timestamp;
    return cache_ring[pos].data;
}

KEEP void tracker_cache_pop( void )
{
    if( cache_count == 0 )
    {
        return;
    }
    cache_tail = ( cache_tail + 1 ) % TRACKER_CACHE_MAX_DEPTH;
    cache_count--;
}

KEEP void tracker_cache_clear( void )
{
    cache_head  = 0;
    cache_tail  = 0;
    cache_count = 0;
}
