#!/usr/bin/env python3
"""
Generate and run the GCC build command for T1000-E factory firmware
using PlatformIO ARM GCC toolchain.
"""
import os, sys, re, subprocess
from pathlib import Path

# =========== CONFIG ===========
PROJECT = Path(r"C:\nRF5_SDK_17.1.0_ddde560\examples\ble_peripheral\t1000-e")
SKD = Path(r"C:\nRF5_SDK_17.1.0_ddde560")
BUILD_DIR = PROJECT / "pca10056" / "s140" / "11_ses_lorawan_tracker"
GCC = r"C:\Users\ViV\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-gcc"
OBJCOPY = r"C:\Users\ViV\.platformio\packages\toolchain-gccarmnoneeabi\bin\arm-none-eabi-objcopy"
OUT_DIR = BUILD_DIR / "Output" / "Debug"
OBJ_DIR = OUT_DIR / "Obj"
EXE_DIR = OUT_DIR / "Exe"

# =========== PARSE PROJECT FILE ===========
emproject = BUILD_DIR / "t1000_e_dev_kit_pca10056.emProject"
with open(emproject, 'r') as f:
    content = f.read()

# Extract include dirs
match = re.search(r'c_user_include_directories="([^"]+)"', content)
inc_dirs_raw = match.group(1).split(';')

# Resolve relative to BUILD_DIR
def resolve_inc(rel):
    if rel.startswith('$'):
        return None
    return str((BUILD_DIR / rel).resolve())

inc_flags = []
for d in inc_dirs_raw:
    d = d.strip()
    if d:
        resolved = resolve_inc(d)
        if resolved:
            inc_flags.append(resolved)

# Extract source files  
src_pattern = re.compile(r'file_name="([^"]+\.(?:c|cpp))"')
srcs = []
for m in src_pattern.finditer(content):
    s = m.group(1)
    if not s.startswith('$'):
        resolved = (BUILD_DIR / s).resolve()
        resolved2 = (PROJECT / s).resolve()
        # Try both
        if resolved.exists():
            srcs.append(str(resolved))
        elif resolved2.exists():
            srcs.append(str(resolved2))

# CPU & defines
cpu_flags = "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16".split()
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

# sdk_config.h 
sdk_config_path = str((BUILD_DIR / ".." / "config_sd" / "sdk_config.h").resolve())

cflags_base = cpu_flags + defines + [f"-I{d}" for d in inc_flags] + [
    "-include", sdk_config_path,
    "-O0", "-g3",
    "-ffunction-sections", "-fdata-sections",
    "-std=gnu11",
    "-Wno-unused-but-set-variable", "-Wno-unused-variable",
    "-Wno-unused-parameter", "-Wno-unused-function",
    "-Wno-pointer-sign", "-Wno-missing-braces",
    "-Wno-address-of-packed-member",
    "-Wno-format",
]

# =========== COMPILE ===========
os.makedirs(OBJ_DIR, exist_ok=True)
os.makedirs(EXE_DIR, exist_ok=True)

print(f"Building with GCC: {GCC}")
print(f"Found {len(srcs)} source files")
print(f"Found {len(inc_flags)} include directories")
print()

# Try compiling first file to verify toolchain works
test_src = srcs[0]
print(f"Test compile: {test_src}")
cmd = [GCC, "-c"] + cflags_base + [test_src, "-o", str(OBJ_DIR / "test.o")]
print(" ".join(cmd))
result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
print(f"Exit: {result.returncode}")
if result.returncode != 0:
    # Show first errors
    for line in result.stderr.split('\n')[:20]:
        print(line)
else:
    print("OK - compiled successfully!")
    os.remove(str(OBJ_DIR / "test.o"))
