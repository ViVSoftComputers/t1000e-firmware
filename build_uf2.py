#!/usr/bin/env python3
"""
build_uf2.py — Deterministic T1000-E firmware build → correct-family UF2.

Encodes the ENTIRE verified build procedure so a local model (or a human, or
a cron job) can produce a flashable UF2 without reasoning through the traps.

Usage:
    python3 build_uf2.py [VERSION]

Defaults VERSION="v32". Run from anywhere (paths are absolute).

What it does (in order):
  1. Builds the app hex via build_factory.py (compile 181 files + link).
  2. Regenerates the SoftDevice + app UF2s with the CORRECT family 0x28860057
     (build_factory.py emits the WRONG family 0xADA52840 which the T1000-E
     bootloader SILENTLY rejects).
  3. Concatenates sd.uf2 + app.uf2 (ALL blocks — do NOT strip MBR blocks; the
     shipped/verified firmware keeps blocks below 0x1000) and renumbers
     numBlocks/totalBlocks.
  4. VERIFIES the output and FAILS LOUDLY if anything is wrong.
  5. Copies the final UF2 into the GitHub repo working tree.

The critical correctness rules (violating any = silent device rejection):
  - Family MUST be 0x28860057 (NOT 0xADA52840)
  - Keep ALL blocks (no address filtering — MBR blocks are expected)
  - Renumber numBlocks/totalBlocks after concat
"""
import os, sys, subprocess, struct, shutil, datetime

# ---------- Paths (Windows-style, absolute) ----------
SDK        = r"C:\nRF5_SDK_17.1.0_ddde560"
PROJECT    = SDK + r"\examples\ble_peripheral\t1000-e"
BUILD_DIR  = PROJECT + r"\pca10056\s140\11_ses_lorawan_tracker"
OBJ_DIR    = BUILD_DIR + r"\Output\Debug\Obj"
EXE_DIR    = BUILD_DIR + r"\Output\Debug\Exe"
SD_HEX     = SDK + r"\components\softdevice\s140\hex\s140_nrf52_7.2.0_softdevice.hex"
APP_HEX    = EXE_DIR + r"\t1000_e_dev_kit_pca10056.hex"
UF2CONV    = r"C:\Users\ViV\uf2conv.py"
REPO       = r"C:\herman\t1000e-firmware-v22"

FAMILY_OK  = 0x28860057
FAMILY_BAD = 0xADA52840

VERSION = sys.argv[1] if len(sys.argv) > 1 else "v32"


def run(cmd, cwd=None):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd, timeout=600)
    if r.returncode != 0:
        print("COMMAND FAILED:", " ".join(cmd))
        print(r.stdout[-2000:])
        print(r.stderr[-2000:])
        sys.exit(1)
    return r


def main():
    print(f"=== T1000-E build → {VERSION} (correct-family UF2) ===\n")

    # 1. Clean object cache (build_factory.py only tracks source mtime, NOT
    #    header deps — so a header change needs a clean rebuild or stale .o
    #    files get linked).
    print("[1/5] Cleaning object cache ...")
    if os.path.isdir(OBJ_DIR):
        shutil.rmtree(OBJ_DIR)
    print("      done")

    # 2. Compile + link via build_factory.py (sets VERSION internally for its
    #    own output naming, but we regenerate the UF2 ourselves in step 3).
    print("[2/5] Compiling + linking (build_factory.py) ...")
    r = run([sys.executable, "build_factory.py"], cwd=BUILD_DIR)
    # build_factory.py always exits 0 even on compile failures, so inspect its
    # printed summary line.
    if "0 failed" not in (r.stdout or ""):
        print("BUILD FAILED — see build output above")
        sys.exit(1)
    # Surface the summary
    for line in (r.stdout or "").splitlines():
        if "Compilation:" in line or "Compilation" in line or "failed" in line:
            print("     ", line.strip())

    # 3. Regenerate UF2s with the CORRECT family.
    print("[3/5] Converting hex → UF2 with family 0x28860057 ...")
    run([sys.executable, UF2CONV, "-f", hex(FAMILY_OK), "-b", "0x0000",
         "-c", "-o", os.path.join(EXE_DIR, "sd.uf2"), SD_HEX])
    run([sys.executable, UF2CONV, "-f", hex(FAMILY_OK), "-b", "0x27000",
         "-c", "-o", os.path.join(EXE_DIR, "app.uf2"), APP_HEX])
    print("      sd.uf2 + app.uf2 written")

    # 4. Concatenate (all blocks) + renumber.
    print("[4/5] Concatenating + renumbering ...")
    sd = open(os.path.join(EXE_DIR, "sd.uf2"), "rb").read()
    ap = open(os.path.join(EXE_DIR, "app.uf2"), "rb").read()
    combined = sd + ap
    total = len(combined) // 512
    out = bytearray()
    for i in range(total):
        b = bytearray(combined[i * 512:(i + 1) * 512])
        struct.pack_into("<II", b, 20, i, total)
        out.extend(b)
    final_path = os.path.join(EXE_DIR, f"t1000-e-{VERSION}.uf2")
    open(final_path, "wb").write(out)
    print(f"      {final_path}: {len(out)} bytes, {total} blocks")

    # 5. VERIFY — fail loudly on any violation.
    print("[5/5] Verifying ...")
    d = out
    n = len(d) // 512
    fams = set(); bad = 0; minaddr = 0xFFFFFFFF
    app_blocks = 0; sub1000 = 0
    for i in range(n):
        off = i * 512
        m0, m1, flags, addr, sz, seq, tot, fam = struct.unpack("<IIIIIIII", d[off:off + 32])
        if not (m0 == 0x0A324655 and m1 == 0x9E5D5157):
            print(f"  FAIL: bad magic at block {i}")
            sys.exit(1)
        fams.add(fam)
        if fam != FAMILY_OK:
            bad += 1
        minaddr = min(minaddr, addr)
        if addr >= 0x27000:
            app_blocks += 1
        if addr < 0x1000:
            sub1000 += 1

    if bad:
        print(f"  FAIL: {bad} blocks have wrong family (expected 0x{FAMILY_OK:08x})")
        sys.exit(1)
    if app_blocks == 0:
        print("  FAIL: zero app blocks at 0x27000+")
        sys.exit(1)

    print(f"  PASS: {n} blocks, family 0x{FAMILY_OK:08x} on all blocks")
    print(f"  PASS: {app_blocks} app blocks at 0x27000+")
    print(f"  PASS: min_addr=0x{minaddr:x}, {sub1000} MBR blocks below 0x1000")
    print(f"\n  → Flashable UF2: {final_path}\n")

    # Copy into the repo working tree (UF2s are gitignored; force-add later).
    repo_dst = os.path.join(REPO, f"t1000-e-{VERSION}.uf2")
    shutil.copyfile(final_path, repo_dst)
    print(f"  Copied to repo: {repo_dst}")
    print("\n=== BUILD SUCCESS ===")


if __name__ == "__main__":
    main()
