# 540p HDMI signal audit after the dev6 tests

Checkpoint: 2026-11-14. No console requests, new payloads, display changes
or firmware writes were performed during this audit. The installed dev6
build and stable v1.4.3 artifacts are unchanged.

## What the tests establish

**PROVEN (logs), CLAIMED (owner's display observation):** the PS3 accepted
the four candidate RAM stores and reached matching AV cache readback, held
for about 15 seconds, then restored stock through 720p and 1080i. Both
the USB capture path and direct Xiaomi monitor failed to show the candidate;
the monitor reported no signal. Automatic recovery returned the picture.
See [dev6 live evidence](HD540_DEV6.md). Cache acceptance is not a measurement
of scanline count, sync phase, transmitter signaling or receiver lock.

## HDMI still uses a stock 1080i description

**PROVEN (private 4.93 binary trace):** the HDMI backend has a path separate
from the RSX raster records. For backend video ID 3, its format-code table
at `0xc00003fc` selects format code 5. Selector `0x800105d0` then selects
template index 6 from the 12-byte table at `0x8003997b`.
The template's video-code byte is 5. Builder `0x8000f9ac` copies that
template into its HDMI packet, configuration function `0x80010824` retains
that code for this HD mode, and AVI builder `0x8000fde0` places it in the
AVI video-code field and calculates the checksum. The checked vtable at
`0x8003d5e0` connects those selector/configuration/AVI operations.
Evidence: private `sysmgr_ss.elf` at those addresses, ignored
`local_only/hd540/static-check/hdmi-selector-summary.json`, and private
decompilation in `local_only/av_trace/` / `local_only/activation/`.
No Sony-derived listings or complete template dumps are published here.

**PROVEN (public source comparison):** the Linux DRM CTA mode table identifies
VIC 5 as 1920 x 1080 interlaced at nominal 60 Hz. Its AVI packer encodes
the selected video code in payload byte 4.
Sources: [DRM CTA modes](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/drm_edid.c),
[AVI packer](https://github.com/torvalds/linux/blob/v6.12/drivers/video/hdmi.c#L153).

**PROVEN (our source):** `hd540_patch.c` changes only two raster qwords per
1080i profile, not the HDMI templates, backend ID or AVI builder.
**INFERRED:** while requesting the candidate, the audited HDMI path still
describes a standard 1080i mode. That is an incomplete implementation of
custom 540p signaling. **UNKNOWN:** whether this causes the observed loss
of picture, whether the transmitter itself rejects the raster, and what
AVI bits actually appear on the wire. We have not captured those packets.

**PROVEN (independent source):** OSSC Pro's 1920 x 540 CRT preset uses
`HDMI_Unknown`, 540 active / 562 total lines and a cleared interlace flag;
its stock 1080i preset uses the corresponding standard HDMI mode. This
distinguishes the custom mode description from the stock interlaced one.
Source: [OSSC Pro timing presets, pinned revision](https://github.com/marqs85/ossc_pro/blob/49a190bbfdad24bdd7f6ed3b28bf898f7b0d38fc/software/sys_controller/src/video_modes_list.c#L107).
**UNKNOWN:** whether changing a PS3 AVI code alone would produce correct
output. It would not correct an invalid sync waveform or other transmitter
mode configuration.

## Raster fields still need an electrical interpretation

**PROVEN (private binary):** raster writer `0x80003bcc` sends the total-line
field to register offset `0x680804`; the other modified packed field goes
to `0x6808f8`. Related public Nouveau register definitions name the former
FP vertical total. They do not establish the meaning of the latter RSX
packed field. Source: private `sysmgr_ss.elf`, writer above;
[Nouveau register definitions](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/nouveau/dispnv04/nvreg.h).

**PROVEN (our patch):** the candidate retains 1080 active source lines,
field-fetch metadata, primary vertical sync edges and HD control words.
It requests an even 1124-line logical frame and a packed-window hypothesis.
**INFERRED:** this may represent two 562-line field periods, following the
successful SD approach, but it is not the same data representation as an
explicit 540-active / 562-total progressive source. **UNKNOWN:** physical
field phase, actual emitted active lines and the packed field's semantics.
The old SD result cannot prove these for the separate HD/HDMI path.

**PROVEN (arithmetic):** OSSC's porch geometry gives horizontal sync
2008..2052 in a 2200-sample line, and vertical sync 542..547 in a 562-line
frame. A 74.184 MHz clock makes that progressive modeline exactly 60 Hz.
`python -B tools/hd540_timings.py --modeline` prints this independent reference
without applying it. It is not a verified PS3 recipe or monitor guarantee.

## Receiver support remains unconfirmed

**CLAIMED (Xiaomi specifications):** G Pro 27Qi 2026 is a 2560 x 1440
monitor with HDMI 2.0 inputs. Its published FAQ does not establish
1920 x 540 progressive input support. Native panel resolution and maximum
refresh do not answer the custom input-timing question.
Source: [Xiaomi G Pro 27Qi FAQ](https://www.mi.com/global/support/faq/details/KA-632981/).
The older G Pro 27i is a separate product revision and its manual should
not be treated as certification for the owner's Qi model.

**PROVEN (MS2131 datasheet):** the reference default EDID/input list does
not advertise 540p. This does not prove the owner's firmware cannot accept
an unadvertised mode; chip-family identification also does not identify all
board/firmware behavior. Source: [manufacturer datasheet, page 8](https://atta.szlcsc.com/upload/public/pdf/source/20241106/01BD35E8B7868C1602CB31851FA6DC75.pdf#page=8).

**INFERRED (next useful checks):** establish independent receiver acceptance
with a known progressive reference source or measure the actual PS3 sync;
trace the HDMI mode configuration and AVI generation as a coordinated
path; resolve the RSX packed-field semantics before a changed candidate.
Another run of the same candidate cannot distinguish these causes.

### Owner's independent PC reference result

**CLAIMED (owner, after this audit):** the NVIDIA-generated 1920 x 540 / 60 Hz
reference works on both the Xiaomi monitor and USB capture card. The supplied
manual timing was 74.184 MHz, totals 2200 x 562, progressive, positive sync,
with the reference porches above. The actual PC HDMI timing/AVI packets were
not independently captured by this project.

**INFERRED:** assuming the GPU sent the requested timing without scaling it
to another output mode, both receivers accept at least this reference signal.
That makes the PS3 candidate raster/transmitter configuration the stronger
suspect. It does not make the PC signal identical to the PS3's unknown
waveform, particularly its interlace/field behavior, clock and AVI metadata.
The published-specification limits above and this successful owner test
are different kinds of evidence.

## Component is a separate target

**PROVEN (implementation):** dev6's preflight and transition mapper accept
HDMI only. The analog backend independently chooses encoder templates in
`0x8000f4e0` / `0x8000f9ac`; it does not use HDMI AVI packets.
**INFERRED:** component may avoid the HDMI mode-description problem, but
still needs correct raster/encoder sync and an identified compatible HD CRT.
**UNKNOWN:** working PS3 540p over component. A future component build needs
its own output guard, mode transition, baseline/rollback and live validation;
the current HDMI result is not a compatibility claim for that input.
