# Backward-compatible PS2 and PS1 emulator video mode analysis

**Verdict: PROVEN (high confidence):** Backward-compatible PS2 emulators (`ps2_emu`, `ps2_gxemu`) and PS1 emulators (`ps1_emu`, `ps1_netemu`, `ps1_newemu`) never output or request a native 240p/288p low-line raster mode. In hardware-BC PS2 (`ps2_emu` on CECHA/B), the emulator does explicitly recognize non-interlaced progressive 240p/288p input from the Graphics Synthesizer (GS) via MMIO status registers, assigning internal format enums 0x16 (`NTSC-240P`) and 0x17 (`PAL-288P`). However, its format translator immediately folds 240p into standard NTSC 480i and 288p into standard PAL 576i through an identical jump table target; the output dispatcher requests only standard 480i/p, 576i/p, 720p, and 1080i/p video IDs. The PS1 emulators run in user space (SELF type 4) and use the standard `cellVideoOut` API, which offers no low-line modes. Content is line-doubled into standard 480i/576i+ display buffers. The owner's hypothesis that stock BC fats or PS1 emulators retain an existing low-line video path that could be repurposed as a universal 240p switcher is therefore refuted.

## Scope and methodology

- **PROVEN:** Directly verified in decrypted ELF binaries from firmware 3.55 and 4.93 PUPs.
- **INFERRED:** Logical deduction from verified control and data flow.
- **CLAIMED:** Attributed to public specifications or community documentation.
- **UNKNOWN:** Unresolved or unverified.
- All addresses are virtual addresses within the respective ELF binaries.
- Public documentation contains addresses and high-level descriptions only; no proprietary code listings, decompilations, or byte dumps.

## Binary inventory and verification

All twelve emulator binaries across firmware 3.55 and 4.93 were extracted and decrypted to ELF format under `local_only/ps1ps2/`:

| Binary name | Version | SELF type | Decrypted ELF size | Verified SHA-256 |
|---|---|---|---:|---|
| `ps2_emu.elf` | 3.55 | Type 3 (LV2) | 5,267,080 | `dacd4985acd478221be687c5fe213fc5c3343c66c3747773f67ab099710f5eae` |
| `ps2_gxemu.elf` | 3.55 | Type 3 (LV2) | 6,866,832 | `aaf007417c27b0a416bcea315da62721586cf1084544449c1394b39bf8478242` |
| `ps2_softemu.elf`| 3.55 | Type 3 (LV2) | 6,146,424 | `e92f70fcdd1e94a45a5612224579ae94f24fb0644ba78336a5559a6f23416487` |
| `ps2_emu.elf` | 4.93 | Type 3 (LV2) | 5,274,984 | `f6811f8772a9fcae2ec16db15f43ad0ae074e8340baa9070f1377adb2b6a07b6` |
| `ps2_gxemu.elf` | 4.93 | Type 3 (LV2) | 6,874,848 | `fcc613b671c1cb8afd658de99911e35951ed4c3933e939d7882e8f6ea3fae881` |
| `ps2_netemu.elf` | 4.93 | Type 3 (LV2) | 10,442,536 | `c50fad4fdf329adc2c5dcb0682578806d725e4aa26f76fbb2b61e4168e9df72d` |
| `ps1_emu.elf` | 3.55 | Type 4 (APP) | 2,824,704 | `3f815309b877b9b655daa6b7d0a12bd7158916e0b30690ffcd1239dff84369c1` |
| `ps1_netemu.elf` | 3.55 | Type 4 (APP) | 2,971,688 | `c14cc1dbbd213ea5e5bde4a921c8e167ffe225808e73483aaeb8d224ff3de53b` |
| `ps1_newemu.elf` | 3.55 | Type 4 (APP) | 2,708,776 | `55088028f7c042a7b70d752ac7de46b402ddd5f3189ef024f995814e5d7efd18` |
| `ps1_emu.elf` | 4.93 | Type 4 (APP) | 2,765,616 | `d63bc176513248cacd5b1f91c45b1c18a3694defbc5deb99c974a6397a70a817` |
| `ps1_netemu.elf` | 4.93 | Type 4 (APP) | 2,913,992 | `32c7e1a53b4cf357b669fbed78dd28f10ecc529f3cfc0d89992476d0b5cb1d0d` |
| `ps1_newemu.elf` | 4.93 | Type 4 (APP) | 2,649,288 | `9b57e93bf25eadc1a195a12938e8490bfb760b4c948bae0108ee71a137cc80ee` |

*(Note: The RPCS3 dev_flash installation is verified byte-for-byte identical to official 4.93 firmware).*

## Hardware BC analysis (`ps2_emu`)

### 1. Low-line input detection

On CECHA/CECHB hardware, the Emotion Engine and Graphics Synthesizer (EE+GS) physically exist on the motherboard and feed their display output into the PS3's VideoIn capture interface.

- **Detection function (`0x781c4` in 3.55):** Reads the GS video status register at MMIO address `0x50100290`.
  - Bit 31 (`0x80000000` mask): Identifies progressive/non-interlaced GS output cadence (`SMODE2` register bit `INT = 0`).
  - Bits 27..28 (`0xc0000000` mask): Identifies interlaced output cadence (`INT = 1`).
  - Bits 16..27 line timing mask (`0x0fff0000`): Differentiates NTSC line timings (`0x1060000`..`0x1080000`) from PAL line timings (`0x1390000`..`0x13a0000`), 480p (`0x20c0000`..`0x20d0000`), and 1080i (`0x2330000`..`0x2340000`).
- **Format assignment:**
  - Non-interlaced NTSC is assigned input format enum `0x16` (`NTSC-240P`).
  - Non-interlaced PAL is assigned input format enum `0x17` (`PAL-288P`).
  - Interlaced NTSC is assigned input format enum `0x00` (`NTSC-480I`).
  - Interlaced PAL is assigned input format enum `0x01` (`PAL-576I`).
- **Notification function (`0x775ec` in 3.55):** Dispatches the input format enum to an inline jump table at `0x777bc`. It logs format changes via debug strings:
  - VA `0x185260`: `%d : Change VideoIn Format to NTSC-240P\n` (for enum `0x16`)
  - VA `0x185238`: `%d : Change VideoIn Format to PAL-288P\n` (for enum `0x17`)
  - VA `0x185318`: `%d : Change VideoIn Format to NTSC-480I\n` (for enum `0x00`)
  - VA `0x1852f0`: `%d : Change VideoIn Format to PAL-576I\n` (for enum `0x01`)

### 2. Output translation and folding

After logging the input format, function `0x775ec` (at call site `0x77898`) passes the format enum (register `r3`) and active system display configuration (register `r4`) to the translator function at `0x49b20`:

- **Jump table at `0x49c2c` (24 entries, indexed by input format):**
  - Entry `0x16` (240p): Relative branch target `0x49d18`.
  - Entry `0x00` (480i): Relative branch target `0x49d18` (**identical target**).
  - Entry `0x17` (288p): Relative branch target `0x49d40`.
  - Entry `0x01` (576i): Relative branch target `0x49d40` (**identical target**).
- **Translation result:** Format enum `0x16` (240p) is unconditionally folded into output mode index 1 (standard NTSC SD), and format enum `0x17` (288p) is folded into output mode index 3 (standard PAL SD). If the user configuration enables progressive/HD upscaling, both are mapped to mode indices 2 (480p), 5 (720p), or 6 (1080i).

### 3. Output dispatcher and AV packet generation

The output mode index is processed by the AV request dispatcher at `0x491c0`:

- **Requested frontend video IDs:** `0x01` (480i standard), `0x03` (576i standard), `0x05` (480p), `0x06` (576p), `0x07` (1080i50), `0x08` (1080i60), `0x09` (720p60), `0x0a` (720p50), `0x0b` (1080p60), `0x0c` (1080p50), `0x10` (`480I_A`), and `0x12` (`1080I_60_A`).
- **Packet producers:** Calls at `0x47344`, `0x47ba4`, and `0x47ffc` build standard 48-byte `VIDEO_MODE` packets and transmit them over hypervisor VUART 0 to the AV manager (`sysmgr_ss`).
- **PROVEN:** Every single requested mode ID maps directly to standard 525-line, 625-line, 750-line, or 1125-line timing profiles. No low-line ID (such as 262-line or 312-line totals) is ever emitted.

### 4. Other PS2 emulators (`ps2_gxemu`, `ps2_softemu`, `ps2_netemu`)

- `ps2_gxemu` (GS-only hardware BC on CECHC/CECHE), `ps2_softemu`, and `ps2_netemu` lack the physical VideoIn capture block and do not contain the `VideoIn` strings or the `0x781c4` register monitor.
- All three share the 10-entry output dispatcher described in [LV1_AV_TRACE.md](LV1_AV_TRACE.md) (dispatch indices 0..9, selecting frontend IDs `0x05`, `0x06`, `0x07`, `0x08`, `0x09`, `0x0a`, `0x0b`, `0x0c`, `0x10`, `0x11`).
- None of the PS2 emulators contain a low-line output path.

## PS1 emulator analysis (`ps1_emu`, `ps1_netemu`, `ps1_newemu`)

### 1. Architecture and interface boundary

- All PS1 emulators are compiled as standard PS3 user-space applications (SELF type 4, `KEY_APP`).
- Unlike the PS2 emulators, they do not issue hypervisor calls (`sc 1`) and do not access hypervisor VUART directly.
- All display mode requests are mediated strictly through the public `cellVideoOut` SDK library.

### 2. Video configuration calls

In `ps1_emu` (function `0x81188` in 3.55):
- The emulator queries display capabilities via `cellVideoOutGetDeviceInfo(0, 0, &deviceInfo)` (stub `0x15744c`).
- It queries available resolutions via `cellVideoOutGetResolution(resId, &res)` (stub `0x1574ac`).
- It configures the display output via `cellVideoOutConfigure(0, &config, NULL, 0)` (stub `0x15736c`).
- **Supported resolution IDs:** The `cellVideoOut` API only defines IDs 1 (`480`), 2 (`576`), 3 (`720`), 4 (`1080`), 5 (`1600x1080`), 6 (`1440x1080`), 7 (`1280x1080`), and 8 (`960x1080`). There are no 240p or 288p API constants.

### 3. Content conversion: line doubling vs. field output

Function `0x350f8` (in 3.55) inspects the current VSH video settings returned by `cellVideoOutGetDeviceInfo`:
- Scan mode field `local_17`:
  - `0`: Interlaced display (logs `VSH : Interlace` at VA `0x15b5e0`).
  - `1`: Progressive display (logs `VSH : Progressive` at VA `0x15b5f0`).
- Aspect ratio field `local_15`:
  - `0`: 4:3 display (logs `VSH : ASPECT_4_3`).
  - `1`: 16:9 display (logs `VSH : ASPECT_16_9`).
- **Line doubling in standard containers:**
  - When outputting to a standard 480i analog raster, the PS1 emulator renders into a 720×480 framebuffer.
  - Each native 240-line scanline is line-doubled (rendered to two adjacent vertical lines in the framebuffer).
  - When scanned out in 480i cadence, field 0 scans lines 0, 2, 4... and field 1 scans lines 1, 3, 5... Because line 0 and line 1 contain identical data from the same 240p scanline, both fields present the identical content on alternating scanlines.
  - The RSX raster timing remains full 525-line NTSC interlaced timing with standard half-line field alternation.
- **User-settable options:**
  - Function `0x35328` handles in-game settings:
    - `g_nUpconvertMode`: Controls scaling/aspect ratio (`0` = Normal 4:3, `1` = Full Screen, `2` = Zoom).
    - `g_bImageSmoothing`: Controls bilinear texture filtering (`0` = Off/nearest-neighbor, `1` = On/smoothed).
  - Neither setting changes output raster timing or bypasses the 480i/576i floor.

### 4. Comparison among PS1 emulators and across firmware versions

- **`ps1_emu` vs. `ps1_netemu` vs. `ps1_newemu`:**
  - `ps1_emu`: Disc-based PS1 emulator. Uses `cellVideoOutConfigure` and `cellVideoOutGetDeviceInfo`.
  - `ps1_netemu`: PSN / digital package PS1 emulator. Uses `cellVideoOutConfigure`, `cellVideoOutGetDeviceInfo`, `cellVideoOutGetResolution`, and `cellVideoOutGetState`.
  - `ps1_newemu`: Newer lightweight variant. Only imports `cellVideoOutConfigure`.
  - All three share identical internal video setup structures, identical `VSH : Interlace` detection, and identical `g_nUpconvertMode` handlers.
- **3.55 vs. 4.93:**
  - The video setup, mode configuration, and line-doubling routines are functionally identical between 3.55 and 4.93. No low-line escape or alternative raster path was introduced in 4.93.

## Summary table of emulator video paths

| Emulator | Firmware | Execution level | Video transport | Input low-line recognition | Output raster modes | Low-line output? |
|---|---|---|---|---|---|---|
| `ps2_emu` (hardware BC) | 3.55 & 4.93 | LV2 (Type 3) | Direct HV VUART | **PROVEN:** Detects GS non-interlace (0x16 240p, 0x17 288p) | Standard 480i/p, 576i/p, 720p, 1080i/p | **NO** (folds into 480i/576i) |
| `ps2_gxemu` (GS BC) | 3.55 & 4.93 | LV2 (Type 3) | Direct HV VUART | None (software GS capture) | Standard 480i/p, 576i/p, 720p, 1080i/p | **NO** |
| `ps2_softemu` / `netemu` | 3.55 & 4.93 | LV2 (Type 3) | Direct HV VUART | None | Standard 480i/p, 576i/p, 720p, 1080i/p | **NO** |
| `ps1_emu` | 3.55 & 4.93 | User (Type 4) | `cellVideoOut` SDK | None (renders to 480/720/1080 buffer) | Standard SDK resolutions 1..8 | **NO** (line-doubled) |
| `ps1_netemu` | 3.55 & 4.93 | User (Type 4) | `cellVideoOut` SDK | None | Standard SDK resolutions 1..8 | **NO** (line-doubled) |
| `ps1_newemu` | 3.55 & 4.93 | User (Type 4) | `cellVideoOut` SDK | None | Standard SDK resolutions 1..8 | **NO** (line-doubled) |

## Architectural conclusions and ranked leads

1. **The BC-fat idea is closed as a software lead:**
   - Finding "240p" strings in `ps2_emu` was an artifact of GS hardware video input capture, not video output generation.
   - Because `ps2_emu` maps 240p and 480i to the exact same output branch at `0x49c2c`, no latent low-line modeline exists in the emulator.
   - Emulators cannot generate custom low-line video without hypervisor AV-manager modifications.

2. **The hypervisor AV manager remains the single gatekeeper:**
   - As proven in [LV1_AV_TRACE.md](LV1_AV_TRACE.md), all video output modes (whether requested via guest VUART or `cellVideoOut`) terminate in `sysmgr_ss.elf` in LV1.
   - The AV manager converts mode IDs into hardcoded 56-byte raster records and 68-byte encoder templates.
   - Any viable software route to native 240p/288p requires modifying or overriding the AV manager's timing tables, not emulator-level patching.

3. **Next technical priority:**
   - Proceed with the read-only verification of hypervisor embedded-image anchors as designed in [EXPERIMENT_lv1_anchor_peek.md](EXPERIMENT_lv1_anchor_peek.md).
