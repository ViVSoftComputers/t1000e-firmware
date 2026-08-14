/*
 * Copyright (C) 2026 ViVoSofT
 *
 * Flash-backed checkpoint of the tracker cache's oldest not-yet-drained
 * entries, so a *deliberate power-off* doesn't lose everything waiting
 * to send. Does NOT protect against a crash or unexpected reset -- see
 * "Why power-off only" below for why that tradeoff is deliberate.
 *
 * Bounded by design: FDS has a small, fixed flash budget (12KB total --
 * FDS_VIRTUAL_PAGES(3) x FDS_VIRTUAL_PAGE_SIZE(1024 words) in
 * sdk_config.h -- shared with device config storage. That's nowhere near
 * enough to mirror the full ~136KB RAM cache (TRACKER_CACHE_MAX_DEPTH
 * entries), so this persists only the oldest CACHE_PERSIST_MAX_SLOTS
 * undrained entries -- the ones that would be sent next anyway, and the
 * ones a restore should protect to keep FIFO order intact. Entries
 * beyond that window stay RAM-only.
 *
 * Why power-off only, not periodic: an earlier T1000-E iteration tried
 * doing FDS flash writes from modem event callbacks (on_modem_alarm(),
 * on_modem_tx_done()) or general main-loop-tick context, and it broke
 * smtc_modem_alarm_start_timer() -- the device would send one packet
 * after joining, then go silent. That fix was to remove FDS from the
 * cache path entirely. This design avoids that failure class by only
 * ever writing once, from app_user_power_off() (app_button.c) -- which
 * runs well after app_lora_stack_suspend() has already cleared the
 * modem alarm and left the network, not from any callback or ticking
 * context. No periodic/incremental checkpointing exists in this design
 * on purpose -- do not add a tick-driven call site without first
 * understanding why the periodic version was removed.
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
 * Safe: runs before the modem stack is initialized at all, nowhere
 * near the alarm-timer interaction this design otherwise avoids.
 */
void cache_persist_restore( void );

/*!
 * @brief Write the cache's current oldest undrained entries to flash.
 * Call exactly once, from app_user_power_off(), after
 * app_lora_stack_suspend() -- see the file header for why.
 */
void cache_persist_checkpoint( void );

#ifdef __cplusplus
}
#endif

#endif /* APP_TRACKER_CACHE_PERSIST_H */
