# 540p60 on HD CRTs: feasibility checkpoint

Research date: 2026-11-13. **UNKNOWN:** working PS3 540p output.
Follow-up: [dev6 signal audit](HD540_SIGNAL_AUDIT.md) identifies unchanged
stock HDMI mode description and unresolved raster/receiver semantics after
the black-screen tests.
**PROVEN (subsequent source/build work, 2026-11-14):** an opt-in HDMI-only
timed prototype now builds; see [dev1 implementation and validation](HD540_DEV1.md).
Its initial HD-menu startup regression is corrected in [dev2](HD540_DEV2.md).
The initial investigation below involved no console contact or reconfiguration.

## Evidence and proposed direction

**PROVEN (upstream source):** OSSC Pro has a `1920x540_60` CRT preset, described
in its source as "540p CRT modes, based on 1080i". Its active area is
1920 x 540, totals are 2200 x 562, horizontal back porch/sync are 148/44
samples, vertical back porch/sync are 15/5 lines, and its interlace flag is
clear. This is a useful external timing reference, not PS3 register settings.
Source: OSSC Pro commit `49a190bbfdad24bdd7f6ed3b28bf898f7b0d38fc`,
[video_modes_list.c, lines 107-109](https://github.com/marqs85/ossc_pro/blob/49a190bbfdad24bdd7f6ed3b28bf898f7b0d38fc/software/sys_controller/src/video_modes_list.c#L107).

**CLAIMED (project documentation):** OSSC Pro's CRT output list describes
540p for 1080i-capable HD CRTs. It distinguishes CRT/DAC output from ordinary
HDMI display output. This does not certify any particular television.
Source: [OSSC Pro manual, CRT output mode](https://junkerhq.net/xrgb/index.php?title=OSSC_Pro#CRT_output_mode).

**CLAIMED (manufacturer changelog):** RetroTINK-5X firmware 1.67 added 540p
for HD CRTs. Version 2.71 subsequently removed 240p and 540p to save space.
This is historical precedent, not a claim that the current 5X firmware offers
540p. Source: [official firmware history](https://retrotink-llc.github.io/firmware/5x.html).

**INFERRED:** the strongest PS3 investigation path starts with its existing
1080i output, then studies repeated field phase and source fetching to obtain
540 visible lines per progressive refresh. The external precedent and PS3
records below support investigating this; they do not prove the PS3 encoder
will generate the required sync.

## Timing arithmetic: nominal 60 is not always exactly 60

**PROVEN (arithmetic):** for a progressive raster,
`refresh = pixel_clock / (horizontal_total * vertical_total)`.
For interlaced 1080i, the field rate is twice the full-frame rate. At
74.250 MHz and 2200 samples per line, the line rate is 33.750 kHz.
The following calculations are reproducible with
[`tools/hd540_timings.py`](../tools/hd540_timings.py):

| Clock (MHz) | Total lines | Scan | Refresh/field rate (Hz) |
|---:|---:|---|---:|
| 74.250 | 1125 | Interlaced reference | 60.000000 |
| 74.250 | 562 | Progressive candidate | 60.053381 |
| 74.250 | 563 | Progressive candidate | 59.946714 |
| 74.175 | 1125 | Interlaced reference | 59.939394 |
| 74.175 | 562 | Progressive candidate | 59.992721 |
| 74.175 | 563 | Progressive candidate | 59.886162 |

**PROVEN (arithmetic):** 562 lines at exactly 60 Hz with a 2200-sample line
requires 74.184 MHz; at `60000/1001` Hz it requires approximately
74.109890 MHz. The 74.175 MHz rows use a rounded firmware table value,
not the exact `74.25 / 1.001` broadcast clock.
**UNKNOWN:** whether the PS3 clock programming path can produce each desired
clock reliably. A nominal mode name, table readback or TV information screen
cannot substitute for measuring the actual line rate, refresh and field phase.

Run locally, without console access:

```powershell
python -B tools/hd540_timings.py
python -B tools/hd540_timings.py --json
```

## What the PS3 firmware actually contains

**PROVEN (private 4.93 binary inspection):** `sysmgr_ss` has these internal
timing profiles. These are internal record indices, distinct from frontend
VIDEO IDs, backend AV IDs and public SDK resolution IDs.

| Internal profile | Nominal rate | Source | Raster totals | Field-fetch metadata |
|---:|---:|---|---|---:|
| 7 | 60 | 1920 x 1080 | 2200 x 1125 | 1 |
| 8 | 59.94 | 1920 x 1080 | 2200 x 1125 | 1 |
| 9 | 50 | 1920 x 1080 | 2640 x 1125 | 1 |
| 10 | 59.94 | 1280 x 720 | 1650 x 750 | 0 |
| 12 | 59.94 | 1920 x 1080 | 2200 x 1125 | 0 |

Source: private `sysmgr_ss.elf`, metadata at service VA
`0x80039148 + index * 24`, raster records at
`0x80038b28 + index * 56`. Nominal rate and source dimensions are metadata;
totals are decoded from the raster records. The binary stays in ignored
`local_only/`; no Sony code or record dump is published here.

**PROVEN (binary inspection):** profiles 7 and 8 have different clock
selectors: selector 4 contains float 74.25, selector 2 approximately
74.175003. **INFERRED:** these floats express MHz, consistent with the raster
totals and nominal field rates. Sources: selector table `0x800394d8`;
clock banks `0x800393e8` and `0x80039460`, 12 bytes per selector;
mode consumer `0x800005e4`.

**PROVEN (binary dispatch inspection):** frontend VIDEO ID `0x07` selects
internal profile 8; `0x08` selects profile 9; the additional `0x12` selects
profile 7. Sources: validator `0x80025c9c`, dispatch table `0x80025d3c`,
case locations `0x80025e08`, `0x80025e10`, `0x80025ea8` respectively.
The public Linux header names frontend `0x07` as 1080i60 and `0x08` as
1080i50; its backend IDs are `0x03` and `0x08` respectively.
Source: [Sony-authored Linux PS3 AV definitions, v6.12](https://github.com/torvalds/linux/blob/v6.12/arch/powerpc/include/asm/ps3av.h#L116).

**PROVEN (binary inspection):** backend 1080i60 ID 3 selects encoder
template 4; backend 1080i50 ID 8 selects template 5. The analog builder
uses fixed register templates rather than a public arbitrary-modeline
interface. Sources: selector `0x8000f4e0`, format-code table `0xc00003fc`,
template base `0xc0000000`, builder `0x8000f9ac`.
See also [the existing AV trace](LV1_AV_TRACE.md).
**UNKNOWN:** which encoder settings must change, if any, for repeated
field phase at the intended 540p timing.

**PROVEN (binary comparison):** raster control word 12 is already zero for
the inspected 1080i, 720p and 1080p records. Their word 13 is `0x11003111`.
The successful SD patch changes a different control-word pattern.
**INFERRED:** copying the SD control changes into HD records is not a valid
540p implementation. Sources: raster records above; raster writer
`0x80003bcc`; [current SD patcher](../app/source/lv1_patch.c).

## Implementation work still required

**PROVEN (v1.4.3/default build):** the timing patcher only recognizes the
SD records; native helper mode mapping/configuration only accepts SD modes.
The gamma readback guard currently accepts only the tested SD source sizes.
Sources: [lv1_patch.c](../app/source/lv1_patch.c),
[vsh_video.c](../plugin/vsh_video.c), [vsh_gamma.c](../plugin/vsh_gamma.c).
**UNKNOWN:** combined native gamma plus HD output. Existing SD gamma success
does not establish this.

**INFERRED (development plan):** a separate experimental HD path should:

1. Identify the exact HD CRT model and input; establish a working stock
   1080i baseline over component before any custom timing.
2. Trace the packed field-window parameters, field-fetch control and encoder
   sync selection together. Compare 1080i and progressive HD paths rather
   than changing only the source height.
3. Model 562/563-line output and clock choices offline. Keeping a 1080-line
   source with field fetching and an even 1124-line logical total is one
   hypothesis; it is not a verified register recipe. A true 540-line source
   would require a separately consistent scanout/rendering path.
4. Add separate HD identity checks, snapshot/readback, transactional rollback
   and a timed restore that continues if the menu stops drawing. Prevent the
   SD enforcer from conflicting with the HD experiment.
5. Test sync, full picture geometry, alternating-line patterns and vertical
   motion; measure whether vertical sync repeats the same phase rather than
   alternating interlaced fields. A stable picture alone is insufficient.
6. After that, test XMB, PS-menu return, apps/games that change modes, and
   recovery. Background enforcement comes after the timed test is verified.

## Compatibility and expected benefit

**PROVEN (Sony documentation):** the PS3 supports ordinary 1080i over
component/D-terminal and HDMI. 540p is absent from the documented output
choices. Source: [PS3 Video Output Settings](https://manuals.playstation.net/document/en/ps3/current/settings/videooutput.html).
**INFERRED:** component is the clearest first test target for this analog
timing project. HDMI needs a separate investigation of signaling, receiver
acceptance and mode metadata.

**UNKNOWN:** compatibility with a particular Sony, Toshiba or other HD CRT;
whether it passes this signal directly or processes it; and the resulting
latency. A TV's internal selectable "540p" processing mode does not by itself
prove acceptance of external 540p. No universal service-menu changes or
zero-lag claims follow from the evidence above.

**INFERRED:** the intended benefit is a non-interlaced 540-line picture on
compatible HD CRTs. The 1920-sample external reference also shows that 540p
does not inherently mean a 960 x 540 framebuffer. Changing output timing
alone does not force every game to render at 960 x 540, raise game frame
rates, or guarantee the TV bypasses its processing.

**PROVEN (scope of the initial investigation):** no live prototype was installed,
and v1.4.3 remains unchanged. **INFERRED (test prerequisite):** the owner's
current SD CRT/SCART setup is not a suitable target for approximately
33.7 kHz HD timing; an identified HD-capable display and input are needed
before attempting output validation.
