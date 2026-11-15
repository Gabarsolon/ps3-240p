# CRT Toolset v1.6.0-dev6 — single-SD-family timing via head cache invalidation

Pre-release. Supersedes v1.6.0-dev5. Full dev4 (HDMI custom resolutions)
feature set.

## What changes from v1.6.0-dev5

dev5 let region-locked consoles past the mode-detection gate, but the timing
test still needed to configure the **other** SD family (480i↔576i) to make the
AV manager reprogram the patched table. A console that exposes only its own SD
standard (NTSC → 480i, PAL → 576i) cannot configure the other family, so the
test failed with:

```text
RAM restored; display recovery failed. Reboot Console
```

dev6 keeps that flip as the primary path (no change on consoles that can
configure both families) and adds a fallback: when `configure(alternate)` fails,
it invalidates the AV manager's per-head cached profile index and re-requests
the current mode, which reprograms the RSX CRTC from the patched table **without
any SD-family change**. `switch_revert` does the same on recovery, so a failed
flip no longer demands a reboot.

- New: `patch_invalidate_head()` (`app/source/lv1_patch.c`).
- Changed: `switch_begin_target` / `switch_revert` (`app/source/switch_flow.c`).
- Mechanism and addresses: `docs/SD_SINGLE_FAMILY_240P.md`.
- Host tests: 44/44 (`python -B tools/test_app.py`).

## Build

```text
python -B tools/build_ps3_app.py --sd-timing-fix
```

Output: `CRT-Toolset-1.6.0-dev6.pkg`, APP_VER 01.75, in-app `1.6.0-dev6`.

## Test (NTSC + component / single-SD console)

1. AV Multi Out, start at 480i (NTSC). Confirm XMB shows 480i.
2. Launch CRT Toolset v1.6.0-dev6.
3. Select **Test 240p**.
   - dev5: "RAM restored; display recovery failed. Reboot Console".
   - dev6 expected: proceeds without switching to 576i; the 240p test picture
     appears with a 15-second countdown.
4. Press Cross within 15 s to keep, or wait for the automatic revert.

If it still cannot activate, the flow reverts to stock cleanly (no reboot
needed). Pull `/dev_hdd0/tmp/ps3_240p.log` for the `AV head ... cache
invalidated` and `videoConfigure` lines.

## Validation

- PROVEN: primary (flip) path unchanged; owner's PAL console unaffected.
- UNKNOWN until a live single-SD console test: whether the repeated same-mode
  request reaches the AV mode setter so the invalidated flag triggers the
  reprogram. Reversible: a failed activation reverts to stock.
