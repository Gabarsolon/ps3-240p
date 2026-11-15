# Single-SD-family 240p/288p via AV head cache invalidation

Problem: the SD timing test leaves the target SD profile and returns to it so
the AV manager (`sysmgr_ss`, LV1) reprograms the RSX CRTC from the patched
timing table. The return used the **other** SD family (480i↔576i). A
region-locked console exposes and can configure only its own SD standard
(NTSC → 480i, PAL → 576i), so `configure(alternate)` fails and the test — and
its revert — fail with "display recovery failed."

## Mechanism (PROVEN, 4.93 `sysmgr_ss`)

The mode setter at `0x800005e4` keeps a per-head cached profile index and skips
the RSX CRTC reprogram when the requested index equals the cached one. The
raster reprogram (`0x80003bcc` / `0x80003a24` / `0x80003b1c`) runs only when the
requested index differs from the cache, and the per-head **valid** flag gates a
full program when zero.

- AV state object: pointer at `0xc000b440` (TOC `0xc0012f40` − 0x7b00) →
  static instance at `0xc000d4c8` (service segment, memsz `0xdd58`).
- Per-head profile-index valid flag at `state + 0x228`: head 0 = high word
  (`0xc000d6f0`), head 1 = low word (`0xc000d6f4`). Head 0 drives 480i analog,
  head 1 drives 576i.
- The app already resolves this word (`lv1_patch.c` `video_address[26]`).

Zeroing the target head's flag forces a full reprogram on the **next request of
the same mode**, with no family change. Evidence: cross-checked against the
independently decompiled setter and the existing `VIDEO_CONTEXT_POINTER` /
`VIDEO_CONTEXT_HOLDER` constants (identical derivation).

## Implementation

- `patch_invalidate_head(patch_context *, int head)` (`app/source/lv1_patch.c`):
  zero the selected head's 32-bit half of `video_address[26]`, readback-checked.
- `switch_begin_target` (`app/source/switch_flow.c`): keep the proven flip as
  the primary path (no regression on consoles that can configure the other SD
  family); when `configure(alternate)` fails, fall back to
  `patch_apply` → `patch_invalidate_head(head)` → `configure(target)`.
- `switch_revert`: invalidate the original head before returning to the original
  mode, so recovery reprograms stock timing even when the other family cannot
  be configured. Only a failure to reach the original mode is fatal.

Routing by actual configurability (does `configure(alternate)` succeed) rather
than `cellVideoOutGetResolutionAvailability`, which reports only the region SD
standard and is unreliable for this decision.

## Status

- Host tests: 44/44 (`python -B tools/test_app.py`), incl. a fallback case.
- UNKNOWN until a live NTSC/single-SD console test: whether a repeat of the
  current mode reaches `0x800005e4` so the invalidated flag triggers the
  reprogram. If it does not, `begin_picture`'s candidate-active check fails and
  the flow reverts cleanly. Primary (flip) path verified on the owner's PAL
  console (no regression).
