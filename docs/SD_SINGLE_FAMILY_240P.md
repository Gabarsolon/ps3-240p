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
  (`0xc000d6f0`), head 1 = low word (`0xc000d6f4`).
- The app already resolves this word (`lv1_patch.c` `video_address[26]`).

Zeroing the **active** head's flag forces a full reprogram on the **next request
of the same mode**, with no family change. Evidence: cross-checked against the
independently decompiled setter and the existing `VIDEO_CONTEXT_POINTER` /
`VIDEO_CONTEXT_HOLDER` constants (identical derivation).

### Which head (corrected in dev7)

Region does **not** fix the head/family map. On the owner's PAL console 576i runs
on head 1; GitHub issue #1 (NTSC console, 480i over component) drives 480i on
**head 1** too, with head 0 idle (`state=0`). dev6 picked the head from the SD
target (`target==5 ? 1 : 0`), so for a 480i target it cleared idle head 0 and
left live head 1 unchanged — the candidate never activated. dev7 reads the head
that is actually programmed (AV cache `state==2`, `patch_active_head`) and clears
that one; the target-derived default is a fallback only when 0 or 2 heads are
active.

### dev7 live result and why the cache lever fails (corrected in dev8)

The dev7 live log (issue #1, NTSC, component) proved dev7 selected the right head
(head 1, flag `state 2 -> 0`) but the patched timing still never activated:

```text
Single-SD fallback: invalidate head=1 (target=4 default=0)
AV head 1 profile-index cache invalidated (0000000000000002->0000000000000000)
videoConfigure id=4 result=00000000
Post-switch analog AV cache match=0 ... Head1=480/525   (still stock 480i)
```

- **PROVEN (issue #1 dev7 log):** `videoConfigure(id=4)` while the console is
  **already id=4** returns success WITHOUT calling the mode-setter `0x800005e4`,
  so the zeroed profile-index flag is never consulted and the patched raster
  (table RAM only) is never loaded into the live head. The 576i bounce is refused
  (`videoConfigure id=5 result=8002b226`). So on a single-SD console neither lever
  (family flip, head-flag invalidation) reaches the mode-setter. The "state" field
  in the AV CACHE log is that profile-index flag (2 = valid, 0 = clear).

### dev8: force a real mode change (HD bounce)

To reach the mode-setter, the SD target must be re-entered from a **different**
resolution id. 480p is not usable (same id 4 as 480i, differing only by
`scanMode`, which `configure()` does not set), so the bounce uses an available HD
mode: **720p (id 2), else 1080i (id 1)**. `480i -> 720p -> 480i` is a genuine id
change; the mode-setter runs and loads the patched table. The HD bounce is briefly
non-15 kHz (black on a 15 kHz CRT) but transient. UNKNOWN until live test on the
issue #1 console: whether 720p is configurable over its component cable and
whether the return actually loads the patched raster. Fails safe to stock.

### dev10: in-place reprogram by format reassert (PROVEN, owner PAL console)

dev8's HD bounce cannot help the issue #1 console: its dev8 log shows 720p and
1080i unavailable (`Resolution availability id=2/1 result=00000000`), so 480i is
its only mode. What works instead is re-requesting the **same** SD mode with a
non-timing field changed: the request differs from the current configuration, so
it is not dropped as a same-mode no-op and reaches the mode-setter, which (with
the head flag cleared) does a full reprogram from the patched table.

- **PROVEN (dev9b, owner PAL console, single-SD path forced, started at 576i):**
  aspect 16:9 at 576i is refused (`8002b226`), so the aspect variant is a no-op.
  The buffer-format variant (`videoConfigure id=5 format=1` then `format=0`)
  reached the mode-setter: head 1 flag back to valid, raster from stock
  `574/625 scan_control 00030000` to candidate `576/624 scan_control 00010000`,
  `Post-switch analog AV cache match=1`, ~0.46 s from BEGIN, no resolution change.
- dev10 makes this the primary path on all consoles (format, then pitch +1024),
  with the old SD flip and then the HD bounce as fallbacks. UNKNOWN until tested
  on an NTSC single-SD console at 480i.

## Implementation (dev10 supersedes the dev8 text below where they differ)

- `reprogram()` (`app/source/switch_flow.c`): clear the programmed head's flag,
  call `reassert` (`app/source/main.c`: format XBGR then pitch +1024, each
  restored; returns 0 only when the head flag is valid again = mode-setter ran).
- `switch_begin_target`: `patch_apply` → `reprogram(target)`; if the candidate is
  not live, flip via the other SD family, else HD bounce.
- `switch_revert`: `patch_undo` → `reprogram(original)`; if that fails or a
  candidate is still live, flip / HD bounce; then `configure(original)`.
- Debug flag `/dev_hdd0/tmp/240p_force_single_sd.on` treats the other SD family
  as refused, to exercise the single-SD path on a dual-SD console.

## Implementation (dev8)

- `switch_begin_target` (`app/source/switch_flow.c`): keep the proven flip as
  the primary path (no regression on consoles that can configure the other SD
  family); when `configure(alternate)` fails, bounce via an available HD mode:
  `configure(720p|1080i)` → `patch_apply` → `configure(target)`. If no HD mode is
  available, last-resort `patch_invalidate_head(patch_active_head())` (dev7
  behaviour; ineffective per the log, but harmless).
- `switch_revert`: prefer the other SD family; if refused, HD-bounce before
  returning to the original mode, so recovery reprograms stock timing. Only a
  failure to reach the original mode is fatal.
- `patch_invalidate_head(patch_context *, int head)` (`app/source/lv1_patch.c`):
  zero the selected head's 32-bit half of `video_address[26]`, readback-checked
  (now last-resort only).
- `patch_active_head` (dev7): the single head with AV cache `state==2`, else -1.

Routing by actual configurability (does `configure(alternate)` succeed) rather
than `cellVideoOutGetResolutionAvailability`, which reports only the region SD
standard and is unreliable for this decision.

## Status

- Host tests: 44/44 (`python -B tools/test_app.py`), incl. an HD-bounce case and
  a no-HD last-resort NTSC head-1 case (dev8).
- dev6 live (issue #1, NTSC/component): reverted cleanly but never activated —
  cleared idle head 0 while live head 1 kept stock timing.
- dev7 live (issue #1): selected the right head (head 1) but still never
  activated — a same-mode re-request never reaches the mode-setter, so clearing
  the flag does nothing (`Post-switch ... match=0`, head still stock 480/525).
- dev8: bounces through an HD mode (720p/1080i) to force a real mode change.
  UNKNOWN until live test on the issue #1 console: whether 720p is configurable
  over that component cable and whether the return loads the patched raster.
  Fails safe to stock. Primary (flip) path verified on the owner's PAL console
  (no regression).
