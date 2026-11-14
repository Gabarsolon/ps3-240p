# v1.5.0-dev3: stock transition check and bounded recovery

Follow-up: [dev4 selects the audited progressive native mode](HD540_DEV4.md)
after identifying the mapper's interlaced conversion result. Dev3's live
results below remain the evidence for this failure path.

Checkpoint: 2026-11-14. **PROVEN (dev2 logs):** the first timed request
entered state 1 (leaving 1080i), timed out, then alternated between recovery
states 5 and 9. It never reached state 2 or 3, and the patch-apply operation
is only reachable after the initial alternate-mode check succeeds.
**INFERRED:** the 540p candidate was never applied in this attempt.
Evidence: ignored `local_only/hd540/shared-preview/recovery-failure/`;
[prior service flow](HD540_DEV2.md).

**CLAIMED (owner):** the picture looked cropped, XMB initially looked fine,
then the screen became black. The owner rebooted and returned to XMB.
**PROVEN (app log):** while the app was still running, its output readback
changed from 1920 x 1080 interlaced to 1280 x 720 with the interlaced scan
flag still reported. A GPU flip timeout followed and the app exited.
**UNKNOWN:** which native/readback field prevented the alternate-mode check;
dev2 did not log enough detail to distinguish the conditions.

**PROVEN (source review):** recovery unnecessarily required the same
alternate-mode check that had just failed, and restarted that path forever.
That was a bug in this prototype's recovery logic. A successful read-only
inspection and host tests had not established real transition/recovery
behavior. The dev2 build should not be used for further timing tests.

## Dev3 behavior

**PROVEN (implementation):** test requests now acknowledge a queued action
and the app exits normally before any display change. The helper waits for
the app-active marker to disappear and the native XMB surface to remain
stable for 1.5 seconds, then performs a fresh preflight and snapshot. The
saved surface therefore belongs to XMB rather than the app's framebuffer.
Sources: [app](../app/source/main.c), [backend](../plugin/vsh_hd540.c),
[service](../plugin/hd540_service.c).

**PROVEN (implementation):** the new **Test stock 720p / 1080i round trip**
action uses the normal modes without calling the candidate patch operation.
The candidate action is refused until a stock round trip has passed in the
current helper session. A failed transition clears that prerequisite.
Sources: [protocol revision 2](../app/source/hd540_protocol.h), service above.

**PROVEN (recovery implementation):** after restoring any changed timing
RAM, the helper directly requests the saved original mode. If that request
cannot be verified, it allows one bounded alternate-mode pulse and one
final original-mode request. The final return never depends on alternate
readiness. After those attempts it latches failure and issues no further
display requests. New cancellation messages cannot reset that budget.
Failed RAM restoration can retry without sending additional video commands.
Source: [service state machine](../plugin/hd540_service.c).

**PROVEN (diagnostics):** dev3 logs native mode/flags, resolution/scan,
pitch and actual alternate head-cache dimensions/line totals to identify
the failing guard. A new protocol magic/readiness marker prevents an old
resident helper from accepting this test flow.
Sources: [backend](../plugin/vsh_hd540.c), [protocol](../app/source/hd540_protocol.h).

**PROVEN (host checks):** tests cover no display request before app exit,
fresh XMB snapshots, no candidate application during the stock test,
candidate prerequisites, app-exit timeout, direct return after failed
alternate readiness, RAM-only recovery retry and zero further display
requests after latched failure despite 1000 cancellation messages.
All 24 real-app startup scenarios and the SD/gamma/HD suites pass.
Sources: [service tests](../tests/test_hd540_service.c),
[native backend tests](../tests/test_vsh_hd540.c),
[test runner](../tools/test_app.py).

**PROVEN (build/install):** the new helper passes the compiled loader/PRX
checks; the package's members are verified after decryption. Dev3 is SFO
01.52, package `build/hd540/deliverables/CRT-Toolset-1.5.0-dev3.pkg`,
301120 bytes, SHA-256
`2d907d6a4a4ebb4fc0d1982304eef02ecc612e003f1b983a96e8bd52b1815205`.
The executable, SFO and matching staged/runtime helper were installed and
byte-verified after the owner rebooted and confirmed XMB. Prior files were
retained. No app launch or mode request was issued remotely.
Evidence: ignored `local_only/hd540/dev3/deployment.json` and build logs.

## Shared observation

**PROVEN (local capture):** OBS can own the physical USB3 Video device while
FFmpeg reads OBS Virtual Camera. The shared feed shows the app menu while
the owner keeps the OBS preview open. OBS's current virtual output is
2560 x 1440 at nominal 60 fps; this is a processed preview, not raw HDMI
timing or field-phase measurement. Evidence: ignored
`local_only/hd540/shared-preview/shared-screen.png` and capture log.
References: [OBS virtual camera guide](https://obsproject.com/kb/virtual-camera-guide),
[documented startup option](https://obsproject.com/kb/launch-parameters).

**PROVEN (recording limit):** the first 60-second recording contains dropped
frames from a too-small DirectShow input buffer. Subsequent recordings use
a 128 MB buffer. **UNKNOWN:** complete temporal fidelity through the card,
OBS and encoder; these captures establish visible behavior only.

## First live stock-only result

**CLAIMED (owner):** the full picture returned normally.
**PROVEN (helper/app logs):** the app published stock action 4, received
the queued-exit state, and exited normally before the helper changed modes.
The helper then captured fresh state and requested native mode 8 from
baseline mode 10. The requested surface became 1280 x 720, but the actual
head cache retained frontend VID 7 and 1080 active / 1125 total lines.
The native resolution/scan bytes were 2/1: the reported scan type also
remained interlaced. The alternate-raster guard correctly did not pass.

**PROVEN (recovery readback):** after that timeout, dev3 requested the
saved original mode directly and reached state 7 / detail 1: stock mode
restored with a test error. Native mode returned to 10, source resolution
to 1080 and XMB pitch to 8192. There was no unbounded display retry loop
and no candidate patch call. The stock round-trip prerequisite therefore
remains false; candidate requests remain locked.
Evidence: ignored `local_only/hd540/dev3/live/roundtrip-app.log`,
`roundtrip-helper.log`, `roundtrip-reply.bin` and
`local_only/hd540/shared-preview/dev3-stock-roundtrip.mkv`.

**INFERRED:** this native display path changed the rendering surface while
retaining the selected 1080i output raster. That distinction explains why
requesting public/native 720p geometry is insufficient to force the timing
profile change needed by the patcher. The app should not weaken the actual
head-cache guard merely because the requested surface dimensions changed.

**PROVEN (subsequent private static trace):** the native PAF queue reaches
VSH functions `0x37301c`, `0x372e08` and `0x435524`. The latter separates
rendering-mode translation from another selection derived from display
state at `0x6fd888`. Its return path can add high flag bits based on the
resulting configuration. The observed `0x01000000` high bit is therefore
not established as a force-output switch and is not a proposed patch.
Evidence: private 4.93 `vsh.elf`, functions/addresses above; decompilation
and disassembly remain in ignored `local_only/activation/`.
**UNKNOWN:** the correct native operation/selection needed to change the
physical output raster while preserving XMB rendering and return state.

**UNKNOWN:** correct 540p output, electrical field phase and HD CRT
compatibility. Stock restoration is now verified for this failure path;
that does not establish successful alternate-raster switching or candidate
recovery. The owner can resume ordinary HDMI 1080p while static investigation
continues; the helper is idle and SD enforcement remains disarmed.
