# v1.5.0-dev2: correct the HD menu startup

Checkpoint: 2026-11-14. **CLAIMED (owner):** dev1's picture was unusable.
**PROVEN (subsequent test):** dev2 later failed the initial stock-mode
transition and entered a recovery retry loop; use [dev3](HD540_DEV3.md) for
the corrected flow. Dev2 should not be used for further timing tests.
**PROVEN (capture):** the app displayed a mostly blank dark-blue frame with
corruption along the bottom. The app log showed 1920 x 1080 interlaced HDMI
and two completed flips, but no `videoConfigure` call. It contained no HD
command publication, and the HD request/reply files were absent.
Evidence: ignored `local_only/hd540/failure-20261114-003922/` and
`local_only/hd540/current-failure/screen.png`.

**PROVEN (dev1 source):** the experimental startup path deliberately skipped
`configure()` for resolution ID 1 to avoid accidentally choosing 1080p.
That also skipped the explicit packed 32-bit pixel-format and pitch setup
used by the renderer. Registering the GCM display buffers did not perform
that setup through the video configuration API.
Source: dev1 startup code and its build in
`local_only/hd540/dev1/`; [current corrected startup](../app/source/main.c).
**INFERRED:** this omission explains the broken menu before any 540p test.
The precise inherited hardware format was not captured by dev1's logs.

## Correction and checks

**PROVEN (dev2 source):** startup now explicitly configures XRGB32 and the
app's pitch for HD as well as SD. It logs the inherited configuration and
then calls `videoGetConfiguration` to require the requested resolution,
format and pitch before rendering. A mismatch stops startup before any
frame is submitted. Scan mode before and after initialization is logged;
the HD experiment still independently requires actual 1080i readback.
Source: [main.c, configure and startup](../app/source/main.c).

**PROVEN (host tests):** the real app test now models an incompatible
inherited HD surface and requires initialization before its first flip.
It also models a configuration update that fails to take effect and checks
that no frame is displayed. All 24 startup/menu scenarios pass, together
with the SD/gamma, HD transaction/recovery and native guard suites.
The modeled format/pitch are test inputs, not measurements of this console.
Sources: [startup tests](../tests/test_startup.c),
[runner](../tools/test_app.py).

**PROVEN (UI/source):** the HD submenu also has **Pause SD enforcement
(no video switch)**. The prior enable flag was still present on the console;
this provides a way to disarm it while using HDMI. It does not restore RAM
timing edits. The stock-record guards remain in place, so a reboot is still
needed if old timing patches are active.
Sources: [HD menu](../app/source/main.c), captured failure report above.

**PROVEN (artifact/readback):** dev2 is SFO version 01.51, package
`build/hd540/deliverables/CRT-Toolset-1.5.0-dev2.pkg`, 299744 bytes,
SHA-256 `05f3fc2b4236432ec938e1cfdb4367286501db991c8677fc8db80de34afd2e83`.
After the owner returned to XMB, its executable and SFO were installed and
read back byte-for-byte, with previous files retained. The unchanged matching
experimental helper was also checked on disk. No app launch, controller
input or timing test was issued remotely.
Evidence: ignored `local_only/hd540/dev2/deployment.json` and package manifest.

**CLAIMED (owner, subsequent check):** the dev2 menu is readable.
**PROVEN (live app readback):** the inherited surface used format 0 with
8192 bytes per row, whereas the app's packed 1920-pixel rows require 7680.
After configuration, the reported format remained 0 and pitch became 7680.
The before/after scan values both remained 0 (interlaced), and the app
reported 1920 x 1080 HDMI at the 59.94 Hz refresh flag. This establishes the
specific inherited-pitch mismatch and successful configuration readback.
Evidence: ignored `local_only/hd540/dev2/verified-menu-app.log`.
The capture device was subsequently unavailable to FFmpeg, which reported
that another application might be using it; no new menu screenshot was
obtained in that check. The exact cause of that device-access failure is
**UNKNOWN**.

**PROVEN (subsequent live preflight):** the owner paused SD enforcement and
ran inspection. The app loaded the matching helper into the inspected free
slot, published inspection action 1, and received status 0 / detail 0 with
the same sequence. The helper recorded the same successful inspection,
its HD readiness marker was present, and the SD enable flag was absent.
The inspection's implementation checks native HDMI 1080i, stock records,
actual head-cache identity and available transition mapping without a
display request or patch write. No timed-test command was present.
Evidence: ignored `local_only/hd540/preflight-20261114-004850/report.json`;
implementation: [native preflight](../plugin/vsh_hd540.c) and
[service inspection action](../plugin/hd540_service.c).

**UNKNOWN:** correct 540p waveform, capture acceptance of the candidate and
real recovery. The candidate timing
and helper recovery implementation are unchanged from
[dev1](HD540_DEV1.md). Verify the normal menu first, then inspect the HD
preflight result before attempting a timed test.
