# T1000-E Tracker Firmware — v31

Built: 2026-08-19  
Device: [Seeed SenseCAP Card Tracker T1000-E for LoRaWAN](https://www.seeedstudio.com/SenseCAP-Card-Tracker-T1000-E-for-LoRaWAN-p-6408.html) (nRF52840 + AG3335 GPS + LR1110 LoRa)  
Based on: [Seeed-Studio/Seeed-Tracker-T1000-E-for-LoRaWAN-dev-board](https://github.com/Seeed-Studio/Seeed-Tracker-T1000-E-for-LoRaWAN-dev-board) (commit `f3ad9d4`)

> **v31 fixes a downlink that never actually worked, adds a matching one for drain interval, and puts live battery/temp/light readings in the Bluetooth download.** The scan-interval downlink (`81 00 00 HH LL`, already documented below) had no handler behind it at all — every downlink silently did nothing since the code path that would apply it was missing. Fixed, and a new `83 00 00 HH LL` downlink controls the consumer's drain interval the same way. `AT+GPX` now opens with a live battery/temp/light reading (not from the cache — taken fresh, right when you ask) so BLE-only sessions aren't flying blind on battery, and every `<trkpt>` carries its own battery/temp/light as GPX `<extensions>`. On top of v30's 5-click LoRaWAN toggle, v29's first Bluetooth release (AT+GPX, `gpx-downloader.html`, flash-checkpoint replay-loop fix), v28's join-independent scanning and GPS epoch fix, v27's flash-backed cache checkpoint, v26's distinct beep patterns, and persistent LED feedback.

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
2. Drag the latest `t1000-e-v31-*.uf2` onto the USB drive
3. Device reboots automatically after flashing (~10 seconds)

## v30 Button Behavior (unchanged in v31)

| Press | Action | Beep Feedback |
|---|---|---|
| **Single-press** | Trigger immediate scan (fast 5s polling, 30s timeout) | Ack beep if idle · 3 short (+green flash) = GPS fix · 2 long (+red flash) = no fix · 1 long (+red flash) = timeout · double-error-beep if busy |
| **Double-press** | Toggle **turbo mode** (~1min scans, fast polling) | 2 short high-pitched beeps on enter · 2 short low-pitched beeps on exit |
| **Triple-press** | BLE advertising | — |
| **Quad-press** | **Force drain all cached entries** | 3 quick beeps on start · rising two-tone chirp on complete |
| **Five-press** | **Toggle LoRaWAN on/off** (persists across reboot) | 2 low falling tones = off · 2 higher rising tones = on |
| **Long-press** (3s) | Power off | Power-off melody |

### Beep Reference

Every pattern below is now unique — no two events share the same sound. (Prior to v26, turbo-enter, turbo-exit, force-drain-start, force-drain-complete, and GPS-timeout all played an identical single 500ms beep.)

| Pattern | Meaning |
|---|---|
| **Single 40ms** | Button press acknowledged — scan starting |
| **Double 40ms** (quick) | **Busy — scan already running, try again later** |
| **3 short** (80ms, 2kHz) | GPS fix acquired — position saved to cache |
| **2 long** (500ms, 2kHz) | No GPS fix after scan duration |
| **1 long** (500ms, 2kHz) | **Hard timeout** — GPS failed to acquire in 30s |
| **2 short, high pitch** (2.6kHz) | Turbo mode entered |
| **2 short, low pitch** (1.2kHz) | Turbo mode exited |
| **3 quick** (60ms, 1.6kHz) | Force-drain started |
| **Two-tone rising chirp** (1.2kHz → 2.4kHz) | Force-drain completed |
| **2 low, falling** (150ms, 1kHz) | LoRaWAN turned off |
| **2 higher, rising** (150ms, 2.2kHz) | LoRaWAN turned back on |

### LED Reference

The board has a red/green LED, no dimming. LEDs now do two jobs: a **momentary flash synced to a beep** (reinforces the sound, e.g. outdoors), and a **persistent indicator for ongoing state** (something a beep — which is over in a moment — can't do).

| LED behavior | Meaning |
|---|---|
| 3 green flashes, synced with the GPS-fix beeps | GPS fix acquired |
| 2 red flashes, synced with the no-fix beeps | No GPS fix |
| 1 red flash, synced with the timeout beep | Hard timeout |
| **Solid red** | **Turbo mode is currently active** — stays lit the whole time, not just at the toggle moment. Suppresses the charge-status indicator (also on red) while on. |
| **Fast green blink** (150ms) | **A drain chain is actively running** — from the first entry to the last, not per-entry (no flicker between sends) |

### Single-Press Flow (Fast Polling)

1. Button press → **busy check** — if scan already running, play error beep and bail immediately
2. If idle → 40ms ack beep → GPS scan starts
3. **Poll at 5s intervals** — if GPS has fix already, end immediately and beep 3×
4. No fix yet → poll again in 5s → repeat until fix or hard timeout
5. **Hard timeout**: 30s once the device has ever gotten a fix since boot (warm start — chip has ephemeris). Before that first-ever fix (cold start — no ephemeris/almanac), the ceiling is 150s instead, since a cold acquisition routinely needs far more than 30s and the GPS chip goes into standby between attempts (see `gnss_scan_stop()`), so short timeouts mean it never gets one uninterrupted shot at it. Every scan type — scheduled, user, turbo — gets the 150s ceiling until the first fix lands, not just the very first attempt.
6. Data saved to cache (never blocks if drain is running — producer is independent)

**Power efficiency:** GPS only runs during the scan window. Between scans, the device sleeps. Scheduled scans use a one-shot 15s mode — no fast polling, just wake-once-and-save.

### Turbo Mode
- Scans every **~1 minute** with fast 5s polling (independent of consumer drain interval)
- Beeps **once** after each scan completes
- Data is cached — consumer drains on its **own 25-minute schedule**
- Turbo scan entries always save (motion gate bypassed — turbo is an explicit user action)
- Double-press again to exit and restore previous scan interval

### Force Drain (Quad-Press)
- Press 4× rapidly → consumer fast-drains all cached entries; green LED blinks fast for the whole chain
- In range: entries drain at 3s intervals via confirmed uplinks
- Out of range: one attempt, then stops
- Cache empty → completion chirp plays immediately — works even with no entries
- Useful for flushing cached data immediately

### LoRaWAN Toggle (Five-Press, v30)
- Press 5× rapidly → toggles LoRaWAN entirely on or off; state is saved to flash and **persists across reboot**
- **Off**: leaves the network, suspends the radio — GPS scanning and caching keep running unaffected, and BLE (`AT+GPX`) still works, so the tracker becomes a pure local logger. Nothing is transmitted until turned back on.
- **On**: resumes the radio and starts a fresh join, same as a normal boot
- A device with LoRaWAN off still starts a fresh join attempt if 5-clicked back on, or if the config is reset — the flag only controls whether `on_modem_reset()` calls `smtc_modem_join_network()` at all
- Existing configs from before v30 are unaffected — the flag defaults to "on" for any device flashed before this feature existed

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

## BLE Console & GPX Download (v29, extended in v31)

3-click starts BLE advertising, unchanged from prior versions — the device advertises as `T1000-E XXXX`. What's new is what you can do once connected: the same AT-command console already used over USB (`app_at.c`/`app_at_command.c`, ~40 commands) runs identically over this BLE connection via a Nordic-UART-style GATT service, so any generic BLE terminal app works, no companion app required.

**Service and characteristic UUIDs** (custom 128-bit, not the standard Nordic UART UUIDs):

| | UUID |
|---|---|
| Service | `49535343-fe7d-4ae5-8fa9-9fafd205e455` |
| Write (send AT commands here) | `49535343-8841-43f4-a8d4-ecbe34729bb3` |
| Notify (responses arrive here) | `49535343-1e4d-4bd9-ba61-23c647249616` |

**Manual use (any BLE terminal app, e.g. LightBlue, nRF Connect):**
1. 3-click the tracker, connect to `T1000-E XXXX`
2. Subscribe to notifications on the Notify characteristic
3. Write `AT+GPX=?\r\n` (hex: `41 54 2B 47 50 58 3D 3F 0D 0A`) to the Write characteristic
4. The response streams back as a GPX 1.1 `<trk>` — one `<trkpt>` per cache entry that has a GPS fix. WiFi/BLE-only entries are skipped; those need the LNS/backend to resolve a position, which the device doesn't have offline.

**Battery, temperature, and light (v31).** Right after the `<?xml?>` declaration, the response opens with an XML comment carrying a *live* reading — `<!-- battery=82% temp=23.5C light=64 -->` — taken fresh at the moment you send `AT+GPX=?`, not pulled from the cache. This is the one thing you can check without a LoRaWAN uplink, which is the point of using BLE at all — previously there was no way to see current battery while using the tracker over Bluetooth. Every `<trkpt>` also carries its own battery/temp/light as GPX `<extensions>`, decoded from the same fixed byte offsets (2/3-4/5-6) `app_tracker_scan_result_send()` already packs into every GPS-fix cache entry. Battery is 0-100%; light is a 0-100 relative level (not raw lux, despite the sensor function's name); temperature is signed, one decimal place.

**`gpx-downloader.html`** — a self-contained Web Bluetooth page (Chrome/Edge/Opera on desktop or Android; unsupported on iOS, all browsers, since WebKit doesn't implement Web Bluetooth) that does the same thing with a Connect + Download button and saves the result as a `.gpx` file via a normal browser download. Open it directly in Chrome. The live battery/temp/light reading shows up in the page itself as soon as it arrives, before the rest of the download finishes.

**Known quirk:** the device reboots on any BLE disconnect (`hal_mcu_reset()` in `app_ble_all.c`'s `ble_evt_handler`, pre-existing behavior from the original BLE-config flow, not something v29 added). Disconnecting after a GPX download reboots the tracker — expected, matches how the existing BLE-config flow has always worked.

Each individual AT response line sent over BLE is kept under ~50 bytes on purpose (see `AT_GPX_get()` in `app_at.c`): `send_data_to_ble()`'s retry loop doesn't tolerate the SoftDevice error a GATT notification larger than the connected phone's negotiated ATT MTU would return, so longer responses are split across several short notifications rather than sent as one long one.

## Architecture (v31)

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

**Producer starts before join, not after (v28+).** `producer_next_s` is first set in `on_modem_reset()`, which fires immediately after modem init — not in `on_modem_network_joined()`. Scanning and caching begin right away regardless of LoRaWAN join status; the OTAA join keeps retrying automatically in the background. Only the consumer still waits for `on_modem_network_joined()`, since draining requires a join. This matters for a device that's only ever in coverage some of the time (e.g. LoRaWAN only at home) — before this, a power-on outside coverage meant zero scanning, zero caching, for as long as it stayed out of range, which defeated the point of the flash-backed cache.

**GPS epoch corrected (v28).** `gnss_get_epoch()` in `ag3335.c` had a wrong day-conversion formula (`days - (days_in_year - doy) + 1` instead of `days + doy - 1`), which made every embedded payload timestamp ~363 days in the past. Fixed in v28 (along with a latent leap-day loop-bound bug), verified with exhaustive date tests and on real hardware.

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

## Configuring Scan and Drain Interval (Downlink)

**v31 fix:** this downlink was documented and presumably in use, but had no handler behind it — `app_lora_packet_downlink_decode()` referenced `DATA_ID_DW_PACKET_INTEVAL_PARAM` for a side effect (triggering a confirmation uplink) but never had a `case` for it, so the actual byte payload was never read and the interval never changed. Every downlink sent in this format silently did nothing. Fixed in v31; the format below is unchanged from what was already documented, so nothing about how you send it needs to change.

Send a downlink on FPort 5 to change the periodic **scan** interval:

| Interval | Hex Payload |
|---|---|
| 2 min | `81 00 00 00 02` |
| 5 min | `81 00 00 00 05` |
| 10 min | `81 00 00 00 0A` |
| 15 min | `81 00 00 00 0F` |
| 30 min | `81 00 00 00 1E` |
| 60 min | `81 00 00 00 3C` |

**Format:** `81 00 00 HH LL` where HH LL = interval in minutes (big-endian).  
The new interval takes effect on the next scan cycle, and persists across reboot (`app_param.hardware_info.pos_interval`).

**New in v31:** the same downlink shape controls the consumer's **drain** interval — how often cached entries actually get sent, independent of how often scanning happens:

| Interval | Hex Payload |
|---|---|
| 5 min | `83 00 00 00 05` |
| 15 min | `83 00 00 00 0F` |
| 25 min (default) | `83 00 00 00 19` |
| 60 min | `83 00 00 00 3C` |

**Format:** `83 00 00 HH LL`, same big-endian minutes encoding as the scan interval. Unlike the scan interval, this doesn't trigger an immediate action — it just confirms via a power-on uplink and applies on the consumer's next scheduled drain. Persists across reboot (`app_param.hardware_info.drain_interval`).

**How to send either one:** ChirpStack → Device → Queue → FPort 5 → Hex payload.

## Cache

The cache is a 1000-entry ring buffer in RAM. At a deliberate power-off, the oldest 300 undrained entries are checkpointed to flash:

```mermaid
flowchart TB
    subgraph RAM["RAM — volatile"]
        direction TB
        P["Producer — Scan<br/>GPS · WiFi · BLE"]
        C["Ring Buffer Cache<br/><b>1000 entries</b>"]
        D["Consumer — Drain<br/>LoRaWAN uplink · every 25 min"]
    end

    subgraph FLASH["Flash — FDS · 60 KB · persistent"]
        CK["Checkpoint<br/><b>300 entries</b><br/>oldest undrained"]
    end

    NET["LoRaWAN<br/>ChirpStack → MQTT → VictoriaMetrics"]

    P -->|"tracker_cache_save()"| C
    C -->|"drain · app_send_frame()"| D
    D -->|"confirmed uplinks"| NET
    C -.->|"power-off (long-press)"| CK
    CK -.->|"restore + re-queue on boot"| C
```

*Static renders: [SVG](docs/cache-persistence.svg) · [PNG](docs/cache-persistence.png) · [Mermaid source](docs/cache-persistence.mmd)*

- Ring buffer, `TRACKER_CACHE_MAX_DEPTH` (1000) entries maximum
- No TTL expiry — entries are replayed in FIFO order regardless of age (the README previously claimed a 4-hour TTL; it was never actually implemented)
- Concurrent read/write safe — producer appends at write pointer, consumer reads at read pointer
- All entries are confirmed uplinks — only popped on `TXDONE_CONFIRMED`

### Flash persistence (power-off only)

The cache is RAM-only and normally lost on any power-off or reset. As of v27, the oldest `CACHE_PERSIST_MAX_SLOTS` (300) not-yet-drained entries are checkpointed to flash — but **only at deliberate power-off** (long-press), not on a crash or dead battery. See `app_tracker_cache_persist.h` for why: an earlier iteration did periodic flash writes from modem event callbacks and it broke `smtc_modem_alarm_start_timer()` (one packet after joining, then silence). The checkpoint runs once, from `app_user_power_off()`, before the modem alarm and network join are suspended — the SoftDevice coordinates flash writes with radio activity, so the write must complete while the radio is still active — never from any callback or ticking context. Do not add a periodic/incremental checkpoint call without understanding why the previous one was removed.

**Restore clears the checkpoint (v29 fix).** `cache_persist_restore()` runs once at boot and used to read the flash checkpoint into the RAM cache without ever deleting it — so a single checkpoint from one deliberate power-off would replay the same entries over LoRaWAN on *every* future boot, forever, including a plain BLE-disconnect reboot (see above). Fixed: each flash slot is deleted immediately after being read, whether the entry was restored or discarded as corrupt — either way it's been consumed and shouldn't come back.

FDS's total flash budget is 60KB (`FDS_VIRTUAL_PAGES` x `FDS_VIRTUAL_PAGE_SIZE` in `sdk_config.h`), grown from 12KB once the checkpoint became a one-shot write instead of a repeating one — shared with device config storage, still nowhere near enough to persist the full 1000-entry cache, hence the 300-slot bound. On an outage generating more undrained entries than that, the newest ones beyond the window are still lost on a power-off, same risk as before this feature existed, just for a much larger backlog than the original 60-slot version.

## Sample Data

![Sample of data collected by the T1000-E unit](docs/sample-data.jpg)

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
