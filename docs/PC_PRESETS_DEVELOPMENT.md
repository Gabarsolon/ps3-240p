# Native HDMI PC presets: development checkpoint

2026-11-14. Experimental `1.6.0-dev1`, isolated build outputs. Stable v1.4.3,
SD PID RC and 540p release artifacts are not rebuilt by this variant.
The owner-console result is scoped to Evilnat 4.93 CEX + Cobra / HDMI.

## Output evidence

**PROVEN (helper logs, real OBS frames and capture-card reports):**

| Target | Rendering source | Result and independent report |
|---|---|---|
| 1280 x 768 / 5:3 | 1280 x 720 | Earlier 15-second WXGA result; see [original probe](DISPLAY_PRESET_PROBE.md). |
| 1280 x 1024 / 5:4 | 1280 x 720 | Visible XMB, cached receiver 1280 x 1024, 15.071617-second active hold and exact stock recovery. |
| 1920 x 1200 / 16:10 | 1920 x 1080 | Visible XMB and 15.069267-second undisturbed hold; separate live size/scan diagnostic reports 1920 x 1200, raw scan 0x07, and exact stock recovery. |

Evidence stays ignored under `local_only/display_presets/automatic/`:
SXGA run `20261114-190745`, sequence `685197177`, including inspected
`sxga-032-state3.jpg`; WUXGA undisturbed run `20261114-191038`, sequence
`1736710052`, including inspected `wuxga-023-state3.jpg`; live WUXGA run
`20261114-191322`, sequence `3566327927`, hold 15.070017 seconds.
All three later requests ended DONE/detail 0 and recovered 1080p.

**PROVEN (receiver discrepancy):** during WUXGA, cached size bytes still
reported 1920 x 1080 while the live dimension words reported 1920 x 1200.
The first automatic run correctly failed its cached-dimension criterion;
it was not relabeled as passing. The separate live diagnostic resolves that
specific discrepancy. Firmware mapping remains unvalidated and physical
refresh is **UNKNOWN**. Native profiles request nominal 60 Hz.

**UNKNOWN:** calibrated aspect/geometry, pixel-perfect scaling, games,
permanent enforcement, other consoles/firmwares, component/RGB and HEN.
The images show borders. These profiles convert existing 720p/1080p sources;
they do not make games render natively at arbitrary resolutions. No preset
in this set is 1024 x 768 / 4:3.

## Native selection and guards

**PROVEN (retail binary and live traces):** monitor types 11/12/13 map to
cumulative capability masks at VSH `0x00470e24`. Forcing the cumulative
SXGA mask also advertises WXGA, and the native mapper selected WXGA.
The probe instead selects the existing individual capability bit:
`0x40000000`, `0x20000000`, or `0x10000000`. Each native provider's original
mask is saved independently and restored exactly.

Native device-info then advertises 720-line sources with conversion 1/2,
or 1080-line sources with conversion 3. The existing native mapper returns
logical PC category `0x10`, the same 60-Hz-family flag field `0x00040000`,
and source mode 11 or 12 respectively. That flag field is not the conversion
number. A mismatched mapping is refused before display submission.
Sources: private matching VSH mapper/dispatch, live logs above, and
`experiments/display_probe/native.c`.

**PROVEN (implementation and host checks):** protocol v2 uses magic
`0x44535032` and `crt_display_probe.v2.ready`. Value 1/2/3 selects the preset.
Each preset requires its own capability-only control, plus the stock 720p
control, in the same resident session. The helper retains console-side
15-second holds, bounded restoration, cancellation, external-state guards
and a latched recovery failure. The helper does not patch timing RAM or directly access MMIO/flash, and it
does not change the saved display default. Native firmware calls perform
the normal live mode change.

## App menu implementation

**PROVEN (source/build):** the isolated development package adds
**HDMI PC resolution presets** to CRT Toolset. Each test is one selection.
It requires HDMI stock 1080p, paused SD enforcement, a matching helper and
an inspected free or matching resident slot. It does not evict other plugins.
A queued request is acknowledged before normal app exit; the helper waits
for the app flag to clear and stable XMB state within a bounded deadline.
It runs missing controls, tests the selected preset, and restores 1080p.
No confirmation can keep the preset permanently in this build.

**PROVEN (handoff regression):** a fixed three-second delay encountered a
still-preparing XMB framebuffer, and the initial menu test refused without
mutating display state. The corrected service retries only non-mutating
preflight while that queue deadline remains; unknown identities or other
guard failures still refuse. Source and host regression:
`service.c`, `native.c`, `tests/test_display_probe.c`.

**PROVEN (second handoff regression):** a later app-to-XMB trial briefly
reported enabled output with the app's 7680-byte pitch. The capability
control then timed out and latched a mismatch against that unsuitable
snapshot. Independent readback subsequently verified stock 1080p, the exact
original masks and monitor state before a normal soft reboot cleared the
latched helper. Evidence: ignored `menu-20261114-192515/final-helper.log`
and the following bounded VSH readback.
The corrected preflight also requires this fingerprinted VSH backend's
format 5 / pitch 8192 before taking ownership; an app pitch cannot become
the recovery baseline. Host tests reject that transient with zero writes.

**PROVEN (corrected app-menu runs):** one Cross selection in each pictured
submenu queued its test, exited normally, and completed DONE/detail 0.
Fresh-session SXGA ran all missing controls plus a 15.070679-second target
hold; receiver dimensions were 1280 x 1024. The next WUXGA selection ran
its missing capability control plus a 15.071251-second target hold; live
receiver dimensions were 1920 x 1200 with scan byte 0x07. Each returned a
visible XMB and stock receiver 1920 x 1080. The final helper was
**50256 bytes**, SHA-256
`c0d83bbc6636a91edd08720da7842d6e0bbe9328ff77744829a8bcd6584b283f`.

Evidence: ignored `local_only/display_presets/menu-20261114-193024/`
(sequence `168340671`, inspected `frame-161-state3-kind4.jpg`) and
`menu-20261114-193257/` (sequence `321887995`, inspected
`frame-086-state3-kind4.jpg`). Both contain real captures, helper logs,
per-phase receiver reports and recovery images. The WUXGA live query is a
separate diagnostic from its earlier undisturbed proof.

**PROVEN (clean-boot bootstrap and WXGA menu):** after a checked stock
reboot, slots 2-6 were empty. The app itself loaded the packaged helper
from `/dev_hdd0/game/PS240P001/USRDIR/crt_display_probe.sprx` into slot 2.
One WXGA menu selection then ran its controls, held 1280 x 768 for
15.069528 seconds and completed DONE/detail 0 with visible 1080p recovery.
Evidence: ignored `fresh-loader/{app.log,slots.json}` and
`menu-20261114-193709/`, sequence `94831767`. All three preset choices
have now been exercised through the actual menu with the final helper.

**PROVEN (installation):** webMAN's keep-PKG command installed all six
exact package members after backups. The installed development PKG is
**307936 bytes**, SHA-256
`2fd59271350d19cdf979ff73a8f42dfeaf3fc06ec0311d3f52763e99f8dc3c3c`.
Evidence: `local_only/display_presets/install-20261114-192852/installed.json`.
It retains title ID `PS240P001`, replacing the installed app; source build
outputs and public stable/HD release assets remain separate.

**PROVEN (observer resilience):** the earlier menu-triggered WXGA request
completed and restored stock despite the PC losing an FTP data connection.
That interrupted capture is not a full undisturbed menu-picture proof.
The observer now reconnects once for read-only transfers; STOR/rename and
uncertain commands are never retried automatically. Windows atomic JPEG
replacement also has a bounded read retry. Sources: `tools/display_probe_test.py`,
`tools/hd540_auto_test.py`; offline regression tests in their matching test files.

```powershell
python tools/build_plugin.py --display-probe
python tools/build_plugin.py --pid-compatibility
python tools/build_ps3_app.py --display-presets
python tools/display_probe_test.py --action all --profile sxga
python tools/display_probe_test.py --action caps --profile wuxga
python tools/display_probe_test.py --action wuxga
python tools/display_probe_test.py --action wuxga --timing-candidate
```

The last command is a separate, possibly capture-disrupting live-dimension
query, not part of undisturbed picture proof. No helper is installed or
loaded by the runner. See [AGENTS.md](../AGENTS.md) for deployment guards.
