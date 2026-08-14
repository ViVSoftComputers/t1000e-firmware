/*
 * Copyright (C) 2026 ViVoSofT
 *
 * Flash-backed checkpoint of the tracker cache's oldest not-yet-drained
 * entries, so a power-off or reset doesn't lose everything waiting to send.
 *
 * Bounded by design: FDS has a small, fixed flash budget (12KB total --
 * FDS_VIRTUAL_PAGES(3) x FDS_VIRTUAL_PAGE_SIZE(1024 words) in
 * sdk_config.h -- shared with device config storage. That's nowhere near
 * enough to mirror the full ~136KB RAM cache (TRACKER_CACHE_MAX_DEPTH
 * entries), so this persists only the oldest CACHE_PERSIST_MAX_SLOTS
 * undrained entries -- the ones that would be sent next anyway, and the
 * ones a restore should protect to keep FIFO order intact. Entries
 * beyond that window stay RAM-only. A real out-of-range walk logged
 * ~48 uplinks in an hour; 60 slots covers that with margin at typical
 * entry sizes (~20-30 bytes each).
 *
 * Not write-through: checkpointing only happens when the cache has
 * actually changed since the last checkpoint (tracked via
 * tracker_cache_generation()), called opportunistically from
 * main-loop-safe contexts. Bounds flash write frequency independent of
 * scan cadence -- during a long stationary idle stretch nothing changes,
 * so nothing gets rewritten.
 */

#ifndef APP_TRACKER_CACHE_PERSIST_H
#define APP_TRACKER_CACHE_PERSIST_H

#include <stdint.h>

#define CACHE_PERSIST_MAX_SLOTS   60

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Replay any flash-checkpointed entries back into the RAM cache,
 * oldest first. Call once at boot, after FDS is initialized
 * (fds_init_write()) and before normal cache/scan operation begins.
 */
void cache_persist_restore( void );

/*!
 * @brief Checkpoint the cache's oldest undrained entries to flash, if
 * the cache has changed since the last checkpoint. No-op otherwise.
 * Call from main-loop-safe context only (e.g. on_modem_alarm(),
 * on_modem_tx_done()) -- touches FDS and blocks briefly per write,
 * same as the existing config persistence in app_at_fds_datas.c.
 */
void cache_persist_tick( void );

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKER_CACHE_PERSIST_H */
