/*
 * Copyright (C) 2026 ViVSoft Computers LLC
 *
 * Ring-buffer cache for T1000-E LoRaWAN tracker.
 * Stores scan result packets when device is out of LoRaWAN range
 * and replays them when connectivity returns.
 */

#ifndef APP_TRACKER_CACHE_H
#define APP_TRACKER_CACHE_H

#include <stdint.h>
#include <stdbool.h>

/* Firmware version - embedded in every uplink */
#define FIRMWARE_VERSION           21

#define TRACKER_CACHE_MAX_DEPTH   200    /* ~33h of 10-min intervals */
#define TRACKER_CACHE_MAX_SIZE    128   /* max LoRaWAN payload size */
#define TRACKER_CACHE_TTL_SECONDS (4 * 3600)  /* 4h: discard entries older than this */

/**
 * @brief Save a packet to the ring-buffer cache.
 * Overwrites the oldest entry if the buffer is full.
 * The entry timestamp is set to the current RTC time.
 */
void tracker_cache_save( const uint8_t *data, uint8_t len );

/**
 * @brief Return the number of cached (non-expired) entries.
 */
uint8_t tracker_cache_count( void );

/**
 * @brief Get the oldest cached entry by index.
 * Index 0 = oldest, index N-1 = newest.
 * @param[in]  idx   Entry index (0 = oldest)
 * @param[out] len   Set to entry length
 * @param[out] ts    Set to entry RTC timestamp (seconds)
 * @return Pointer to the cached data, or NULL if idx is out of range.
 */
uint8_t *tracker_cache_get( uint8_t idx, uint8_t *len, uint32_t *ts );

/**
 * @brief Remove the oldest cached entry (FIFO pop).
 */
void tracker_cache_pop( void );

/**
 * @brief Clear the entire cache.
 */
void tracker_cache_clear( void );

#endif /* APP_TRACKER_CACHE_H */
