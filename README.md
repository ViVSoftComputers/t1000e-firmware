# T1000-E Tracker Firmware — v21 Cache Edition

Built: 2026-07-19
Device: Seeed SenseCAP T1000-E (LoRaWAN tracker, nRF52840 + LR1110)
Based on: Seeed-Studio/Seeed-Tracker-T1000-E-for-LoRaWAN-dev-board (commit f3ad9d4)

## What This Firmware Does

Adds a **200-entry ring-buffer cache** to the factory T1000-E firmware. When the device goes out of LoRaWAN range, sensor/GPS data is saved to cache. When it comes back in range, cached entries are replayed automatically.

### Architecture: Producer/Consumer

- **Producer** (`app_tracker_scan_result_send`): Every scan cycle builds the payload and saves it to cache via `tracker_cache_save()`. Never sends directly.
- **Consumer** (`cache_consumer_trigger` + `on_modem_tx_done`): The only code path that calls `app_send_frame()`. Pulls from cache FIFO, sends confirmed uplinks, pops on ACK.

### Cache Details

- 200-entry ring buffer (~33 hours at 10-min intervals)
- 4-hour TTL (entries older than 4h are skipped on replay)
- Drain: 3 seconds between confirmed entries (fast flush when back in range)
- Retry: matches scan interval (no extra battery drain)
- Only pops entries on confirmed delivery (network ACK received)

### Version Identification

Power-on uplink (FPort 5) ends in `XX c0 de` where XX is the firmware version byte:
- v20: `14 c0 de`
- v21: `15 c0 de`
- etc.

## Prerequisites to Build

1. **nRF5 SDK 17.1.0** at `C:\nRF5_SDK_17.1.0_ddde560`
   - Must include SoftDevice s140 7.2.0
   - Must include the Seeed T1000-E project under `examples/ble_peripheral/t1000-e/`

2. **PlatformIO ARM GCC toolchain** at `C:\Users\<user>\.platformio\packages\toolchain-gccarmnoneeabi\`
   - GCC 12.3.1 (arm-none-eabi-gcc)
   - Install via PlatformIO: `pio platform install nordicnrf52`

3. **Python 3** with no special dependencies (stdlib only)

4. **uf2conv.py** from Microsoft UF2 tools — placed at `C:\Users\<user>\uf2conv.py`

## Source Files Modified (relative to Seeed repo)

| File | Changes |
|------|---------|
| `apps/examples/11_lorawan_tracker/main_lorawan_tracker.c` | Cache integration: include, drain state, producer/consumer logic |
| `t1000_e/tracker/inc/app_tracker_cache.h` | Cache engine header (NEW) |
| `t1000_e/tracker/src/app_tracker_cache.c` | Cache engine implementation (NEW) |
| `t1000_e/tracker/src/app_lora_packet.c` | Version byte in power-on uplink, FIRMWARE_VERSION include |
| `pca10056/s140/11_ses_lorawan_tracker/build_factory.py` | GCC build script (NEW) |
| `pca10056/s140/11_ses_lorawan_tracker/t1000_e_dev_kit_pca10056.ld` | GCC linker script (replaced SES one) |

## How to Build

```bash
cd C:\nRF5_SDK_17.1.0_ddde560\examples\ble_peripheral\t1000-e\pca10056\s140\11_ses_lorawan_tracker
python3 build_factory.py
```

Output:
- `Output/Debug/Exe/t1000_e_dev_kit_pca10056.elf` — compiled firmware
- `Output/Debug/Exe/t1000-e-vXX.uf2` — combined UF2 (SoftDevice + app)

### Creating a Flashable UF2 (family 0x28860057, no MBR)

The build script produces a UF2 with wrong family ID. Fix it:

```bash
UF2CONV="C:/Users/ViV/uf2conv.py"
SD_HEX="C:/nRF5_SDK_17.1.0_ddde560/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex"
EXE="Output/Debug/Exe"

# Build SD + App UF2s with correct family
python3 "$UF2CONV" -f 0x28860057 -b 0x0000 -c -o "$EXE/sd.uf2" "$SD_HEX"
python3 "$UF2CONV" -f 0x28860057 -b 0x27000 -c -o "$EXE/app.uf2" "$EXE/t1000_e_dev_kit_pca10056.hex"

# Concatenate, strip MBR blocks (0x0-0xFFF), fix sequence numbers
python3 -c "
import struct
sd = open('$EXE/sd.uf2','rb').read()
ap = open('$EXE/app.uf2','rb').read()
c = sd + ap
v = [c[b*512:(b+1)*512] for b in range(len(c)//512)
     if struct.unpack('<I',c[b*512:b*512+4])[0]==0x0A324655
     and struct.unpack('<I',c[b*512+4:b*512+8])[0]==0x9E5D5157
     and struct.unpack('<I',c[b*512+12:b*512+16])[0]>=0x1000]
t = len(v); o = bytearray()
for i,b in enumerate(v):
    bb = bytearray(b)
    struct.pack_into('<II',bb,20,i,t)
    o.extend(bb)
open('t1000-e-vXX-cache.uf2','wb').write(o)
print(f'Done: {len(o)} bytes, {t} blocks')
"
```

### Flashing

1. Connect T1000-E via USB-C
2. Double-press the button rapidly — device enters UF2 bootloader mode (appears as a USB drive)
3. Drag `t1000-e-vXX-cache.uf2` onto the drive
4. Device reboots automatically

## Key Design Decisions

- **GCC not SES**: SEGGER Embedded Studio couldn't compile the project (missing startup files, GCC 15 strictness). Use PlatformIO's arm-none-eabi-gcc 12.3.1.
- **gcc_startup_nrf52840.S**: SES's thumb_crt0.s is SEGGER-specific. Must use nRF5 SDK's GCC startup file for the vector table.
- **UF2 family 0x28860057**: T1000-E bootloader requires this family ID. Standard nRF52840 (0xADA52840) is rejected.
- **No MBR blocks**: UF2 blocks at 0x0-0xFFF cause bootloader rejection. Must be stripped.
- **Unconfirmed drain? NO**: v16 tried unconfirmed drain, entries were lost (SENT → popped but never arrived). Confirmed drain with ACK-based pop is correct.
- **No callback cascade**: Calling app_send_frame from inside on_modem_tx_done is not re-entrant safe. Use timer-based cascade (3s alarm between entries).

## Pitfalls

- **Build cache**: If FIRMWARE_VERSION changes but the binary doesn't update, delete `Output/Debug/Obj/` and rebuild.
- **Timer callbacks**: Must have signature `void handler(void *p_context)`, NOT `void handler(void)`. Stack corruption otherwise.
- **Escape sequences**: The patch tool double-escapes `\n` → `\\n`. Check the binary after patching string literals.
- **addr offset**: UF2 address field is at byte offset 12, not 8.

## v21 vs Base Firmware

| Feature | Factory | v21 Cache |
|---------|---------|-----------|
| Out-of-range behavior | Data lost | Cached & replayed |
| Cache size | None | 200 entries |
| Drain speed | N/A | 3s between entries |
| Battery impact (offline) | Normal scans | Normal scans (retry = scan interval) |
| Power-on marker | no marker | `XX c0 de` (versioned) |
| Sensor uplinks | `be ef` | `be ef` |
