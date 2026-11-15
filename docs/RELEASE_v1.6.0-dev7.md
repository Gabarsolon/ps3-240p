# CRT Toolset v1.6.0-dev7 — single-SD timing targets the active AV head

Pre-release. Supersedes v1.6.0-dev6. Full dev4 (HDMI custom resolutions)
feature set.

## What changes from v1.6.0-dev6

dev6 added a single-SD-family fallback: when the 480i<->576i flip cannot be
used (a region-locked console that refuses the other SD family), it invalidates
the AV manager's per-head cached profile index and re-requests the current mode,
reprogramming the RSX CRTC from the patched table without a family change.

dev6 chose the head from the SD target (`target==5 ? head 1 : head 0`), assuming
480i always runs on head 0 and 576i on head 1. GitHub issue #1 disproves that:
an NTSC console (480i, component) drives its active 480i output on **head 1**.
dev6 therefore invalidated head 0 — which was idle (`state=0`) — while the live
head 1 (`state=2`) kept its stock 525-line timing. The candidate never became
active and the flow reverted cleanly with:

```text
Timing was not activated. The active analog timing did not match the candidate.
Previous timing and mode were restored.
```

dev7 picks the head to invalidate from the AV cache itself: the head that is
actually programmed (`state==2`). The target-derived default is used only when
detection is ambiguous (zero or both heads active). `switch_revert` uses the
same detection, so recovery also clears the live head.

- New: `patch_active_head()` (`app/source/lv1_patch.c`) — the programmed head, or -1.
- Changed: `switch_begin_target` / `switch_revert` (`app/source/switch_flow.c`).
- `patch_invalidate_head()` now logs the no-op ("already clear") case too, so the
  diagnostic log always shows the head decision.
- Mechanism and addresses: `docs/SD_SINGLE_FAMILY_240P.md`.
- Host tests: 44/44 (`python -B tools/test_app.py`), incl. an NTSC head-1 case.

## Build

```text
python -B tools/build_ps3_app.py --sd-timing-fix
```

Output: `CRT-Toolset-1.6.0-dev7.pkg`, APP_VER 01.76, in-app `1.6.0-dev7`.

## Test (NTSC + component / single-SD console — issue #1)

1. AV Multi Out, start at 480i (NTSC). Confirm XMB shows 480i.
2. Launch CRT Toolset v1.6.0-dev7.
3. Select **Test 240p**.
   - dev6: "active analog timing did not match the candidate" — reverts.
   - dev7 expected: the 240p test picture appears with a 15-second countdown.
4. Press Cross within 15 s to keep, or wait for the automatic revert.

If it still cannot activate, the flow reverts to stock cleanly (no reboot).
Pull `/dev_hdd0/tmp/ps3_240p.log` for `Single-SD fallback: invalidate head=...`
and the `AV head ... cache invalidated` / `videoConfigure` lines.

## Validation

- PROVEN: head selection now follows the live AV cache; the NTSC head-1 case is
  covered by a host test. Primary (flip) path unchanged; owner's PAL console
  unaffected.
- UNKNOWN until a live issue-#1 console test: whether the repeated same-mode
  request reaches the AV mode setter so the invalidated flag triggers the
  reprogram. Reversible: a failed activation reverts to stock.
