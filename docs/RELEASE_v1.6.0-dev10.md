# CRT Toolset v1.6.0-dev10 — in-place reprogram by format reassert (all consoles)

Pre-release. Supersedes v1.6.0-dev8. Full dev4 (HDMI custom resolutions)
feature set.

## What changes

The SD timing test no longer needs a mode change to activate the patched
240p/288p timing. After patching the timing table, dev10 clears the programmed
AV head's profile-index flag and re-requests the **same** SD mode with the
framebuffer format switched XRGB -> XBGR -> XRGB. The request differs from the
current configuration, so the firmware does not drop it as a same-mode no-op,
and the AV mode-setter runs a full reprogram from the patched table. There is no
resolution change, no 50/60 Hz flash, and it takes about half a second.

This is the primary path on **every** console. Fallbacks, used only if the
candidate is not live afterwards: the old flip via the other SD family, else a
bounce through an available HD mode. Revert uses the same order.

## Why (issue #1)

- dev6/dev7: a same-mode re-request (`videoConfigure id=4` while already id=4)
  never reaches the AV mode-setter, so clearing the head flag alone does nothing.
- dev8: the issue #1 console (NTSC, component) allows only 480i: 576i is refused
  (`8002b226`) and 720p/1080i are unavailable, so there is nothing to bounce
  through.
- dev9b, PROVEN on the owner's PAL console with the single-SD path forced
  (`/dev_hdd0/tmp/240p_force_single_sd.on`, starting at 576i): the aspect
  variant was refused (16:9 at 576i, `8002b226`), the **format** variant reached
  the mode-setter: head 1 went from stock `574/625 scan 00030000` to candidate
  `576/624 scan 00010000`, `Post-switch analog AV cache match=1`, about 0.46 s
  from BEGIN.

- Changed: `switch_begin_target`, `switch_revert`, `switch_stock_begin`
  (`app/source/switch_flow.c`); `configure_full`, `reassert` (format, then pitch
  +1024) and the force flag (`app/source/main.c`).
- Mechanism: `docs/SD_SINGLE_FAMILY_240P.md`.
- Host tests pass (`python -B tools/test_app.py`), incl. forced single-SD,
  reassert-live (no flip) and fallback-flip cases.

## Build

```text
python -B tools/build_ps3_app.py --sd-timing-fix
```

Output: `CRT-Toolset-1.6.0-dev10.pkg`, APP_VER 01.80, in-app `1.6.0-dev10`.

## Test

1. AV Multi Out at your console's SD mode (480i NTSC / 576i PAL).
2. Launch CRT Toolset v1.6.0-dev10, select **Test 240p** (NTSC) or **Test 288p**.
3. Expect the test picture almost immediately, with no black flash, and a
   15-second countdown. Press Cross to keep, or wait for the automatic revert.

If it fails, the flow reverts to stock. Pull `/dev_hdd0/tmp/ps3_240p.log` for
`Reassert variant=...`, `Format reassert id=... live=...` and `Post-switch ...`.

## Validation

- PROVEN: format reassert activates the candidate on the owner's PAL console
  with the single-SD path forced (dev9b log).
- PROVEN (owner PAL console, dev10, no force flag): Stock 576i reprograms in
  place (574/625); Test 288p activates with no flip (576/624 scan 00010000,
  `Post-switch match=1`, ~0.36 s); auto-revert reprograms back to stock
  574/625 scan 00030000 with no flip (`Automatic/cancel recovery=0`).
  Evidence: `local_only/issue1/owner_dev10b.log`. An earlier dev10 build
  flipped on revert when the pre-test state was a kept candidate; fixed (revert
  falls back only when the reprogram does not reach the mode-setter).
- UNKNOWN until tested: the same on an NTSC single-SD console at 480i (issue #1).
