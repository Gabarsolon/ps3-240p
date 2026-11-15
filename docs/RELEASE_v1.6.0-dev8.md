# CRT Toolset v1.6.0-dev8 — single-SD timing forces a real mode change (HD bounce)

Pre-release. Supersedes v1.6.0-dev7. Full dev4 (HDMI custom resolutions)
feature set.

## Why dev7 failed on the issue #1 console

dev6/dev7 tried to activate the patched SD timing on a single-SD (region-locked)
console by zeroing the programmed head's AV profile-index cache flag and
re-requesting the current mode. The dev7 live log (issue #1, NTSC, component)
proved dev7 picked the right head (head 1, `state 2 -> 0`) but the timing still
never activated:

```text
Single-SD fallback: invalidate head=1 (target=4 default=0)
AV head 1 profile-index cache invalidated (0000000000000002->0000000000000000)
videoConfigure id=4 result=00000000
Post-switch analog AV cache match=0 ... Head1=480/525   <- still stock 480i
```

Root cause: `videoConfigure(id=4)` while the console is **already id=4** is a
no-op — cellVideoOut returns success without calling the sysmgr mode-setter
(`0x800005e4`), so the cleared flag is never consulted and the patched table is
never loaded. The 576i bounce dev4/6/7 used is refused on a single-SD console
(`videoConfigure id=5 result=8002b226`), and no same-mode reprogram path exists.

## What dev8 changes

The single-SD fallback forces a **real** mode change. When the other SD family is
refused, it bounces through an available **HD mode — 720p (id 2), else 1080i
(id 1)** — then returns to the SD target. The `480i -> 720p -> 480i` return is a
genuine id change, so the mode-setter runs and loads the patched 240p table.
(480p is not reachable: it shares id 4 with 480i and differs only by `scanMode`,
which `configure()` does not set.) `switch_revert` uses the same HD bounce so
stock timing is restored the same way.

- The HD bounce is briefly non-15 kHz (a 15 kHz SCART CRT shows black during the
  bounce) but transient; the flow lands on patched 240p or reverts cleanly.
- If no HD mode is available either, dev8 falls back to the old head-flag
  invalidation (ineffective per issue #1, but harmless).
- Dual-SD consoles (e.g. the owner's PAL unit) are unchanged: the 576i/480i flip
  still succeeds and the HD bounce is never reached.

- Changed: `switch_begin_target` / `switch_revert` (`app/source/switch_flow.c`).
- Mechanism and addresses: `docs/SD_SINGLE_FAMILY_240P.md`.
- Host tests: 44/44 (`python -B tools/test_app.py`), incl. an HD-bounce case and
  a no-HD last-resort NTSC head-1 case.

## Build

```text
python -B tools/build_ps3_app.py --sd-timing-fix
```

Output: `CRT-Toolset-1.6.0-dev8.pkg`, APP_VER 01.77, in-app `1.6.0-dev8`.

## Test (NTSC + component / single-SD console — issue #1)

1. AV Multi Out, start at 480i (NTSC). Confirm XMB shows 480i.
2. Launch CRT Toolset v1.6.0-dev8.
3. Select **Test 240p**.
   - Expect a brief black flash (the 720p bounce) then the 240p test picture
     with a 15-second countdown.
4. Press Cross within 15 s to keep, or wait for the automatic revert (another
   brief black flash back to stock 480i).

If it still cannot activate, the flow reverts to stock cleanly (no reboot).
Pull `/dev_hdd0/tmp/ps3_240p.log` for `Single-SD fallback: HD bounce id=...`,
the `videoConfigure` lines and `Post-switch analog AV cache match=...`.

## Validation

- PROVEN: dev8 bounces through an HD mode to force the mode-setter; host tests
  cover the HD-bounce and no-HD paths. Dual-SD flip path unchanged.
- UNKNOWN until a live issue-#1 console test: whether the `480i->720p->480i`
  return actually loads the patched table on that console (i.e. whether 720p is
  configurable over its component cable, and whether the mode-setter reads the
  patched raster). Reversible: a failed activation reverts to stock.
