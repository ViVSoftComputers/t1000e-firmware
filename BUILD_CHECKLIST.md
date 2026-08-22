# T1000-E Firmware Build Checklist (for Roo Code / agentic use)

This document is a **deterministic, step-by-step procedure** for building the
T1000-E LoRaWAN tracker firmware and producing a flashable UF2. It is written so
an agent (Roo Code with a local model) can execute it **without reasoning through
any of the traps** — every step is a single command with a single expected output.

> **Golden rule:** never reconstruct the build from memory. Run
> `build_uf2.py` — it already encodes the correct family ID, SoftDevice
> concatenation, MBR handling, renumbering, and verification. Your only job is
> to run commands and check the one success line.

---

## The build environment (do not guess these paths)

| Thing | Value |
|-------|-------|
| Build directory | `C:\nRF5_SDK_17.1.0_ddde560\examples\ble_peripheral\t1000-e\pca10056\s140\11_ses_lorawan_tracker` |
| Build script | `build_uf2.py` (in the build directory) |
| GitHub repo | `C:\herman\t1000e-firmware-v22` |
| Python | `python3` (Windows, must run from git-bash or with Windows paths) |

---

## Procedure

### Step 1 — Check out the right branch

```bash
cd /c/herman/t1000e-firmware-v22
git fetch origin
git checkout v32
git pull origin v32
```

**Expected:** you are now on branch `v32` with no errors.

### Step 2 — Sync source to the SDK build tree (ONLY if source changed)

The build script compiles from the SDK tree, NOT the GitHub repo. If a source
file changed in the repo, copy it over first:

```bash
cd /c/herman/t1000e-firmware-v22
cp t1000_e/tracker/inc/app_at.h              /c/nRF5_SDK_17.1.0_ddde560/examples/ble_peripheral/t1000-e/t1000_e/tracker/inc/
cp t1000_e/tracker/src/app_at.c              /c/nRF5_SDK_17.1.0_ddde560/examples/ble_peripheral/t1000-e/t1000_e/tracker/src/
cp t1000_e/tracker/src/app_at_command.c      /c/nRF5_SDK_17.1.0_ddde560/examples/ble_peripheral/t1000-e/t1000_e/tracker/src/
```

> Skip this step if only the version number changed and you are rebuilding an
> already-synced tree. If unsure, copy the files — it is harmless.

### Step 3 — Build + produce the UF2

```bash
cd /c/nRF5_SDK_17.1.0_ddde560/examples/ble_peripheral/t1000-e/pca10056/s140/11_ses_lorawan_tracker
python3 build_uf2.py v32
```

**Expected — you MUST see ALL of these lines at the end:**

```
PASS: 2423 blocks, family 0x28860057 on all blocks
PASS: 1813 app blocks at 0x27000+
PASS: min_addr=0x0, 11 MBR blocks below 0x1000
=== BUILD SUCCESS ===
```

If you do NOT see `=== BUILD SUCCESS ===`, the build FAILED. Do not proceed.
Report the error lines verbatim.

### Step 4 — Verify the UF2 exists and is the right size

```bash
ls -la /c/nRF5_SDK_17.1.0_ddde560/examples/ble_peripheral/t1000-e/pca10056/s140/11_ses_lorawan_tracker/Output/Debug/Exe/t1000-e-v32.uf2
```

**Expected:** file size `1240576` bytes (≈1.24 MB). If it is ~61 KB, the
SoftDevice was NOT included — that is wrong, do not flash it.

### Step 5 — Commit + push the UF2 to GitHub

```bash
cd /c/herman/t1000e-firmware-v22
git add -f t1000-e-v32.uf2
git commit -m "v32: rebuild UF2"
git push origin v32
```

**Expected:** push completes with no error.

---

## Failure triage (what to check if a step does not match)

1. **Build said "Compilation: N OK, M failed" with M > 0** → a source file has
   a compile error. Read the error lines; fix the source, then re-run Step 3.
2. **UF2 is ~61 KB** → SoftDevice missing; the concat step did not run. Re-run
   Step 3 from scratch (it cleans the object cache automatically).
3. **Family is not `0x28860057`** → do NOT rely on `build_factory.py` alone; it
   emits `0xADA52840`. Only `build_uf2.py` produces the correct family.
4. **Push hangs** → git credential prompt. Use the inline-URL push:
   `git push https://<user>:<token>@github.com/ViVSoftComputers/t1000e-firmware.git v32`
   (token is in `~/.git-credentials`).

---

## What NOT to do

- Do **not** run `build_factory.py` and treat its `.uf2` output as flashable
  (wrong family `0xADA52840`).
- Do **not** invent flashing tools (`uf2tool`, `nrfutil`, `pyserial`). The
  T1000-E is flashed by dragging the UF2 onto its bootloader USB drive.
- Do **not** strip MBR blocks from the UF2 (the shipped firmware keeps them).
- Do **not** report success unless you saw `=== BUILD SUCCESS ===`.
