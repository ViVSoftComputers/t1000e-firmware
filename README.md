# T1000-E LoRaWAN Tracker Firmware

Custom firmware for the Seeed SenseCAP T1000-E LoRaWAN tracker, built from the
[Seeed-Studio reference source](https://github.com/Seeed-Studio/Seeed-Tracker-T1000-E-for-LoRaWAN-dev-board)
(forked from commit `f3ad9d4`, v1.2).

## Build Environment

| Component | Version | Notes |
|---|---|---|
| **Compiler** | ARM GCC 12.3.1 | Via PlatformIO (`toolchain-gccarmnoneeabi`) |
| **SDK** | nRF5 SDK 17.1.0 | At `C:\nRF5_SDK_17.1.0_ddde560\` |
| **SoftDevice** | s140 7.2.0 | From SDK (`components/softdevice/s140/hex/`) |
| **UF2 converter** | Microsoft uf2conv.py | At `C:\Users\ViV\uf2conv.py` |
| **Host** | Windows 10 | git-bash (MSYS) terminal |

**CRITICAL**: SEGGER Embedded Studio 8.28a **cannot** compile this firmware.
SES 8.28a ships GCC 15.2 which treats implicit function declarations as errors
and uses `thumb_crt0.s` (SEGGER-specific startup). The standard nRF5 SDK GCC
startup (`gcc_startup_nrf52840.S`) must be used instead.

**We wasted 2 days trying SES before discovering this.**

## Build

```bash
cd C:\nRF5_SDK_17.1.0_ddde560\examples\ble_peripheral\t1000-e\pca10056\s140\11_ses_lorawan_tracker
python build_factory.py
```

The build script:
1. Parses the SES `.emProject` file for include paths and source files
2. Compiles all 180 source files (including `gcc_startup_nrf52840.S`)
3. Links using the standard nRF5 SDK GCC linker script
4. Generates `.hex`, `.elf`, and `.uf2` output in `Output/Debug/Exe/`

### Prerequisites

- nRF5 SDK 17.1.0 at `C:\nRF5_SDK_17.1.0_ddde560\`
- PlatformIO ARM GCC at `C:\Users\ViV\.platformio\packages\toolchain-gccarmnoneeabi\`
- Python 3 with `intelhex`
- Microsoft uf2conv.py at `C:\Users\ViV\uf2conv.py`

## Source Modifications from Seeed v1.2

### Debug Markers (for firmware identification)

1. **Boot banner** (`main_lorawan_tracker.c`): Changed from `"T1000-E Tracker example"`
   to `"T1000-E HERMES 20260719"` — visible in UART debug output on boot.

2. **Uplink signature** (`main_lorawan_tracker.c` + `app_lora_packet.c`): Every LoRaWAN
   uplink payload now ends with 2 bytes: `0xBE 0xEF`. Visible in ChirpStack payload
   view as trailing `beef` hex. Power-on uplink goes from 13→15 bytes, sensor payloads
   from 8→10 bytes, GPS payloads from 16→18 bytes.

### Critical GCC Compatibility Fixes

These are NOT optional — the firmware will compile but **will not boot** without them:

1. **Timer callback signatures** (`app_beep.c`, `app_led.c`, `app_button.c`, `app_user_timer.c`):
   Changed all `void handler(void)` → `void handler(void *p_context)`. The nRF5 SDK
   timer expects `app_timer_timeout_handler_t` which is `void (*)(void *)`. Wrong
   signatures cause stack corruption and hard fault on first timer tick.

2. **GCC startup file**: Added `gcc_startup_nrf52840.S` from the nRF5 SDK. The SES
   project uses `thumb_crt0.s` which is SEGGER-specific. Without the GCC startup,
   the interrupt vector table is missing and the CPU executes garbage at boot.

3. **Standard GCC linker script**: Replaced the SES-generated linker script with
   `generic_gcc_nrf52.ld` from the nRF5 SDK. The SES linker script is incompatible
   with GCC's section naming and symbol expectations.

4. **Missing includes** (for GCC strictness, not crash-critical):
   - `smtc_hal_spi.c`: added `<string.h>`
   - `smtc_hal_gpio.c`: added `"smtc_hal_mcu.h"`
   - `smtc_hal_trace.c`: added `"smtc_hal_usb_cdc.h"`
   - `smtc_hal_uart.c`: added `"smtc_hal_mcu.h"` + `"ag3335.h"`
   - `smtc_hal_mcu.c`: added `"app_led.h"`
   - `apps_utilities.c`: added `"smtc_hal_trace.h"`
   - `main_lorawan_tracker.c`: added `"app_timer.h"` + forward declarations

5. **SES RTL retarget** (`app_board.c`): Added `#ifdef __SEGGER_CC`-guarded retarget
   stubs for SES builds only. GCC uses `-specs=nosys.specs` which provides its own.

6. **flash_placement.xml**: Removed `size="0x4"` caps on `.text` and `.rodata` sections
   that artificially limited them to 4 bytes.

## Flashing

The T1000-E bootloader uses UF2 family ID `0x28860057` (**not** the standard nRF52840
`0xADA52840`). The factory UF2 confirmed this.

1. Put device in DFU mode (hold button, plug USB)
2. Drag-drop the `.uf2` file onto the USB drive
3. Device auto-flashes and reboots

The build script produces an app-only UF2. For a combined UF2 (SoftDevice + App):
merge the SoftDevice hex with the app hex, then convert with `-f 0x28860057`.

## Verification

After flashing, check ChirpStack for the power-on uplink:
- DATA_ID `0x1E` should be **15 bytes** ending in `BE EF`
- Factory firmware is 13 bytes (no signature)

## Root Cause Analysis: Why GCC 12.3.1 Build Initially Failed

The SES project at `11_ses_lorawan_tracker` uses SEGGER's `thumb_crt0.s` startup
file. When compiled with GCC (even GCC 12.3.1), this file either compiles incorrectly
or the linker places it wrong because:

1. SES linker script expects `.vectors` section — GCC startup uses `.isr_vector`
2. SES startup expects SEGGER-specific symbols (`__SEGGER_RTL_*`) not present in GCC
3. Without a proper vector table at 0x27000, the CPU reads garbage as the initial
   stack pointer and reset vector → immediate hard fault → faint LED flash, nothing.

The fix: use `gcc_startup_nrf52840.S` + standard nRF5 SDK GCC linker script.
This is what the nRF5 SDK's `armgcc` examples use and is the intended GCC build path.

## License

Based on Seeed-Studio's reference implementation. See `README-Seeed.md` for original
license terms.
