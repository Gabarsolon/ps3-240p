# CRT Toolset v1.6.0

Includes everything in v1.6.0-dev4 (HDMI custom resolutions: 540p, 1024x768,
1280x768, 1280x1024, 1920x1200) plus the SD timing fix below.

## Fix: 240p/288p test on single-SD-standard consoles (issue #1)

The SD timing test (Test 240p / Test 288p) now reprograms the display in place
instead of switching to the other SD standard. It clears the programmed AV
head's profile flag and re-requests the same SD mode with the framebuffer
format toggled, so the AV mode-setter reloads the patched timing. No
resolution change, no 50/60 Hz flash, about half a second. Region-locked
consoles that expose only 480i (NTSC) or 576i (PAL) now work. If the candidate
is not live afterwards, the old flip and then an HD bounce are used as
fallbacks. Details: `docs/SD_SINGLE_FAMILY_240P.md`.

- PROVEN (owner PAL console): activates and auto-reverts with no flip.
- PROVEN (issue #1 reporter, NTSC CRT, component and composite): "It works!"
  Checked with Sonic CD and PSOne Classics MegaMan Legends
  (https://github.com/Gabarsolon/ps3-240p/issues/1#issuecomment-6926805195).

## Build

`python -B tools/build_ps3_app.py --sd-timing-fix` -> `CRT-Toolset-1.6.0.pkg`,
APP_VER 01.81, in-app `1.6.0`. Same code as v1.6.0-dev10.

SHA-256: `a090c068241467e99dfb8c69a9625cae6ef4fdd34b91d58892b18cb1bb757172`

## Test

1. AV Multi Out at your SD mode (480i NTSC / 576i PAL).
2. Launch CRT Toolset, select **Test 240p** (NTSC) or **Test 288p**.
3. Cross keeps it; otherwise it reverts after 15 s.

If it fails, the flow reverts to stock. Log: `/dev_hdd0/tmp/ps3_240p.log`.
