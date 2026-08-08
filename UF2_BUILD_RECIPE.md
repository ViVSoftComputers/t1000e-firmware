# T1000-E Flashable UF2 Build Recipe — v22

**Last verified:** 2026-08-08 — flashed and confirmed working on T1000-E hardware.

## Quick Reference

```
UF2CONV="C:/Users/ViV/uf2conv.py"
SD_HEX="C:/nRF5_SDK_17.1.0_ddde560/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex"
EXE_DIR="Output/Debug/Exe"

# Step 1: Build the firmware .hex
cd C:\nRF5_SDK_17.1.0_ddde560\examples\ble_peripheral\t1000-e\pca10056\s140\11_ses_lorawan_tracker
python3 build_factory.py

# Step 2: Create SoftDevice UF2 with T1000-E family
python3 "$UF2CONV" -f 0x28860057 -b 0x0000 -c -o "$EXE_DIR/sd.uf2" "$SD_HEX"

# Step 3: Create App UF2 with T1000-E family
python3 "$UF2CONV" -f 0x28860057 -b 0x27000 -c -o "$EXE_DIR/app.uf2" "$EXE_DIR/t1000_e_dev_kit_pca10056.hex"

# Step 4: Concatenate, strip MBR blocks, fix sequence numbers
python3 -c "
import struct
sd = open('$EXE_DIR/sd.uf2','rb').read()
ap = open('$EXE_DIR/app.uf2','rb').read()
c = sd + ap
v = [c[b*512:(b+1)*512] for b in range(len(c)//512)
     if struct.unpack('<I',c[b*512:b*512+4])[0]==0x0A324655
     and struct.unpack('<I',c[b*512+4:b*512+8])[0]==0x9E5D5157
     and struct.unpack('<I',c[b*512+12:b*512+16])[0]>=0x1000]
t = len(v); o = bytearray()
for i,b in enumerate(v):
    bb = bytearray(b)
    struct.pack_into('<II', bb, 20, i, t)
    o.extend(bb)
open('t1000-e-v22.uf2','wb').write(o)
print(f'Done: {len(o)} bytes, {t} blocks')
"
```

## Critical Rules

1. **Family ID MUST be `0x28860057`** — NOT `0xADA52840`. The T1000-E bootloader silently rejects `0xADA52840`.
2. **MBR blocks (addr < 0x1000) MUST be stripped** — they cause bootloader rejection.
3. **Sequence numbers MUST be fixed** after concatenation — both `numBlocks` and `totalBlocks` in each UF2 block header.
4. **SoftDevice MUST be included** — the UF2 needs both SD s140 7.2.0 + app firmware.
5. **Build with `build_factory.py`** — it uses the correct linker script (`generic_gcc_nrf52.ld` patched), GCC startup file, and source discovery.

## Pitfalls

- do **NOT** use `t1000_e_dev_kit_pca10056.ld` directly — it has `INCLUDE "nrf_common.ld"` that fails without MDK path
- `build_factory.py` uses `generic_gcc_nrf52.ld` from the nRF5 SDK, patched with project memory regions
- The `-Wno-implicit-function-declaration` flag is needed for the stock Seeed code (pre-existing warnings)
- `PRINTF("\r\n")` escape sequences get mangled by the patch tool — verify after patching with `grep -n 'SKIP_IT\|USER_SCAN\|lora sos' *.c`
- Always clear `Output/Debug/Obj/` before rebuild after source changes (stale caching)
- The app UF2 alone (without SD) is ~61KB — too small to flash. Combined with SD it's ~1.2MB.
