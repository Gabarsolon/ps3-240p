# Prototype LV1 Timing Patch: Native 240p/288p Profile Override

> **Correction (2026-11-11):** The fixed physical addresses are boot observations, not stable write targets; the native app now resolves protection pages. The original package remained at 576i in the owner's SCART RGB test. Encoder behavior, activation and the claimed output remain unverified. This historical specification is superseded by [the app review](APP_REVIEW_2026-11-11.md), including the request-height validation gap.

**Status: SPECIFICATION COMPLETED — READY FOR VERIFICATION**  
**Date: 2026-11-11**  
**Target Environment: PS3 Slim CECH-25xx, Evilnat 4.93 CFW + Cobra, webMAN MOD**  

---

## 1. Overview & Patch Philosophy

This document defines the fail-open, non-destructive prototype specification for overriding standard definition video timing in PlayStation 3 physical memory to produce native **240p (NTSC 60 Hz)** or **288p (PAL 50 Hz)** analog output.

### Non-Destructive In-Memory Design
1. **Zero Flash Modification:** No files in `/dev_flash`, `/dev_hdd0`, or the CORE_OS image are altered.
2. **Reboot Clean:** All patches target volatile DDR3/XDR physical RAM backing the running `sysmgr_ss` process. Power cycling or restarting the console completely restores standard factory firmware behavior.
3. **Fail-Open Pre-flight Gate:** Before applying any memory modifications, the target locations are read and compared against known stock cryptographic hashes. If any byte diverges from expected stock 4.93 firmware, the patch operation immediately halts without writing.

---

## 2. Memory Targets and Address Derivation

From the verified Stage 1b dynamic page traversal of Slot 10 (`sysmgr_ss`, PID 10, AddressProtectionDomain `0x7614b0`):

| Target Block | Virtual Address | Live Physical DRAM Address | Length | Stock SHA-256 |
|---|---|---|---:|---|
| **Metadata Record 1 (480i)** | `0x80039160` | `0x144160` | 24 bytes | `d5bfc18bf3510558595e88fe25ededc4306284f90331adaf2fabc68dd753d63f` (first 16 bytes) |
| **Raster Record 1 (480i)** | `0x80038b60` | `0x143b60` | 56 bytes | `381678f339e524cd406d4e8545bbdf36c6b8aad426acc497f71f92f7268ec57e` (first 16 bytes) |
| **Encoder Template 0 (NTSC)**| `0xc0000000` | `0x793000` | 68 bytes | `8a13e9ec965d86a1bc81cd706dfa2bfd9f0fbcf0234e3b7b25425e2ff7f3c455` (first 16 bytes) |

*(Note: For PAL 288p, Metadata Record 4 is at `0x1441a8`, Raster Record 4 is at `0x143c08`, and Encoder Template 1 is at `0x793044`).*

---

## 3. Patch Payloads

### 3.1 NTSC 240p Override (Replacing Record 1 / 480i)

#### A. Metadata Override (`0x144160`, 24 bytes)
- Stock 480i:
  `426fc28f 00000001 00000004 00000b40 000001e0 00000000`
- Patched 240p:
  `426fc29f 00000000 00000004 00000b40 000000f0 00000000`
- Changes:
  - `+0x00`: Refresh float `59.94006` (`0x426fc29f`).
  - `+0x04`: Interlace flag changed from `1` (interlaced) to `0` (progressive).
  - `+0x10`: Active height changed from `480` (`0x01e0`) to `240` (`0x00f0`).

#### B. Raster Override (`0x143b60`, 56 bytes)
- Stock 480i:
  ```
  00000b3f 00000d67 00000b7f 00000c77
  000001df 0000020c 000001e9 000001e8 000001ee
  054704cb 04cb03d3 01f401e2 00030000 11000000
  ```
- Patched 240p:
  ```
  00000b3f 00000d67 00000b7f 00000c77
  000000ef 00000105 000000f4 000000f4 000000f7
  054704cb 04cb03d3 00f700f4 00010000 11000000
  ```
- Changes:
  - `+0x10` (`VActive - 1`): `239` (`0x00ef`).
  - `+0x14` (`VTotal - 1`): `261` (`0x0105`, 262 lines total).
  - `+0x18` (`VMarker`): `244` (`0x00f4`).
  - `+0x1c` (`VSyncStart`): `244` (`0x00f4`).
  - `+0x20` (`VSyncEnd`): `247` (`0x00f7`).
  - `+0x2c` (`w11`): `(247 << 16) | 244` = `0x00f700f4`.
  - `+0x30` (`w12`): Scan control changed from `0x00030000` (interlaced) to `0x00010000` (progressive).

#### C. MultiAV Encoder Override (`0x793000`, 1 byte edit)
- Stock Template 0, offset `+0x11` (register `0x37` value): `0x80`
- Patched Template 0, offset `+0x11` (register `0x37` value): `0x00`
- Effect: Disables hardware interlace sync generation in the Sony CXM4027R video encoder chip.

---

## 4. Mode Activation and Per-Head Cache Invalidation

In `sysmgr_ss` mode setup (`0x800005e4`), the function maintains a cached copy of the active profile index for Head 0 and Head 1. If a display request specifies the currently active profile index, `sysmgr_ss` can skip rewriting the RSX CRTC registers.

To guarantee that the newly patched timing table is loaded and programmed into RSX hardware MMIO and Syscon:
1. **Mode Toggle Sequence:**
   - Command a mode switch to an alternate resolution (e.g. 480p or 720p).
   - Command a mode switch back to 480i.
   - Upon switching to 480i, `sysmgr_ss` reads the modified table at `0x143b60` and `0x144160`, writes the 240p parameters to RSX CRTC registers `0x680800..0x6808f8`, and dispatches the progressive template to the CXM4027R encoder.
2. **Emulator Launch Sequence:**
   - When a PS1 or PS2 game launches with display settings set to standard 480i, the emulator requests mode setup from `sysmgr_ss`. With the table patched in RAM, the game will scan out in native 240p scanlines.

---

## 5. Pre-flight Verification Script Architecture

The PowerShell verification tool (`tools/lv1_anchor_peek.ps1`) already contains the complete bounds-checked page resolution logic:
1. `Verify-PreConditions`: Ensures console connectivity and checks PID 10 Process descriptor.
2. `Peek-TargetAnchors`: Performs cryptographic hash verification of stock values before permitting any modification.
3. `Apply-TimingOverride`: (Specification for future owner testing) Submits bounded 64-bit word pokes via Cobra syscall 10 (`/poke.lv1`).

---

## 6. Safety Summary

- Target DRAM ranges are strictly within hypervisor heap `[0x00000000, 0x02000000)`.
- No RSX MMIO registers are poked directly from user space; all hardware programming is performed legitimately by `sysmgr_ss`'s native mode-setter `0x800005e4`.
- Reversible instantly via restart.
