# Display preset investigation: retail 4.93

Started 2026-11-14 with offline analysis. The subsequent
[native timed probe](DISPLAY_PRESET_PROBE.md) now establishes 1280 x 768 HDMI
on the owner's console, with visible capture, receiver-reported progressive
scan and stock restoration. Initial activation unknowns below are historical
for that preset; other outputs, games, HEN and other PC modes remain unknown.
See the [approved roadmap](DISPLAY_PRESETS_ROADMAP.md).

## Evidence scope

**PROVEN** means checked source/binary; **INFERRED** marks interpretation;
**CLAIMED** attributes an external report; **UNKNOWN** marks unverified behavior.
Firmware addresses below are virtual addresses in the private 4.93
`sysmgr_ss.elf`, SHA-256
`07c0bd2a2403f65b6d741e11a15106114365cb78165833bac726476f671b64c9`.
Addresses explicitly marked VSH refer instead to private retail `vsh.elf`,
SHA-256 `2368923a20c7ed7f4927dbcad8854e15095d0d1cabad70c6531b76e36c9cfb17`.
Sony-derived listings, full table dumps and decompilation remain under ignored
`local_only/`; this document publishes addresses and descriptions only.

## Existing PC modes are present in the retail AV service

**PROVEN (fresh binary reads):** validator dispatch at `0x80025d10`, through
the jump table at `0x80025d3c`, maps the three Linux PC frontend IDs to
existing retail profiles. Their metadata and clock records are present:

| Proposed preset | Frontend VID | Internal profile | Metadata address | Raster address | Nominal clock record |
|---|---:|---:|---|---|---:|
| 1280 x 768 | `0x0d` | 16 | `0x800392c8` | `0x80038ea8` | 79.5 |
| 1280 x 1024 | `0x0e` | 17 | `0x800392e0` | `0x80038ee0` | 108.0 |
| 1920 x 1200 | `0x0f` | 18 | `0x800392f8` | `0x80038f18` | 154.0 |

**PROVEN (stored values):** all three metadata entries contain nominal
refresh 60.0, scan field zero, horizontal multiplier one and the listed
active dimensions. Clock-index bytes at `0x800394d8` select entries 6/7/8;
both clock banks (`0x800393e8`, `0x80039460`) contain the listed floating-point
frequency values at those indices. **INFERRED:** frequency units are MHz
and these metadata scan values identify progressive timing; see the consumer
trace in [LV1 AV trace](LV1_AV_TRACE.md#timing-profile-structure-and-consumers).
No measured output frequency is claimed.

**PROVEN (public source):** Linux v6.12 `video_mode_table` names these same
three PC dimensions, and its header defines their frontend IDs. Sources:
[Sony/Linux driver](https://github.com/torvalds/linux/blob/v6.12/drivers/ps3/ps3av.c#L57-L79),
[AV header](https://github.com/torvalds/linux/blob/v6.12/arch/powerpc/include/asm/ps3av.h#L116-L131).
This source evidence does not establish retail GameOS compatibility.

**PROVEN (binary):** an additional 1360 x 768 metadata/raster profile exists
at index 19 (`0x80039310`, `0x80038f50`), reached by frontend VID `0x15`.
**UNKNOWN:** its usable service state and connector path. The precheck at
`0x80025ccc` rejects frontend IDs `0x12..0x15` in one service state; it should
not be treated as an immediately available extra preset.

## HDMI descriptions already distinguish these PC modes

**PROVEN (binary):** backend IDs 10/11/12 select format codes `0x201`,
`0x301`, `0x401` through `0xc00003fc`. HDMI selector `0x800105d0` maps these
to template indices 16/17/18 in the 12-byte template table at `0x8003997b`.
Each template's video-code byte (offset 2) is zero. The copy at
`0x8000f9ac` and AVI builder `0x8000fde0` connect that byte to the HDMI AVI
video-code field; this is the same path traced for stock HD in
[the HDMI audit](HD540_SIGNAL_AUDIT.md#hdmi-still-uses-a-stock-1080i-description).

**INFERRED:** existing PC profiles offer a more coherent starting point than
arbitrary new timings: native raster, clock and HDMI-description entries
already exist. **UNKNOWN:** actual transmitter/receiver acceptance on the
owner's hardware and correct retail requests to activate them together.

## GameOS selection and scaling remain separate questions

**PROVEN (project source):** our current VSH bridge deliberately limits its
ordinary mapper to SD resolution IDs 4/5, and its HD submission helper has
HDMI/1920-pixel-source guards. Sources: `plugin/vsh_video.c`,
`vsh_video_map`, `vsh_video_configure`, `vsh_video_hd_submit`.
It is not a generic PC-preset API today.

**PROVEN (private VSH trace):** the retail configuration path includes a
native mode mapper (`vsh.elf` `0x000c7080` / `0x000c56f4`), configuration
submission (`0x0038ac04`) and asynchronous display setup (`0x0037301c`,
`0x00372e08`, `0x00435524`). Its public configuration RPC also has
retail/state checks (`0x00126e18`, `0x000c56b8`, `0x000c5bec`).
**UNKNOWN:** which complete, reversible PC conversion request is accepted
and whether it preserves the source surface correctly. Trace the native
debug-monitor/conversion path before broadening any existing bridge guard.

### Focused follow-up: native PC monitor selection route

**PROVEN (private VSH binary, bounded Ghidra trace):** the debug-monitor
RPC handler at `0x00126a7c` invokes display-interface method offset `0x54`.
The checked interface table resolves that method to `0x00453ad4`; its
dispatch table at `0x00453b24` has explicit cases for monitor types 11, 12
and 13, preserving those values for dispatcher `0x0044d2c4`. The dispatcher
invokes backend method offset `0x40` on optional device objects at offsets
`0x24`/`0x28` and the object at offset `0x34`.

**PROVEN (local SDK declarations):** `cell/cell_video_out.h` names monitor
types 11/12/13 as WXGA/SXGA/WUXGA and separately defines display-conversion
values for these targets. A declaration alone does not establish working
retail behavior. **INFERRED:** the native selector is a concrete existing
route worth tracing before replacing any timing tables. **UNKNOWN:** backend
method implementations, retail/device restrictions, persistence, return
semantics and whether this selector actually changes output on the owner's
console. Do not call it a working PC-mode switch yet.

Private reproducibility evidence: `local_only/display_presets/trace.py`,
`summary.json`, `bounded-vsh.asm`, the bounded setter decompilation under
`local_only/activation/`, and `xsetting4712_methods.json`. Fresh ELF reads
confirmed the identities, PC metadata/clock selections, validator targets
and HDMI video-code fields. No proprietary listing is included here.

**PROVEN (private AV-service trace):** scaler/placement function
`0x80002ca4` consumes source dimensions, horizontal/vertical position and
profile metadata separately. Its branches include centered windows and
different conversion/scaling policies. The validator at `0x80025f9c` and
`0x80025fd0` bounds source dimensions against profile dimensions.
**INFERRED:** a preset might retain a game's existing rendering surface and
place it within a different output raster. **UNKNOWN:** arbitrary aspect
correction, useful scaling ranges, filtering quality, game transitions and
global enforcement. Existing 540p alternate-row fetching is not proof of
a general filtered downscaler.

## First candidate and next proof

**INFERRED recommendation:** investigate 1280 x 768 HDMI first. Its width
matches a normal 1280 x 720 source and an existing PC profile supplies the
remaining raster/clock/description. Preserving a centered 720-line picture
would leave 24 lines above and below (arithmetic, not observed output).
1280 x 1024 is 5:4; it must not be advertised as the requested 4:3 preset.
1024 x 768 remains a separate custom-timing lead, not one of these three
Linux presets.

Before a live test:

1. Follow backend method offset `0x40` from the native monitor dispatcher,
   then resolve the PC conversion route and its source-buffer/pitch contract;
   distinguish actual output mode from source resolution IDs.
2. Establish connector/state checks and native return/error semantics.
3. Prepare exact stock-state capture and bounded restoration using the
   resident helper; no saved boot default or arbitrary clock programming.
4. Only then test one preset, checking receiver signal/scan, visible geometry
   and restoration separately. XMB success will not prove game compatibility.

**PROVEN (subsequent owner-console probe):** the first native 1280 x 768
HDMI candidate now passes the timed test and restoration. See
[complete results and build scope](DISPLAY_PRESET_PROBE.md). This is an
isolated developer helper, not a new app-menu release or permanent mode.
