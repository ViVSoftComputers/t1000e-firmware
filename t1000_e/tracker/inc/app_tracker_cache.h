/*
 * Copyright (C) 2024-2026 ViVoSofT
 *
 * Ring-buffer cache for T1000-E LoRaWAN tracker.
 * Stores scan result packets that failed to send.
 * When connectivity returns, replay non-expired entries in FIFO order.
 *
 * Each cached packet stores:
 *   - timestamp (RTC seconds when it was cached)
 *   - length (1-128 bytes)
 *   - payload data
 *
 * All entries are replayed in FIFO order — no TTL expiry.
 */

#ifndef APP_TRACKER_CACHE_H
#define APP_TRACKER_CACHE_H

#include <stdint.h>
#include <stdbool.h>

/* Firmware version - embedded in every uplink */
#define FIRMWARE_VERSION           27  /* v27: flash-backed cache checkpoint (survives power-off) */

#define TRACKER_CACHE_MAX_DEPTH   1000   /* ~166h of 10-min intervals */
#define TRACKER_CACHE_MAX_SIZE    128   /* max LoRaWAN payload size */

/**
 * @brief Save a packet to the ring-buffer cache.
 * Overwrites the oldest entry if the buffer is full.
 * The entry timestamp is set to the current RTC time.
 */
void tracker_cache_save( const uint8_t *data, uint8_t len );

/**
 * @brief Return the number of cached entries.
 */
uint16_t tracker_cache_count( void );

/**
 * @brief Return a counter bumped on every save and every pop.
 * Lets a caller cheaply detect "has the cache changed since I last
 * looked" without diffing contents -- see app_tracker_cache_persist.c.
 */
uint32_t tracker_cache_generation( void );

/**
 * @brief Get the oldest cached entry by index.
 * Index 0 = oldest, index N-1 = newest.
 * @param[in]  idx   Entry index (0 = oldest)
 * @param[out] len   Set to entry length
 * @param[out] ts    Set to entry RTC timestamp (seconds)
 * @return Pointer to the cached data, or NULL if idx is out of range.
 */
uint8_t *tracker_cache_get( uint16_t idx, uint8_t *len, uint32_t *ts );

/**
 * @brief Remove the oldest cached entry (FIFO pop).
 */
void tracker_cache_pop( void );

/**
 * @brief Clear the entire cache.
 */
void tracker_cache_clear( void );

#endif /* APP_TRACKER_CACHE_H */
