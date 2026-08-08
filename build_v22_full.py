#!/usr/bin/env python3
"""
T1000-E v22 ONE-CLICK BUILD + UF2 CREATOR
Copyright (C) 2026 ViVSoft Computers LLC

1. Builds the firmware using build_factory.py
2. Creates a flashable UF2 with the correct T1000-E family ID
3. Output: t1000-e-v22.uf2 — drag onto T1000-E in bootloader mode
"""

import subprocess, sys, os
from pathlib import Path

BUILD_DIR = Path(r"C:\nRF5_SDK_17.1.0_ddde560\examples\ble_peripheral\t1000-e\pca10056\s140\11_ses_lorawan_tracker")
SD_HEX    = r"C:\nRF5_SDK_17.1.0_ddde560\components\softdevice\s140\hex\s140_nrf52_7.2.0_softdevice.hex"
UF2CONV   = r"C:\Users\ViV\uf2conv.py"
EXE_DIR   = BUILD_DIR / "Output" / "Debug" / "Exe"

def run(cmd, **kw):
    print(f"  > {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    r = subprocess.run(cmd, shell=isinstance(cmd, str), **kw)
    if r.returncode != 0:
        print(f"  FAILED (exit {r.returncode})")
        sys.exit(1)

# Step 1: Build
print("=== STEP 1: BUILD FIRMWARE ===")
os.chdir(BUILD_DIR)
run([sys.executable, "build_factory.py"])

# Step 2: Create SD UF2
print("\n=== STEP 2: CREATE SOFTDEVICE UF2 ===")
run([sys.executable, UF2CONV, "-f", "0x28860057", "-b", "0x0000", "-c",
     "-o", str(EXE_DIR / "sd.uf2"), SD_HEX])

# Step 3: Create App UF2
print("\n=== STEP 3: CREATE APP UF2 ===")
run([sys.executable, UF2CONV, "-f", "0x28860057", "-b", "0x27000", "-c",
     "-o", str(EXE_DIR / "app.uf2"), str(EXE_DIR / "t1000_e_dev_kit_pca10056.hex")])

# Step 4: Merge, strip MBR blocks, fix sequence numbers
print("\n=== STEP 4: MERGE + STRIP MBR + FIX SEQUENCES ===")
import struct
sd = open(EXE_DIR / "sd.uf2", "rb").read()
ap = open(EXE_DIR / "app.uf2", "rb").read()
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

out_path = BUILD_DIR / "t1000-e-v22.uf2"
open(out_path, "wb").write(o)

# Verify
b0 = o[:32]
magic0, magic1, flags, addr, sz, seq, total, fam = struct.unpack('<IIIIIIII', b0)
assert magic0 == 0x0A324655 and magic1 == 0x9E5D5157, "Bad UF2 magic!"
assert fam == 0x28860057, f"Wrong family: 0x{fam:08x} (expected 0x28860057)"
assert addr >= 0x1000, f"MBR block at 0x{addr:08x}!"

print(f"\n✓ DONE: {len(o)} bytes, {t} blocks -> {out_path}")
print(f"  Family: 0x{fam:08x}  |  First block: 0x{addr:08x}  |  Sequence: {seq}/{total}")
print("\nTo flash: double-press T1000-E button → drag UF2 onto USB drive.")
