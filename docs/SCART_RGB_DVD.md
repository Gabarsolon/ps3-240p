# SCART RGB green tint on DVD/BD playback: mechanism and live probe

Owner report (2026-11-15): PS3 Slim CECH-25xx, Evilnat 4.93, AV MULTI OUT with
an RGB SCART cable into a CRT. XMB and games look correct in RGB; DVD-Video and
Blu-ray movie playback show a bright green picture. The same discs over
composite on the same cable look correct. This file records what was proven on
the console, not the forum folklore (that is in ignored `local_only/scart_green/`).

## Proven on the live console (read-only snapshots)

Method: webMAN `getmem.ps3mapi` read-only process-memory reads of the running
`main_vsh.self`, plus an isolated, auto-restoring probe plugin
(`experiments/scart_rgb/plugin.c`, module `crt_scart_probe`). Private evidence:
`local_only/scart_green/snap2_*.json`, `snap_*.json`, `crt_scart.log`.

- **PROVEN:** DVD/BD playback runs as its own process, `main_bdp_BDVD.self`,
  alongside `main_vsh.self`. VSH still owns the analog AV state.
- **PROVEN (AV state block, head 1 = analog):** identical 576i geometry in both
  states (`vid=3`, `720x576`, pitch `0xc00`). Only two fields differ between
  XMB and DVD playback:
  | field | XMB (correct RGB) | DVD (green) |
  |---|---|---|
  | video_format | `0x07` (ARGB_8BIT) | `0x10` |
  | colour space | `0x09` (RGB_12) | `0x0a` (YUV444_12) |
  The colour-space codes match Linux `ps3av.h`
  (`PS3AV_CMD_VIDEO_CS_RGB_12 = 0x0009`, `YUV444_12 = 0x000a`). INFERRED: the
  player selects a YUV output; the TV, held in RGB by SCART pin 16, routes the
  Y (luma) channel to the green gun, hence an all-green picture.
- **PROVEN (negative result):** re-submitting head 1 through VSH's own
  `avset_setvideomode` wrapper (VA `0x5d9d8c`, fingerprinted before call) with
  the colour-space field forced to `0x09`, and separately with video_format
  forced to `0x07`, **changed the stored fields and visibly changed brightness
  but did not remove the green.** The analog-encoder colour space is therefore
  not driven by that video-head record field alone.

## Where the colour space is actually decided (offline, private 4.93 VSH)

Addresses are 4.93 CEX `vsh.elf` virtual addresses (fixed-address module;
private image stays in `local_only/`). No Sony code is reproduced here.

- **PROVEN:** the analog colour space reaches the backend as AV command
  `0x00030004` (avset `AV_VIDEO_CS`), built in the AV command assembler around
  `0x5e0b00` / `0x5e16bc`. Its `av_cs_out` comes from a mapper at `0x5e8690`
  keyed on `(avport, video_id, flags & 0x31)`.
- **PROVEN:** the per-head colour-space value is assigned in builder
  `0x5db0cc` by **mode bucket**, not by the caller's cs field:
  bucket `{1,8}` → RGB (`1`/`9`), bucket `{2,16}` → YUV (`2`/`a`),
  bucket `{4,32}` → YUV (`4`/`c`); 8-bit vs 12-bit is chosen from three TOC
  flags (`0x6eef6c/70/74`) and `state[0x104]`. XMB's mode lands in the RGB
  bucket; the player requests a mode that lands in the YUV bucket.
- INFERRED: forcing RGB therefore requires re-emitting backend command
  `0x00030004` with `av_cs_out = RGB` for `AVPORT_AVMULTI_0` (or flipping the
  routing input that selects the bucket), **not** editing the video-head
  record. This is the next live experiment and is still UNKNOWN.

## Live result 2026-11-15 (camera + probe): one-shot cannot fix it

Captured on the owner's CRT with an FHD camera (`local_only/scart_green/cam/`).

- **PROVEN (green baseline):** DVD playing, head-1 record `1 3 2d0 240 c00 10 0 10000 a`, picture green.
- **PROVEN (negative, full record):** re-submitting the complete known-good XMB
  record `1 3 2d0 240 c00 7 0 10000 9` through `avset_setvideomode` returned
  rc=0 and VSH stored cs=`9`, but the camera still showed green.
- **PROVEN (player re-asserts):** the probe's bounded auto-restore logged
  `restore_skipped` because the live record had already changed back — the
  `bdp_BDVD` player re-drives the analog AV state continuously, so any one-shot
  correction is overwritten within a frame.

Therefore the video-mode packet is the wrong lever and a single write cannot
hold. A fix must make the player's own colour-space decision yield RGB. The
realistic, reversible target is a RAM patch at the VSH analog colour-space
decision (mapper `0x5e8690` / builder `0x5db0cc`) forcing `AVPORT_AVMULTI_0`
to RGB, in the same class as the project's existing RAM patches and the PS2
`dvdplayer.elf` RGB patch. This is **UNKNOWN / not yet attempted**.

## LV1 result 2026-11-15: encoder is command-driven, not cache-driven

Tooling added to `experiments/scart_rgb/plugin.c`: ps3mapi `SET_PROC_MEM`
(0x32) process poke, and LV1 `PEEK`/`POKE` (0x1008/0x1009) reusing the hd540
transport. `patch_resolve` (`app/source/lv1_patch.c`) resolves the `sysmgr_ss`
analog AV context live.

- **PROVEN (LV1 poke works):** a sentinel `0xdead` written to the head-1
  request word and read back confirmed LV1 RAM writes land.
- **PROVEN (sysmgr AV cache, head 1 = analog):** request word `req[3]` =
  `ctx+0xf8`. XMB `0x00000000_00000007` (format 7 = analog RGB8); DVD
  `0x00000009_00000010`. Only this word flips between RGB and green.
- **PROVEN (negative):** forcing `req[3]` back to `7` (or geometry words) in
  LV1 RAM did **not** change the output. The context is a software cache/mirror
  for queries; the analog encoder is programmed by the incoming AV command, not
  re-read from this cache. Writing it alone never reprograms the encoder.
- **PROVEN (where it is programmed):** `sysmgr_ss` display-setup
  `0x800005e4` (the same function HD540_DEV18 documents for RSX pitch) consumes
  the validated request (`0x80025c9c`) and programs the head via callees
  including backend/encoder setup `0x800015b4`. The analog encoder colour space
  is committed from the command here, through the Syscon backend path, not from
  `ctx+0xf8`.
- **PROVEN (VSH is not authoritative during playback):** every VSH-side lever
  (setvideomode record, full XMB record, AVMULTI backend `0x30004` av_cs_out)
  returned rc=0 but left the picture green. The `bdp_BDVD` player owns the
  display; its command, routed to `sysmgr_ss`, is what the encoder follows.

**Remaining to make it work (UNKNOWN, deeper + riskier):** force RGB at the
authoritative point — either (a) an LV1 RAM code patch in the `sysmgr_ss`
encoder-colour-space commit so AVMULTI is always RGB (affects every caller;
LV1 code patch, reboot-recoverable but brick-adjacent care needed), or (b) a
userland patch of `bdp_BDVD` so the player requests RGB (the PS3 analogue of
the PS2 `dvdplayer.elf` fix). Neither is a one-shot RAM-value write.

## Breakthrough 2026-11-15: analog colour space is fully controllable

Tooling: `experiments/scart_rgb/plugin.c` now also patches `sysmgr_ss` LV1
**text** (translate VA→physical, 64-bit-safe poke, up to 4 stacked patches,
auto-restored) and dumps any process on-console. Automation: the Pico DS3
bridge on COM9 (`local_only/scart_green/ctl.py`) drives the PS3, and an FHD
camera (`ffmpeg dshow "video=FHD Camera"`) reads the CRT. All test images in
ignored `local_only/scart_green/cam/`.

**PROVEN (encoder colour-space path, offline + live):** the analog encoder
colour space is set in `sysmgr_ss` `0x8000f7a8` (called from backend builder
`0x8000f9ac` for AV ports 0x10/0x11), which edits the selected 68-byte CXM
template's register pairs (templates at service `0xc0000000`; 576i = template
1). The packet is `{avport, av_vid, av_cs_out, av_cs_in}` at the pointer in
`r4`. The branch at `0x8000f7dc` (`bgt`, on `av_cs_in`) and the branch at
`0x8000f85c` (`bne`, on `av_cs_out==0`) select, respectively, a reg-0x37
"component" setup (DVD's natural path → green) versus a reg-0x38=0x20 "RGB
output" setup (XMB's path → correct RGB for RGB input).

**PROVEN (live, camera):** two stacked LV1 code patches applied at XMB, held
across an XMB→DVD relaunch:
- `0x8000f7dc`: `0x419d0064` → `0x48000064` (force the `av_cs_out` path), and
- `0x8000f85c`: `0x409e0050` → `0x60000000` (nop → force the RGB-output branch).

Result: **the green is gone.** XMB stays correct. The DVD now outputs on the
RGB DACs, but shows the **raw YUV framebuffer as RGB** (sky cyan, grass red) —
the encoder is in RGB-output mode with **no YUV→RGB conversion applied**.
Evidence: `cam/patch2_test.jpg`, `cam/pink3.jpg` (vs `cam/g1.jpg` green).
A single `0x8000f7dc`→nop (route DVD to the reg-0x37 path) leaves it green,
confirming the reg-0x37 path is the component/green output.

**INFERRED:** RSX scans the DVD video surface out as YCbCr to the encoder.
Neither encoder branch performs YCbCr→RGB. Correct RGB SCART therefore needs
the conversion enabled — either the CXM CSC (a template/register not toggled
here, e.g. reg 0x52/0x66, needs the CXM4027R datasheet or empirical brute) or
the RSX scanout CSC (`0x800015b4`, head-1 control `0x680520`, colour-space
field `param_3[9]` in bits 16..18). This is the remaining, unfinished step;
each live trial costs an XMB→DVD cycle (~40 s). **UNKNOWN:** the exact
conversion register and value.

## PROVEN 2026-11-15: the CXM encoder does not convert YCbCr→RGB

Camera + pixel-stat brute (`local_only/scart_green/stat.py`, `cycle.py`).
`e050(ctx,tmpl,reg,val,mask)` does `reg=(old&mask)|val` on a template pair.

- Baselines (mean RGB of the lit screen): green `cam/g1.jpg` G-dominant
  (R0.33 G1.00 B0.48); raw-YUV-as-RGB `cam/pink3.jpg` R-dominant
  (R1.00 G0.74 B0.95); correct XMB `cam/xmb_clear.jpg` B-dominant.
- Trial (two base patches + `0x8000f844` reg 0x66 val 1→0, `cam/t_cs66.jpg`):
  the **letterbox black bars render magenta**, i.e. Cb/Cr≈128 drive R/B with
  no −128 offset and no matrix. **PROVEN:** in RGB-output mode the encoder
  emits the YCbCr samples straight onto the RGB DACs; it has no YCbCr→RGB
  CSC. Changing encoder registers (0x38, 0x66, …) only re-labels the output,
  it cannot colour-correct the video. The reg-0x37 path is component (green);
  the reg-0x38 path is RGB passthrough (raw YUV). Neither converts.

**Conclusion:** correct RGB SCART for the YCbCr film requires the conversion
upstream of the encoder — the **RSX scanout CSC** (`sysmgr_ss` `0x800015b4`,
head-1 control `0x680520`, colour-space field from `param_3[9]`, bits 16..18),
so RSX outputs RGB and the encoder RGB-passthrough patches then show it
correctly. The menu (RGB graphics drawn by the player) is already close under
the passthrough patches; only the decoded video is YCbCr. The RSX-CSC route is
not yet attempted. **UNKNOWN:** the colour-space value that makes RSX convert,
and whether RSX exposes YCbCr→RGB scanout conversion at all.

## PROVEN 2026-11-15: RSX scanout colour-space is colorimetry, not a CSC

`sysmgr_ss` `0x800005e4` selects a 0xc-byte scanout descriptor per video id
(tables at `c000af50`/`af60`, index via `c000af58`); byte 9 is the scanout
colour space written to RSX PCRTC `0x680508`/`0x680520` bits 16..18.
Offline dump: byte 9 is **0 for every SD mode (480i/576i/480p/576p)** and
**2 or 3 for HD** — i.e. the SD-vs-HD colorimetry standard, identical for XMB
and the DVD at 576i. It is **not** a YCbCr→RGB enable.

**PROVEN (whole-pipeline conclusion):** the decoded film is YCbCr-valued
pixels placed into the framebuffer by the player. The encoder has no
YCbCr→RGB CSC (magenta-black result), and RSX scanout only labels
colorimetry. The YCbCr→RGB conversion is performed by the **player's
compositor**, which does it for HDMI-RGB output but writes YCbCr for analog
(the copy-protection path). Therefore nothing in the AV manager — encoder
templates or RSX scanout — can colour-correct the video; the pixels are
already YCbCr before the AV manager sees them.

**Remaining path (high effort, UNKNOWN):** make the player composite to RGB
for analog. That means patching `bdp_BDVD` (a ~4 MB stripped userland binary,
dumpable with the probe's `SP_DUMP`) or its decode/compositor library, to take
its HDMI-RGB conversion path for analog output. No in-AV-manager fix exists.

## What this release/experiment DID achieve

- **PROVEN:** the analog green on DVD/BD over RGB SCART is the encoder in
  component (YPbPr) mode; two reversible LV1 code patches
  (`0x8000f7dc`→`0x48000064`, `0x8000f85c`→`0x60000000`) switch it to RGB
  output. The green is eliminated and XMB is unaffected.
- The DVD **menu** (RGB graphics) is then close to correct; only the decoded
  video is YCbCr and needs the player-side conversion above.
- Tooling retained: `experiments/scart_rgb/plugin.c` (LV1 peek/poke + stacked
  code patches, process dump), `local_only/scart_green/` (controller `ctl.py`,
  camera, `stat.py`, `cycle.py`, offline `sdec.py`).

## Reference (external, CLAIMED)

Linux `drivers/ps3/ps3av.c` `ps3av_set_videomode_packet` sends analog
`av_video_cs = RGB8` only when the mode id carries `PS3AV_MODE_RGB`, which is
set from `region & PS3AV_REGION_RGB`; otherwise it uses the mode table's cs.
This matches the retail bucket behaviour above. Source:
https://github.com/torvalds/linux/blob/master/drivers/ps3/ps3av.c

## Risk note for the next step

Re-emitting an AV backend command with a wrong field can blank analog output
until a reboot. It cannot brick the console (no flash/Syscon writes). The probe
holds any change for a bounded interval and auto-restores, and a soft reboot
clears everything.

## PROVEN 2026-11-16: encoder fix reproduced live; player +0xf0 is NOT the lever

Live test on the owner's console (PAL 576i, RGB SCART, FHD camera). agy's five
single-word `bdp_BDVD` candidates from `local_only/scart_green/bdp_dump/agy_findings.md`
were tested against the running player. Candidate 1 (my test order) = the getter
`CVDisplaySurface::GetColorSpace()` at `0x008da954`, `0x806300f0` -> `0x38600000`
(return 0 = RGB).

- **PROVEN (encoder-RGB patch only activates on an encoder reprogram):** the two
  LV1 `sysmgr_ss` text patches (`0x8000f7dc` -> `0x48000064`,
  `0x8000f85c` -> `0x60000000`) do nothing while the player is already playing:
  the encoder colour space is committed only on an actual head reprogram, which
  happens at **player launch**, not per frame. Applied during playback = still
  green. Applied **at XMB and held across an XMB->DVD relaunch** = green gone,
  encoder in RGB passthrough. This is the required procedure (matches the
  2026-11-15 breakthrough). Each cycle ~40 s; the plugin holds max 60 s
  (`hold_seconds` 1..60) and refuses `apply`/snapshot while a patch is held
  (one-owner lock), so the reprogram must come from the player relaunch, not the
  probe. Player pid changes per launch (`0x1010200` -> `0x1030200`); re-poke the
  process candidate into the new pid (query `getmem.ps3mapi?proc`).
- **PROVEN (negative, Candidate 1 getter):** with the encoder in RGB passthrough,
  two data points, both wrong colour:
  - player surface = 1 (default, no poke): whole picture magenta
    (`cam/step3_launch.jpg`).
  - player surface = 0 (getter -> 0, Candidate 1): green sky, magenta/red ground
    (`cam/step4_cand1.jpg`).
  Neither is correct RGB. agy's model (field `0xf0` == 0 -> "identity RGB
  passthrough" -> correct) assumes the framebuffer is already RGB; the decoded
  film pixels are mis-coloured under **both** shader branches, so the `+0xf0`
  field is not the fix. All five candidates route to the same RGB-identity
  branch, so none can colour-correct YCbCr content. Getter restored to
  `0x806300f0` after the test; LV1 patches auto-reverted.

**Remaining (handed to a fresh offline RE agent, no console needed):** find why
the player's **HDMI** output is correct RGB and force that same conversion for
the analog surface. Target `FUN_00753b8c` (`0x00753b8c`) coefficient math and the
HDMI-vs-analog path split in the dumps
(`local_only/scart_green/bdp_dump/bdp_0x10000.bin`,
`bdp_0x810000.bin`), i.e. the actual YCbCr->RGB matrix selector, not the `0xf0`
toggle. See `local_only/scart_green/bdp_dump/agy_task2.md`.
