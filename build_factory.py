#!/usr/bin/env python3
"""
T1000-E Cache Firmware Build Script
Copyright (C) 2026 ViVSoft Computers LLC

Builds the T1000-E tracker firmware using PlatformIO ARM GCC (GCC 12.3.1).
Compiles all source files from the SES .emProject, links with SoftDevice s140 7.2.0,
and produces a combined UF2 for drag-drop flashing onto the device.

Usage: python3 build_factory.py
"""
import os, sys, re, subprocess, time, shutil, datetime
from pathlib import Path

# Version tag — bump this before each build for a new UF2
VERSION = "v13"  # Version byte in payload + fixed combined UF2 + ENTRY(main)
BUILD_DATE = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")

START = time.time()

# =========== CONFIG ===========
PROJECT = Path(r"C:\nRF5_SDK_17.1.0_ddde560\examples\ble_peripheral\t1000-e")
SKD = Path(r"C:\nRF5_SDK_17.1.0_ddde560")
BUILD_DIR = PROJECT / "pca10056" / "s140" / "11_ses_lorawan_tracker"
GCC = r"C:\Users\ViV\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-gcc"
GXX = r"C:\Users\ViV\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-g++"
OBJCOPY = r"C:\Users\ViV\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-objcopy"
SIZE = r"C:\Users\ViV\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-size"
UF2CONV = r"C:\Users\ViV\uf2conv.py"

OUT_DIR = BUILD_DIR / "Output" / "Debug"
OBJ_DIR = OUT_DIR / "Obj"
EXE_DIR = OUT_DIR / "Exe"
LD_SCRIPT = BUILD_DIR / "t1000_e_dev_kit_pca10056.ld"

os.makedirs(OBJ_DIR, exist_ok=True)
os.makedirs(EXE_DIR, exist_ok=True)

# =========== PARSE PROJECT FILE ===========
emproject = BUILD_DIR / "t1000_e_dev_kit_pca10056.emProject"
with open(emproject, 'r') as f:
    content = f.read()

# Extract include dirs
match = re.search(r'c_user_include_directories="([^"]+)"', content)
inc_dirs_raw = match.group(1).split(';')

inc_flags = []
for d in inc_dirs_raw:
    d = d.strip()
    if d:
        resolved = str((BUILD_DIR / d).resolve())
        inc_flags.append(resolved)

# Include SDK for common headers  
inc_flags.append(str(SKD / "components"))
inc_flags.append(str(PROJECT))
inc_flags.append(str(BUILD_DIR / "config_sd"))

# Extract source files
src_pattern = re.compile(r'file_name="([^"]+\.(?:c|cpp))"')
srcs = []
se_exclude_basenames = ['aes.c', 'cmac.c', 'soft_se.c']
for m in src_pattern.finditer(content):
    s = m.group(1)
    if not s.startswith('$'):
        # Try resolving relative to BUILD_DIR first
        p1 = (BUILD_DIR / s).resolve()
        p2 = (PROJECT / s).resolve()
        p3 = (SKD / s).resolve()
        if p1.exists() or p2.exists() or p3.exists():
            # Exclude software SE files (conflicts with LR11xx HW crypto)
            if os.path.basename(s) in se_exclude_basenames and 'soft_secure_element' in s.replace('\\', '/'):
                continue
            fp = str(p1 if p1.exists() else (p2 if p2.exists() else p3))
            srcs.append(fp)

# Also find all .c files in lora_basics_modem/smtc_modem_core recursively
modem_core = PROJECT / "lora_basics_modem" / "smtc_modem_core"
additional_srcs = []
for root, dirs, files in os.walk(modem_core):
    for f in files:
        if f.endswith('.c'):
            s = str(Path(root) / f)
            # Skip unused regions, radios, and features
            skip_patterns = ['cn_470', 'ww2g4', 'fragmentation', 'ral_llcc68', 'ral_sx126x', 'ral_sx128x', 'ralf_sx128x', 'ral_lr1121', 'ralf_lr1121']
            if any(p in s.lower().replace('\\', '/') for p in skip_patterns):
                continue
            additional_srcs.append(s)

# Add all found
# Also exclude software SE (uses LR11xx HW crypto)
se_exclude = ['soft_secure_element/aes.c', 'soft_secure_element/cmac.c', 'soft_secure_element/soft_se.c']

existing = set(srcs)
for s in additional_srcs:
    if s not in existing and not any(p in s.lower().replace('\\', '/') for p in se_exclude):
        srcs.append(s)
        existing.add(s)

print(f"Source files: {len(srcs)}")

# Add GCC startup file (vector table) — ESSENTIAL for GCC builds
# The SES project uses thumb_crt0.s which is SEGGER-specific
gcc_startup = str((SKD / "modules" / "nrfx" / "mdk" / "gcc_startup_nrf52840.S").resolve())
if os.path.exists(gcc_startup):
    srcs.insert(0, gcc_startup)  # MUST be first — contains the vector table
    print(f"Added GCC startup: {gcc_startup}")

# Add cache system source file
cache_c = str((PROJECT / "t1000_e" / "tracker" / "src" / "app_tracker_cache.c").resolve())
if os.path.exists(cache_c) and cache_c not in srcs:
    srcs.append(cache_c)
    print(f"Added cache source: {cache_c}")
else:
    print(f"Cache source {'exists' if os.path.exists(cache_c) else 'NOT FOUND'}: {cache_c}")

# =========== COMPILER FLAGS ===========
cpu_flags = ["-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"]

defines = [
    "-DNRF52840_XXAA", "-DNRF_SD_BLE_API_VERSION=7", "-DS140", "-DSOFTDEVICE_PRESENT",
    "-DBOARD_PCA10056", "-DCONFIG_GPIO_AS_PINRESET", "-DFLOAT_ABI_HARD",
    "-DINITIALIZE_USER_SECTIONS", "-DNO_VTOR_CONFIG",
    "-DAPP_TIMER_V2", "-DAPP_TIMER_V2_RTC1_ENABLED",
    "-DLR11XX", "-DLR11XX_TRANSCEIVER", "-DENABLE_MODEM_GNSS_FEATURE",
    "-DADD_SMTC_ALC_SYNC", "-DADD_SMTC_FILE_UPLOAD", "-DADD_SMTC_STREAM",
    "-DAPP_TRACKER", "-DRP2_103", "-DSMTC_MULTICAST",
    "-DTASK_EXTENDED_1", "-DTASK_EXTENDED_2",
    "-DHAL_DBG_TRACE=1", "-DMODEM_HAL_DBG_TRACE=1",
    "-DREGION_AS_923", "-DREGION_AU_915", "-DREGION_EU_868",
    "-DREGION_IN_865", "-DREGION_KR_920", "-DREGION_RU_864", "-DREGION_US_915",
    "-DDEBUG", "-DDEBUG_NRF",
]

sdk_config_path = str((BUILD_DIR / ".." / "config_sd" / "sdk_config.h").resolve())

cflags = cpu_flags + defines + [f"-I{d}" for d in inc_flags] + [
    "-include", sdk_config_path,
    "-O0", "-g3",
    "-ffunction-sections", "-fdata-sections",
    "-std=gnu11",
    "-Wno-unused-but-set-variable", "-Wno-unused-variable",
    "-Wno-unused-parameter", "-Wno-unused-function",
    "-Wno-pointer-sign", "-Wno-missing-braces",
    "-Wno-address-of-packed-member",
    "-Wno-format", "-Wno-discarded-qualifiers",
    "-Wno-int-conversion",
    "-Wno-implicit-function-declaration",
]

# =========== LINKER SCRIPT ===========
# Use the standard nRF5 SDK GCC linker script (designed for gcc_startup_nrf52840.S)
src_ld = SKD / "config" / "nrf52840" / "armgcc" / "generic_gcc_nrf52.ld"
if src_ld.exists():
    ld_text = src_ld.read_text()
    # Patch memory layout for this project
    ld_text = ld_text.replace(
        'FLASH (rx) : ORIGIN = 0x27000, LENGTH = 0xd9000',
        'FLASH (rx) : ORIGIN = 0x27000, LENGTH = 0xc6000')
    ld_text = ld_text.replace(
        'RAM (rwx) :  ORIGIN = 0x20005978, LENGTH = 0x3a688',
        'RAM (rwx) :  ORIGIN = 0x20010000, LENGTH = 0x30000')
    LD_SCRIPT.write_text(ld_text)
    print(f"Linker script: {LD_SCRIPT} (nRF5 SDK GCC, patched for project)")
else:
    print(f"ERROR: Linker script not found at {src_ld}")
    sys.exit(1)

ldflags = cpu_flags + [
    f"-T{LD_SCRIPT}",
    f"-L{SKD / 'config' / 'nrf52840' / 'armgcc'}",
    f"-L{SKD / 'modules' / 'nrfx' / 'mdk'}",
    "-Wl,--gc-sections",
    # Force-keep cache functions and main (--gc-sections discards them otherwise)
    "-Wl,--undefined=tracker_cache_save",
    "-Wl,--undefined=tracker_cache_count",
    "-Wl,--undefined=tracker_cache_get",
    "-Wl,--undefined=tracker_cache_pop",
    "-Wl,--undefined=tracker_cache_clear",
    "-Wl,--undefined=main",
    f"-Wl,-Map={EXE_DIR / 't1000_e_dev_kit_pca10056.map'}",
    "-specs=nosys.specs", "-lc",
    "-Wl,--no-wchar-size-warning",
]

# =========== COMPILE ===========
def compile_one(src_path, obj_dir):
    """Compile a single C file"""
    # Create a unique object name based on relative path
    try:
        rel = os.path.relpath(src_path, str(SKD))
    except ValueError:
        rel = os.path.relpath(src_path, str(PROJECT))
    rel = rel.replace('..', '__').replace('/', '_').replace('\\', '_').replace(':', '_')
    obj_path = obj_dir / f"{rel}.o"
    
    # Check cache
    if obj_path.exists():
        src_mtime = os.path.getmtime(src_path)
        obj_mtime = os.path.getmtime(obj_path)
        if obj_mtime > src_mtime:
            return obj_path  # up to date
    
    obj_path.parent.mkdir(parents=True, exist_ok=True)
    
    cmd = [GCC, "-c"] + cflags + [src_path, "-o", str(obj_path)]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    
    if result.returncode != 0:
        # Show error
        err_lines = result.stderr.split('\n')
        for line in err_lines[:5]:
            print(f"  ERROR: {line}")
        return None
    return obj_path

# Compile all sources
objects = []
failures = []
for i, src in enumerate(srcs):
    if i % 10 == 0:
        elapsed = time.time() - START
        print(f"  [{i}/{len(srcs)}] ({elapsed:.0f}s)...", end=' ', flush=True)
        print(f"compiling: {os.path.basename(src)}")
    
    obj = compile_one(src, OBJ_DIR)
    if obj:
        objects.append(str(obj))
    else:
        failures.append(src)
        if len(failures) >= 20:
            print("Too many failures, aborting")
            break

elapsed = time.time() - START
print(f"\n=== Compilation: {len(objects)} OK, {len(failures)} failed in {elapsed:.0f}s ===")

if failures:
    print("Failed files:")
    for f in failures[:10]:
        print(f"  {f}")
    sys.exit(1)

# =========== LINK ===========
print("\n=== Linking... ===")
elf_path = EXE_DIR / "t1000_e_dev_kit_pca10056.elf"
hex_path = EXE_DIR / "t1000_e_dev_kit_pca10056.hex"

# Use response file to avoid Windows command line length limit
rsp_path = EXE_DIR / "link.rsp"
with open(rsp_path, 'w') as f:
    for obj in objects:
        # GCC on MSYS2 needs forward slashes in response files
        f.write(f"\"{obj.replace(chr(92), '/')}\"\n")

link_cmd = [GCC] + ldflags + [f"@{rsp_path}", "-o", str(elf_path)]
result = subprocess.run(link_cmd, capture_output=True, text=True, timeout=120)

if result.returncode != 0:
    print("LINK FAILED:")
    for line in result.stderr.split('\n')[:30]:
        print(f"  {line}")
    sys.exit(1)

# =========== POST-PROCESSING ===========
print("\n=== Post-processing ===")

# Run size
subprocess.run([SIZE, str(elf_path)])

# Convert to hex  
subprocess.run([OBJCOPY, "-O", "ihex", str(elf_path), str(hex_path)], check=True)
hex_size = os.path.getsize(hex_path)
print(f"\nApp hex: {hex_path} ({hex_size} bytes)")

# Build application-only UF2
uf2_app_path = EXE_DIR / f"t1000-e-{VERSION}-app.uf2"
subprocess.run(["python3", UF2CONV, "-f", "0xADA52840", "-b", "0x27000",
                 "-c", "-o", str(uf2_app_path), str(hex_path)], check=True)
print(f"App UF2: {uf2_app_path} ({os.path.getsize(uf2_app_path)} bytes)")

# Also build combined hex (SoftDevice + app) — bypass mergehex.py type-02 bug
sd_hex = SKD / "components" / "softdevice" / "s140" / "hex" / "s140_nrf52_7.2.0_softdevice.hex"
uf2_combined_path = EXE_DIR / f"t1000-e-{VERSION}.uf2"
if sd_hex.exists():
    # Step 1: Generate SoftDevice-only UF2 at base 0x0000
    sd_uf2_path = EXE_DIR / "s140_nrf52_7.2.0_softdevice.uf2"
    subprocess.run(["python3", UF2CONV, "-f", "0xADA52840", "-b", "0x0000",
                     "-c", "-o", str(sd_uf2_path), str(sd_hex)], check=True, timeout=60)
    sd_size = os.path.getsize(sd_uf2_path)
    print(f"SoftDevice UF2: {sd_uf2_path} ({sd_size} bytes)")

    # Step 2: Concatenate SoftDevice UF2 + App UF2 at binary level.
    # UF2 blocks are self-describing (each block has its own address in the header),
    # so simple concatenation works — no central metadata to fix up.
    with open(sd_uf2_path, 'rb') as src:
        combined = bytearray(src.read())
    with open(uf2_app_path, 'rb') as src:
        combined.extend(src.read())
    with open(uf2_combined_path, 'wb') as dst:
        dst.write(combined)

    combined_size = os.path.getsize(uf2_combined_path)
    print(f"Combined UF2: {uf2_combined_path} ({combined_size} bytes)")

    # Step 3: VERIFY — extract the app region from the combined UF2 and compare
    print("\nVerifying combined UF2 contains app code at 0x27000...")
    import struct
    with open(uf2_combined_path, 'rb') as f:
        raw = f.read()
    num_blocks = len(raw) // 512
    app_blocks = 0
    for bn in range(num_blocks):
        off = bn * 512
        magic0, magic1, flags, addr, sz, _, _, _ = struct.unpack('<IIIIIIII', raw[off:off+32])
        if magic0 == 0x0A324655 and magic1 == 0x9E5D5157 and not (flags & 1):
            if addr >= 0x27000:
                app_blocks += 1
    if app_blocks > 0:
        print(f"  PASS: {app_blocks} app blocks found at 0x27000+ in combined UF2")
        print(f"\\nUF2 ready for drag-drop flashing: {uf2_combined_path}")
    else:
        print(f"  FAIL: ZERO app blocks at 0x27000+ — combined UF2 is corrupt!")
        print("  Falling back to app-only UF2 for verification.")
        uf2_combined_path = uf2_app_path
else:
    print(f"WARNING: SoftDevice hex not found at {sd_hex}")
    uf2_combined_path = uf2_app_path
    print(f"App-only UF2: {uf2_app_path}")

elapsed = time.time() - START
print(f"\n=== BUILD COMPLETE in {elapsed:.0f}s ===")
