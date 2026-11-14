# HDMI 240p / 288p investigation

Started 2026-11-14 while the owner tested PS1 analog audio. The initial
reference audit was offline only; subsequent stock HDMI controls are
recorded below. **UNKNOWN:** working PS3 HDMI 240p/288p. No low-line HDMI
timing candidate has been applied. The analog and 540p recipes are unchanged.

## What HDMI needs

**PROVEN (public source):** Linux v6.12's CTA mode table includes progressive
SD modes, with separate 4:3 and 16:9 video identification codes (VICs):

| Output | 4:3 VIC | 16:9 VIC | Transmitted active samples | Sample clock | AVI pixel-repeat value |
|---|---:|---:|---:|---:|---:|
| 240p, repeated pixels | 8 | 9 | 1440 x 240 | 27 MHz | 1 |
| 288p, repeated pixels | 23 | 24 | 1440 x 288 | 27 MHz | 1 |
| 240p, wider encoding | 12 | 13 | 2880 x 240 | 54 MHz | 0 |
| 288p, wider encoding | 27 | 28 | 2880 x 288 | 54 MHz | 0 |

Sources: [CTA modes](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/drm_edid.c#L765-L797)
and [PAL entries](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/drm_edid.c#L847-L879).
The 720-pixel logical reference for the repeated modes uses `DBLCLK`;
the physical sample description above expands it twice. Linux's
[mode-to-AVI helper](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/drm_edid.c#L6844-L6845)
sets repetition value 1 for that flag. The AVI field
represents repetitions minus one; its location is established by the
[AVI packet packer](https://github.com/torvalds/linux/blob/v6.12/drivers/video/hdmi.c#L153-L157).
These are standard-reference facts, not observed PS3 output.

**PROVEN (reference arithmetic):** the chosen integer totals are 262 NTSC
or 312 PAL lines. At the listed clocks and horizontal totals they give
60.054449 Hz / 50.080128 Hz, respectively. Do not silently label those exact
59.94 Hz / 50 Hz or promise identical game cadence. These are one reference
per family, not an exhaustive list of acceptable low-line timings.
Source: `tools/hdmi_lowline_reference.py`, checked against the pinned table.

**INFERRED:** the repeated-pixel encoding is a useful first target because
the PS3 already has an interlaced SD HDMI description using the same AVI
repeat value. A receiver must still support the actual low-line signal.
**UNKNOWN:** transmitter acceptance, receiver lock, audio-clock behavior and
full-picture geometry. The mode table alone establishes none of these.

## Fresh retail 4.93 audit

**PROVEN (private ELF reads):** the input `sysmgr_ss.elf` has SHA-256
`07c0bd2a2403f65b6d741e11a15106114365cb78165833bac726476f671b64c9`.
The checked selector at `0x800105d0` uses 25 twelve-byte HDMI templates
beginning at `0x8003997b`. None has VIC 8/9/12/13/23/24/27/28.
This is absence from this checked table, not a hardware impossibility result.

**PROVEN (private backend/AVI trace):** these existing SD routes provide
concrete anchors:

| Family | Backend ID | Format code | Template index / address | Stock VIC | Template repeat nibble |
|---|---:|---:|---|---:|---:|
| NTSC interlaced | 0 | `0x106` | 2 / `0x80039993` | 6 | 1 |
| PAL interlaced | 5 | `0x115` | 13 / `0x80039a17` | 21 | 1 |

Sources: backend format table `0xc00003fc`, selector `0x800105d0`, packet
construction `0x8000f9ac`, configuration `0x80010824` and AVI builder
`0x8000fde0`. The builder takes the VIC from template offset 2 and the
pixel-repeat nibble from offset 0; aspect handling can adjust the VIC.
Only decoded facts and hashes are published; no Sony template/code dumps.

**PROVEN (fresh timing-record reads):** profiles 1/2 hold 480-line
interlaced sources; profiles 3/4 hold 576-line interlaced sources. They
store horizontal multiplier 4 and width 2880. Progressive profiles 5/6
instead store multiplier 2, width 1440 and source heights 480/576.
All six select clock-table index 1, whose nominal float is 54.0 in both
banks. This is a stored value, not a measured HDMI pixel clock.
Sources: metadata `0x80039148 + 24 * profile`, raster
`0x80038b28 + 56 * profile`, clock-index table `0x800394d8` and
clock banks `0x800393e8` / `0x80039460`.

**PROVEN (private consumer trace):** display setup `0x800005e4` feeds the
metadata's source-interlace field to `0x80003a24`, which changes RSX scan
controls. It separately programs the raster through `0x80003bcc` and
source placement through `0x80002ca4`. HDMI packet construction is another
path. **INFERRED:** merely halving metadata height, changing AVI VIC, or
reusing the analog encoder edit is not a complete HDMI implementation.

**PROVEN (prior implementation/live controls):** changing the shared HDMI
template byte affected the transmitter command as well as the AVI packet;
that stock control lost the picture. The subsequent 540p implementation
isolated the AVI builder and eventually produced receiver-detected
progressive 540-line output. Sources: [shared-field finding](HD540_DEV7.md#live-stock-and-hdmi-description-control-readback),
[AVI isolation](HD540_DEV8.md), [540p result](HD540_DEV18.md#separate-receiver-check-and-final-verdict).
**UNKNOWN:** whether the same transmitter configuration can pass low-line
SD when the raster and AVI are changed coherently. Do not transplant the
540p patch with just a different height.

## First runnable work

**PROVEN (project implementation):** the offline tool generates the two
sample encodings above, checks alternate-row source bounds, and builds a
self-authored RGB/BT.601 AVI reference packet with checksum. It has no
network, USB or display-control path:

```powershell
python -B tools/hdmi_lowline_reference.py
python -B tools/hdmi_lowline_reference.py --family pal --aspect 16:9
python -B tools/hdmi_lowline_reference.py --encoding native2880
python -B tests/test_hdmi_lowline_reference.py -v
```

An optional known private input adds a read-only address/hash audit:

```powershell
python -B tools/hdmi_lowline_reference.py --firmware <private-sysmgr_ss.elf>
```

Unknown firmware is refused before parsing its tables. The successful audit
and bounded initial inventory are saved privately in
`local_only/hdmi_lowline/{reference-audit.json,inventory.json}`.
**PROVEN (validation):** all nine focused tests pass, and the known ELF audit
confirms the SD routes and absent low-line VICs. These checks do not exercise
a console-side patch.

**PROVEN (bounds arithmetic):** alternate-row fetching from an unchanged
720 x 480 source ends at row 478; from 720 x 576 it ends at row 574. The
tool checks the last accessed byte against the original extent and the
doubled pitch against the 16-bit fetch field. **UNKNOWN:** the complete
RSX window/scaling recipe on the SD HDMI path. A bounded read is not a
correct-picture or pixel-perfect-emulation result.

## Implementation and validation sequence

The following are **INFERRED next steps**, not implemented capabilities:

1. Audit the native HDMI 480p/576p selection and restore contract. Prefer
   starting from supported progressive SD output, avoiding an assumption
   that GameOS exposes interlaced SD over HDMI. Determine how RSX sample
   multiplication, clock division and the transmitter core agree before
   choosing between adapting profiles 5/6 and the existing interlaced SD
   profiles. Keep renderer dimensions and pitch separate from output timing.
2. Prepare a separately identified CFW helper with checked firmware identity,
   dynamic sysmgr discovery, source-buffer bounds, explicit refusal while
   SD enforcement is active, and a console-side 15-second restoration path.
   Reuse the lifecycle/rollback design, not 540p's hardcoded source guards.
   Preserve the exact selected connector and boot settings; use native mode
   transitions, without a reboot or an altered saved default for recovery.
3. Run a stock SD HDMI round trip, then isolate AVI/repetition changes from
   raster and fetching changes. The old 720p/1080i prerequisites belong to
   the 540p protocol; they are not automatically prerequisites for this one.
   A control that loses the picture stops that sequence.
4. Test one low-line family first. Check software state, receiver signal and
   progressive classification, actual dimensions, full geometry, audio and
   restoration independently. An OBS canvas size or a cached height is not
   proof of the input timing. Receiver support for these modes is UNKNOWN.
5. Only after a working timed output, test games, persistent enforcement and
   source-resolution changes. PS2 BC emulator direct output is a separate
   integration; preserved GS resolution changes, pixel mapping and end-to-end
   latency remain UNKNOWN. A progressive HDMI raster does not establish
   zero buffered frames or zero latency.

## Stock HDMI controls (2026-11-14 evening)

**PROVEN (checked artifact and live log):** a separate stock-only helper
selected native 480p from idle 1080p XMB, held for 15.109079 seconds after
readiness and restored the exact saved native/AV-head state. The final
helper SHA-256 is
`97dc5b775eaf329b288f2ec2d0e7eee2fd0262b996f5971135562b8102f03df8`.
Its LV1 transport refuses writes; mode changes use the fingerprinted native
VSH API. It does not replace the installed app, saved boot preference or
release helper. Evidence: ignored
`local_only/hdmi_lowline/480p-20261114-223242/{helper.log,report.json}`.

**PROVEN (software/USB readback):** the active AV head described a 720x480
source, pitch 3072, 480-active/525-total raster; the software AVI packet had
VIC 3 with zero repeat. One sparse USB live sample returned 720x480 and scan
0x07, while cached dimensions stayed 1920x1080 and cached signal said absent.
**UNKNOWN:** the exact capture-firmware mapping/refresh. Evidence: the same
folder's `receiver.json`; [status interpretation](CAPTURE_INPUT_STATUS.md).

**PROVEN (saved OBS frames):** the picture stayed black during the stock
480p hold, and returned on restoration to 1080p. This control therefore
fails picture validation despite successful software completion. A previous
undisturbed stock-480p hold was also black before any live USB-register query.
**CLAIMED (owner):** selecting normal 480p through XMB likewise makes OBS
black. **INFERRED:** the 480p capture-path failure is independent of a
custom low-line timing patch. Receiver/USB processing and the HDMI chain
need to be separated before treating black 240p as a PS3 timing failure.

**PROVEN (initial control limitation):** native mapper request 5 returned
failure on this connected HDMI setup. The first helper nonetheless
submitted its manually audited 576p mode; it timed out and latched failed
recovery. Later independent readback showed idle stock 1080p and no RAM
patch, and a normal webMAN soft reboot cleared that stopped helper. No
storage check was started. The revised guard refuses an unavailable native
mapping before submitting a mode, avoids restarting an in-flight recovery,
and allows a bounded 20-second recovery interval. Evidence:
`576p-20261114-222546/`, `post-control/`, and the revised artifact above.

**Current boundary:** no low-line HDMI candidate, package or app-menu option
has been tested. Console-side stock recovery works in the revised NTSC
control; working HDMI 240p/288p and a readable SD capture remain **UNKNOWN**.

## Independent stock controls and direct capture

**CLAIMED (owner, later follow-ups):** normal 480p remains black in OBS
after a normal PS3 reboot, OBS custom 720x480 / 60 fps / YUY2, and disabling
PS3 Deep Colour. Direct PS3-to-Xiaomi G Pro 27Qi HDMI 480p produces a visible
picture. The capture connection uses an HDMI splitter. Sources: owner
messages; private `owner-monitor-control.json` and `manual-controls.json`.
**INFERRED:** stock 480p can work on this PS3/display pairing; the failing
capture chain must be checked separately from custom low-line timing.

**PROVEN (direct USB-video captures):** with OBS's capture source explicitly
deactivated, FFmpeg opened only `USB3 Video`, recorded no audio, and saved
six frames each using YUY2 and MJPEG at 720x480 / 60 fps. Both completed
with exit code 0; every saved frame was spatially uniform/dark with no XMB
picture. The YUY2 capture logged no errors. MJPEG logged startup decode
warnings before its six dark frames; do not call that path error-free.
Evidence: ignored `local_only/hdmi_lowline/direct-card/20261114-230500/`,
including `report.json`, `options.log`, frames and per-format FFmpeg logs.

**PROVEN (capture identity/lock observation):** the card advertised both
tested USB formats at this capture size. Immediately preceding these
captures, the sparse live status returned 720x480 and scan byte 0x07;
the cached status still described 1920x1080 and signal absent. These are
firmware-specific reports, not a physical clock or decrypted-image proof.
Source: the same captured capabilities and recorded diagnostic output.

**INFERRED:** OBS processing alone does not explain the black picture;
the same symptom occurs through a separate reader. **UNKNOWN:** whether
the splitter/link, HDCP negotiation, capture firmware or another part of
the card's input path causes it. A successful direct-monitor test does not
validate the splitter output feeding the card. The next physical control
uses that exact splitter output/cable with the monitor at stock 480p.
**CLAIMED (Sony documentation):** HDMI output requires HDCP compatibility;
that makes negotiation a possible investigation area, not an established
cause here. Source: [Video Output Settings](https://manuals.playstation.net/document/en/ps3/current/settings/videooutput.html#hint).
No firmware update, EEPROM write, or low-line timing patch was performed.

**CLAIMED (owner instruction):** OBS was subsequently closed explicitly.
The direct-capture cleanup attempted to reactivate its source, but OBS was
no longer exposing the expected source UI; it was left closed as instructed. Source: owner message
and the private capture report. This is not an OBS crash finding.
