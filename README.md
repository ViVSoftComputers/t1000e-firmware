# T1000-E Tracker Firmware — v25 Independent Producer/Consumer Edition

Built: 2026-08-11  
Device: [Seeed SenseCAP Card Tracker T1000-E for LoRaWAN](https://www.seeedstudio.com/SenseCAP-Card-Tracker-T1000-E-for-LoRaWAN-p-6408.html) (nRF52840 + AG3335 GPS + LR1110 LoRa)  
Based on: [Seeed-Studio/Seeed-Tracker-T1000-E-for-LoRaWAN-dev-board](https://github.com/Seeed-Studio/Seeed-Tracker-T1000-E-for-LoRaWAN-dev-board) (commit `f3ad9d4`)

> **v25 is a complete architectural rewrite.** Producer and consumer are fully independent — each has its own timer, own schedule, own watchdog. They share nothing except the ring buffer cache. Producer only touches GPS/sensors and writes to cache. Consumer only touches the LoRa radio and reads from cache. Neither blocks the other.

## 📖 Read the Full Article

Detailed write-up with architecture diagrams, field test results, and flashing guide:  
**[T1000-E Cache Firmware: Never Lose LoRaWAN Sensor Data Again](https://hub.lorameshdevices.com/projects/t1000-e-cache-firmware-never-lose-lorawan-sensor-data-again)** — LoRa Mesh Devices

## ⚠️ Before You Flash — Backup Your Factory Firmware

**This is permanent.** Once flashed, the factory firmware is overwritten.

1. Connect your T1000-E via USB-C
2. Double-press the button rapidly to enter UF2 bootloader mode (device appears as a USB drive)
3. Copy `CURRENT.UF2` from the drive to your computer
4. Rename it to `FACTORY_T1000E_BACKUP.uf2` and store it somewhere safe
5. To restore: drag the backup UF2 onto the device in bootloader mode

## Flash

1. Double-press the button to enter UF2 bootloader
2. Drag `t1000-e-v25.uf2` onto the USB drive
3. Device reboots automatically after flashing (~10 seconds)

## v25 Button Behavior

| Press | Action | Beep Feedback |
|---|---|---|
| **Single-press** | Trigger immediate scan (fast 5s polling, 30s timeout) | Ack beep if idle · 3 short = GPS fix · 2 long = no fix · 1 long = timeout · double-error-beep if busy |
| **Double-press** | Toggle **turbo mode** (~1min scans, fast polling) | 500ms long beep on enter · 500ms long beep on exit |
| **Triple-press** | BLE advertising | — |
| **Quad-press** | **Force drain all cached entries** | 500ms long beep when complete |
| **Long-press** (3s) | Power off | Power-off melody |

### Beep Reference

| Pattern | Meaning |
|---|---|
| **Single 40ms** | Button press acknowledged — scan starting |
| **Double 40ms** (quick) | **Busy — scan already running, try again later** |
| **3 short** (80ms) | GPS fix acquired — position saved to cache |
| **2 long** (500ms) | No GPS fix after scan duration |
| **1 long** (500ms) | **Hard timeout** — GPS failed to acquire in 30s |
| **500ms** | Turbo toggle or force-drain complete |

### Single-Press Flow (Fast Polling)

1. Button press → **busy check** — if scan already running, play error beep and bail immediately
2. If idle → 40ms ack beep → GPS scan starts
3. **Poll at 5s intervals** — if GPS has fix already, end immediately and beep 3×
4. No fix yet → poll again in 5s → repeat up to 6 polls (30s total)
5. **30s hard timeout** → force-end GPS, play 1 long timeout beep, reschedule
6. Data saved to cache (never blocks if drain is running — producer is independent)

**Power efficiency:** GPS only runs during the scan window. Between scans, the device sleeps. Scheduled scans use a one-shot 15s mode — no fast polling, just wake-once-and-save.

### Turbo Mode
- Scans every **~1 minute** with fast 5s polling (independent of consumer drain interval)
- Beeps **once** after each scan completes
- Data is cached — consumer drains on its **own 25-minute schedule**
- Turbo scan entries always save (motion gate bypassed — turbo is an explicit user action)
- Double-press again to exit and restore previous scan interval

### Force Drain (Quad-Press)
- Press 4× rapidly → consumer fast-drains all cached entries
- In range: entries drain at 3s intervals via confirmed uplinks
- Out of range: one attempt, then stops
- Cache empty → 500ms beep — works immediately even with no entries
- Useful for flushing cached data immediately

### Mutual Exclusion
Only one scan runs at a time — if any scan is in progress (single-click, scheduled, or turbo), all other scan triggers are rejected:
- **Button press while scanning** → error beep, ignore
- **Scheduled scan while user scan active** → skip, wait for next cycle
- **Turbo scan while single-click active** → skip, wait for next cycle

### Motion Gate (25m)
Pure motion gate — only cache entries when the device actually moves.

| Condition | Saves to cache? |
|---|---|
| **Moved >25m** (GPS jitter compared to last saved position) | ✅ Yes |
| **Stationary** (GPS fix within 25m of last position) | ❌ No — cache stays empty, consumer finds nothing to drain |
| **No GPS fix** (can't determine if moved) | ✅ Yes — can't gate what you can't measure |
| **User press** (single-click) | ✅ Yes — always saves |
| **Turbo active** (double-click) | ✅ Yes — always saves |
| **First GPS fix after power-on** | ✅ Yes — establishes baseline |

Result: stationary device → empty cache → consumer fires on schedule, finds nothing, waits. Battery and airtime conserved.

## Architecture (v25)

### Core Principle: Complete Separation

Producer and consumer share **nothing** except the ring buffer cache. Each has:
- Its own timer (`producer_next_s` / `consumer_next_s`)
- Its own watchdog
- Its own schedule function (`schedule_producer()` / `schedule_consumer()`)
- Its own state (scan state machine / drain state machine)

```
┌─────────────────────────────────────────────────────────────────────┐
│                       on_modem_alarm()                              │
│                                                                     │
│  ┌─ if now >= consumer_next_s:                                     │
│  │    ┌─ cache has entries + drain idle → drain one entry          │
│  │    ├─ cache empty + force_drain → beep finish immediately       │
│  │    ├─ cache empty → schedule_consumer(drain_interval=1500s)     │
│  │    └─ drain active → watch for 120s timeout → schedule(3s)      │
│  │                                                                  │
│  └─ if now >= producer_next_s:                                      │
│       └─ app_tracker_scan_process() → GPS → save to cache           │
│           Fast 5s polling (user/turbo) or 15s one-shot (scheduled)  │
│           30s hard timeout if GPS never gets a fix                   │
│                                                                     │
│  sync_alarm() → min(producer_next, consumer_next) → modem alarm    │
└─────────────────────────────────────────────────────────────────────┘
```

```mermaid
flowchart LR
    subgraph PRODUCER["PRODUCER — GPS & Sensors only"]
        P_TIMER["producer_next_s"]
        SCAN["app_tracker_scan_process()"]
        SAVE["tracker_cache_save()"]
        GATE["Motion gate: moved >25m?"]
        TIMEOUT["30s hard timeout"]
        ---
        P_TIMER -->|"timer expires"| SCAN
        SCAN -->|"5s poll (user)<br/>15s one-shot (sched)"| GATE
        GATE -->|"yes"| SAVE
        GATE -->|"no (stationary)"| SKIP["skip — cache conserved"]
        SCAN -.->|"stall recovery"| TIMEOUT
        TIMEOUT -.->|"force end scan"| GATE
    end

    subgraph CACHE["Ring Buffer Cache"]
        RB[("200 entries<br/>concurrent read/write")]
    end

    subgraph CONSUMER["CONSUMER — LoRa only"]
        C_TIMER["consumer_next_s"]
        DRAIN["cache_consumer_trigger()"]
        TX["app_send_frame()<br/>confirmed uplinks"]
        TXDONE["on_modem_tx_done()"]
        EMPTY["empty?<br/>force_drain beep"]
        ---
        C_TIMER -->|"timer expires"| DRAIN
        DRAIN -->|"get oldest entry"| TX
        TX -->|"LoRaWAN"| TXDONE
        TXDONE -->|"CONFIRMED: pop"| DRAIN
        TXDONE -->|"NOT_SENT: stop"| C_TIMER
        C_TIMER -->|"cache empty"| EMPTY
    end

    SAVE -->|"append"| RB
    DRAIN -->|"read front"| RB
    TXDONE -->|"pop front"| RB
```

### Timer Separation

| | Producer | Consumer |
|---|---|---|
| **Timer variable** | `producer_next_s` (absolute RTC) | `consumer_next_s` (absolute RTC) |
| **Schedule function** | `schedule_producer(delay)` | `schedule_consumer(delay)` |
| **Default interval** | 300s (5 min, configurable via downlink) | 1500s (25 min) |
| **Never touches** | `app_send_frame()` or LoRa | `tracker_cache_save()` or GPS/sensors |
| **GPS scan** | AG3335 (dedicated chip, UART) | — |
| **Radio** | — | LR1110 (dedicated chip, SPI) |

`sync_alarm()` picks the sooner of the two timers for the single modem alarm — both fire independently.

### ISR-Safe Architecture

`APP_TIMER_CONFIG_USE_SCHEDULER` is `0` in `sdk_config.h`, which means `app_timer`
callbacks — including the button click handler — run directly in RTC interrupt
context, not the main loop. To avoid racing the main loop's modem/producer state:

- **Button click handler** (`app_button.c`): only sets `pending_button_action`
  (one of scan-now / turbo-toggle / force-drain) + `hal_sleep_exit()`. ZERO
  modem API calls, ZERO reads/writes of `tracker_scan_status` or the alarm.
- **Main loop** (`process_pending_button_action()` in `main_lorawan_tracker.c`):
  picks up `pending_button_action` once per iteration and does the real work —
  busy check, beep, `smtc_modem_alarm_clear_timer()` / `schedule_producer()` /
  `schedule_consumer()` — all from safe, single-threaded context.
- **Busy check**: `tracker_scan_status` acts as a mutex, checked only from the
  main loop now, so there's no window where button-interrupt context and the
  main loop can both be mutating it at once.

### Watchdogs

| Watchdog | Side | Trigger | Action |
|---|---|---|---|
| **Scan stall** | Producer | Same scan status for 3 alarm ticks (skips status 1 — GPS scanning) | Force-reset to idle |
| **GPS scan timeout** | Producer | GPS scan running > 30s with no fix | Force `gnss_scan_end()`, timeout beep, reset |
| **Drain TX timeout** | Consumer | `cache_drain_active` stays true > 120s overall | Force-reset and reschedule (25 min) |
| **send_frame failure** | Consumer | `app_send_frame()` returns false | Immediate drain reset |

### Drain Chain

When in LoRa range:
1. Consumer timer fires → drain one entry via confirmed uplink
2. `TXDONE_CONFIRMED` → pop entry → `schedule_consumer(3)` → 3s later drain next
3. Repeat until cache empty → `schedule_consumer(1500)` → wait 25 min for next schedule

When out of range:
1. TX fails with `NOT_SENT` → `cache_drain_active = false` → `schedule_consumer(1500)`
2. Next schedule tick retries

**120s overall drain timeout**: if the drain chain takes longer than 2 minutes total (across all entries), it aborts and waits for the next scheduled interval.

## Key Design Decisions

1. **Producer never calls `app_send_frame()`** — GPS/sensors only write to cache
2. **Consumer never calls `tracker_cache_save()`** — LoRa only reads/drains from cache
3. **Drain interval: 25 min** — consumer drains infrequently to save airtime; quad-press force-drains anytime
4. **Pure motion gate** — no heartbeat forcing saves when stationary; empty cache = nothing to drain
5. **Fast single-click: 5s polls** — GPS checked every 5s, returns immediately on fix; 30s hard cap. No queue.
6. **Scheduled scans: 15s one-shot** — power-efficient; no polling, just wake once and save
7. **Turbo always saves** — explicit user action bypasses motion gate, uses fast 5s polling
8. **Mutual exclusion** — only one scan runs at a time; busy beep feedback on collision
9. **Confirmed uplinks only** — cache entries popped only on real ACK, not on NOT_SENT
10. **ISR-safe** — button ISR never calls modem API; all alarm operations happen in main loop context
11. **120s drain timeout** — overall drain chain cap; resets and waits for next schedule

## Configuring Scan Interval (Downlink)

Send a downlink on FPort 5 to change the periodic scan interval:

| Interval | Hex Payload |
|---|---|
| 2 min | `81 00 00 00 02` |
| 5 min | `81 00 00 00 05` |
| 10 min | `81 00 00 00 0A` |
| 15 min | `81 00 00 00 0F` |
| 30 min | `81 00 00 00 1E` |
| 60 min | `81 00 00 00 3C` |

**Format:** `81 00 00 HH LL` where HH LL = interval in minutes (big-endian).  
**How to send:** ChirpStack → Device → Queue → FPort 5 → Hex payload.  
The new interval takes effect on the next scan cycle.

## Cache

- Ring buffer, 200 entries maximum
- 4-hour TTL (entries older than 4 hours are expired)
- Concurrent read/write safe — producer appends at write pointer, consumer reads at read pointer
- All entries are confirmed uplinks — only popped on `TXDONE_CONFIRMED`

## Build

Requires nRF5 SDK 17.1.0 and ARM GCC toolchain.

```bash
cd examples/ble_peripheral/t1000-e/pca10056/s140/11_ses_lorawan_tracker
python3 build_factory.py
```

Build script compiles 180 files using `generic_gcc_nrf52.ld` from the nRF5 SDK.

See `UF2_BUILD_RECIPE.md` for full UF2 assembly steps.

## License

AGPL-3.0 — see [LICENSE](LICENSE)
