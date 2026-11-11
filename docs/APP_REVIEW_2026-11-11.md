# Native app review and revised test build

## v1.1.4: PS-menu presentation and callback handling

**CLAIMED (owner observation):** on the installed diagnostics build v1.1.3, pressing PS freezes the OS menu before any choice can be made. **PROVEN (retrieved PARAM.SFO/log):** version 01.13 is installed; startup and initial frames complete at stock 576i/SCART, the heartbeat later reports `system_menu=1`, and the log eventually records a clean exit without a GPU timeout. That log cannot establish a working overlay or identify which event led to exit, since raw callback statuses were not logged. The freeze is therefore not explained solely by the now-disabled timing experiment.

**PROVEN source difference:** the app returned without flipping when `system_menu` was set. Local PS3DK sample `samples/gcm/hello-ppu-cellgcm-sysmenu/source/main.c` freezes its scene but continues drawing/flipping throughout the OS menu. v1.1.4 adopts that presentation policy using the last software frame. It also handles DRAWING_BEGIN/END, suppresses pad polling and timing inspection while OS UI is active, requires button release after resume, refreshes output state only once the OS has finished drawing, and avoids treating OS-paused flip/fence work as a two-second GPU failure. An explicit EXIT_GAME callback immediately interrupts those waits. Event status/parameter records are queued in the callback and written to the log after dispatch, so no disk I/O occurs inside the callback itself. Normal OS exit no longer logs the misleading generic “Menu presentation failed” text.

Fifteen entry-point host scenarios pass. The six new cases verify continuous frozen-scene presentation during menu/drawing ownership, resume without held Circle becoming an app quit, explicit Quit during normal/suspended flips, and three-second OS GPU suspensions without a spurious timeout. Tests use the native event values, not invented mock IDs. Native compilation and package acceptance pass. **UNKNOWN:** whether this change fixes the physical console overlay; it must not be reported as console-verified before the owner's result. Timing tests remain disabled and the transparent icon is retained.

## v1.1.3: production timing transitions disabled after console freeze

**CLAIMED (owner observation):** the display remained 60 Hz / 480i after the PAL selection and visibility prompt. Circle led to black; invoking the OS's PS-button exit menu froze the whole console. These are actual adverse test results, despite the log's successful memory/API responses. No hardware or flash damage is established, and the cause of the freeze is **UNKNOWN**.

v1.1.3 keeps the established menu and supplied icon with its original transparency, but blocks `switch_begin` and `switch_stock` in production before any availability query, snapshot, write or mode request. The UI explains that timing tests are unavailable and directs stock recovery to a console reboot. Pure host fixtures explicitly define `TIMING_OFFLINE_TEST` to retain transaction/rollback test coverage; this is not enabled in the native package. A ninth entry-point scenario selects the disabled PAL item, closes its notice and exits, verifying only the normal startup video configuration occurred and no timing inspection was started. The physical activation mechanism must be reworked offline before any more console experiment.

The earlier v1.1.2 instructions below are historical and are superseded by this restriction.

## Owner result after reboot: menu works, 288p experiment fails

**CLAIMED (owner observation):** v1.1.2's menu appeared after the reboot. Selecting PAL 288p showed 480i; pressing Circle led to black. **PROVEN (retrieved run log):** both initial buffers rendered non-background pixels and completed their flips at stock 720x576 interlaced on SCART port 0x85. The PAL test performed 576 -> 480, verified 15 candidate stores, then requested public resolution 576. Circle restored 15 words, issued 480 -> 576, and returned API status zero. Each transaction rechecked all 42 target words. Therefore RAM rollback completed according to the selected peek/poke backend, but physical output restoration was not established. The message `Automatic/cancel recovery=0` is an API/RAM outcome, not proof of a usable waveform; the owner's black screen contradicts interpreting it as full recovery.

**UNKNOWN:** why the physical output stayed at 480i or lost sync while standard API readback reported 576. The alternate-SD/request approach has not produced validated 288p. Do not repeat that hardware test as though activation or signal recovery has been fixed. A different activation strategy needs offline investigation before another console test.

As an independent executable check, the v1.1.2 EBOOT downloaded from the installed app matches its package manifest. RPCS3 decrypted it successfully, and all four loadable ELF segments matched the compiled artifact. The same installed EBOOT ran under headless RPCS3 with two completed flips and repeated menu heartbeats. Null rendering does not verify physical pixels or analog sync. The old v1.1.1 console log predating the reboot is not evidence that v1.1.2 was damaged.

The supplied source icon already contains a transparent alpha channel. The initial asset import flattened it onto opaque black. The corrected importer retains alpha on a transparent RGBA canvas; the package update changes the icon while preserving the exact working compiled ELF and current SFO version 01.12. Native output remains unverified.

## v1.1.2 first-frame investigation and supplied icon

**CLAIMED (owner observation):** v1.1.1 shows black immediately on launch. **PROVEN (retrieved v1.1.1 log):** stock 720x576 interlaced, SCART port 0x85, videoConfigure success and successful read-only resolution of 42 stock targets. About 78 seconds later the app logs menu presentation failure and exit, with no BEGIN experiment or timing transaction. This is a presentation failure before a requested mode test; it does not demonstrate a failed 288p transition.

**INFERRED hypothesis:** retaining an unchanged screen after a single initial buffer-0 flip may contribute to black first presentation. v1.1.2 submits and completes both initial buffers before accepting input or idling. It logs each initial software-render sample and flip completion and a periodic menu heartbeat. LV1 inspection is deferred until the owner selects it or starts a test, so first presentation contains no hypervisor reads. The cause and physical effect of this change are not yet proven. Eight real-entry-point host scenarios now require the initial 0/1 buffer pair, verify inspection is deferred, and exercise flip timeout as well as video-state cases.

The owner's supplied CRT/240p PNG is retained unchanged at `app/assets/icon-source.png` and fitted without stretching into the package's 320x176 `ICON0.PNG`. SFO version 01.12 / displayed version 1.1.2. FTP transfer commands use PASV after EPSV/active data retrieval attempts failed; PASV successfully retrieved the diagnostic log.

## v1.1.1 startup correction

**PROVEN (owner-run v1.1 log, retrieved read-only):** startup logged `Video state rc=00000000 state=0` and exited before menu presentation or LV1 inspection. The app compared the native return value to the legacy `VIDEO_STATE_ENABLED` value 1. Local PS3DK `cell/cell_video_out.h` declares native `CELL_VIDEO_OUT_OUTPUT_STATE_ENABLED=0`, disabled=1, preparing=2; its `sysutil/video.h` wrapper forwards the native state unchanged. The legacy constants are therefore unsuitable for these checks. This was an introduced application bug, not evidence of unsupported SCART output.

v1.1.1 uses the native constant in both the initial query and configuration completion check. More startup failures now include a log message. Seven host scenarios compile and execute the real application entry point: native enabled state 0 with the observed 576i mode reaches one presented menu frame and clean exit; preparing-to-enabled also reaches the menu; disabled output, stuck preparing/disabled states, API query failure and configuration failure stop with the expected bounded/logged behavior. The corrected package's SFO version is 01.11. Successful launch on the console is still awaiting owner verification; no native low-line result is claimed.

## Actual status

**CLAIMED (owner observation):** the previous package left the SCART RGB screen at 576i. The previous app's exact status message is unavailable. **UNKNOWN:** native 240p/288p output on this console. No hardware measurements or successful low-line output have been established in this iteration. The earlier feasibility/prototype documents' claims of proven encoder behavior and hardware success are superseded by this status.

**PROVEN (local build and host tests):** v1.1 compiles with PS3DK 0.26.0, uses the Buzz Bridge software renderer, checks every video configuration result, resolves its RAM targets through protection pages, and verifies memory transactions. This is an experimental test package.

## Findings

| Finding | Evidence | Consequence / change |
|---|---|---|
| Old UI differed from Buzz Bridge | Previous `app/source/main.c` used dbgfont; reference `buzz-pad-bridge/ps3/plugin/autoload/ui_layout.c` uses a software font/layout | Reuse the reference renderer, colors, overscan margins, scaling, interlace filter, selection and wrapping; change app title/menu text only |
| Basic legacy LV1 syscalls were valid | [webMAN peek/poke source](https://github.com/aldostools/webMAN-MOD/blob/master/include/ps3mapi/peek_poke.h): CFW 8/9; PS3MAPI 8 with 0x7777 and 0x1008/0x1009 | Do not assume syscall-number failure. Probe the PS3MAPI backend first, then the legacy backend read-only; select a backend only after identity/data validation, and never switch backend during writes |
| PAL profile 4 contained three wrong stock raster fields | Private 4.93 `sysmgr_ss.elf`, VA 0x80038c08, words 3, 9, 10; stock module SHA-256 `07c0bd2a2403f65b6d741e11a15106114365cb78165833bac726476f671b64c9` | Correct numerical values in native payloads and legacy offline reference; preserve stock horizontal fields in the candidate |
| Only internal profiles 1 and 4 were patched | Previous native app and `timing_payloads.h`; profile aliases documented in `LV1_AV_TRACE.md` | Cover NTSC profiles 1/2 and PAL profiles 3/4, preserving each profile's differing horizontal fields/output routing. This addresses the NTSC alias omission; it does not prove PAL selected a different profile |
| Physical addresses were hardcoded | Previous native `lv1_patch.h`; dynamic mapping documented in `LV1_SYSMGR_SS_LOADADDR.md` | Resolve PID 10, its protection domain and each target's mapped page anew; validate domain identity, list termination, bounds, ambiguity, aliases and all 42 stock/candidate words before writing |
| Old native writer skipped preflight and rollback | Previous `lv1_apply_override`/`lv1_restore_stock` | Check the complete target set before the first store, verify each store and the final set, undo completed stores on failure, reject unknown data |
| Old app ignored videoConfigure results and requested the same active profile | Previous Cross/Square handlers; profile cache at private `sysmgr_ss.elf` VA 0x800005e4 | Use an exposed alternate **SD** family before applying the selected candidate; check requests, enabled state and matching public resolution ID. Never request 480p/720p for the SCART experiment |
| Old app called table writes “ACTIVE” without measuring output | Previous status messages | Report candidate RAM state separately from firmware-reported output; never label a verified 240p/288p waveform |

The candidate uses 262/312 total lines. **INFERRED arithmetic:** at the retained nominal horizontal rates, this implies approximately 60.054/50.080 Hz, not exactly 59.94/50.000 Hz. The menu's 60/50 Hz text denotes the standard family. Exact raster/encoder acceptance is **UNKNOWN**, including register 0x37's electrical meaning; differences between stock encoder templates are not a programming specification.

## Remaining activation gap

**PROVEN API limitation:** local PS3DK `sysutil/video.h` exposes standard resolution IDs and framebuffer pitch/aspect; `videoConfiguration` has no arbitrary active-height or scan-mode field. **PROVEN static check:** the AV manager validator at VA 0x80025fd0 bounds request height by metadata height (`LV1_AV_TRACE.md`). **INFERRED risk:** a stock 480/576-line request may therefore be rejected after metadata becomes 240/288 lines. A successful `videoConfigure`/resolution readback is also insufficient to prove that physical analog timing changed.

The new log records the return codes and firmware state so an owner-run experiment can establish which stage fails. This build does not bypass that validator, directly poke RSX MMIO, inject an AV-manager code patch, or claim that the remaining activation gap has been solved. If only PAL SD is exposed by firmware, it stops before changing memory because no supported alternate SD transition is available.

## Verification performed

- `python -B tools/test_app.py`: noncontiguous physical pages, address/identity/list validation, both aliases, unknown-byte refusal, mixed-state restoration, failure injection at every changed PAL word with exact rollback, video-request failure/recovery, and menus at SD/HD in both aspects.
- Optional private-input audit: all 42 planned stock words match the local 4.93 module. No firmware file or instruction listing is copied into the public tree.
- PS3DK C build with `-Wall -Wextra -Werror`; package member decryption/acceptance and hashes.
- Host-rendered PAL menu preview at `build/tests/pal-menu.png` (generated from `pal-menu.ppm`). This is a rendering preview, not a console screenshot.

## Historical v1.1.2 test (retired; do not repeat)

1. Install `deliverables/PS3-240P.pkg` (SFO version 01.12, on-screen v1.1.2). Reboot first if any earlier timing candidate was kept. The observed v1.1/v1.1.1 failures occurred before RAM edits, so those failed launches themselves do not require a reboot.
2. Start in the normal 576i SCART RGB configuration. Select **Test PAL 288p (50 Hz)** with Cross.
3. If a picture appears, verify the display/scaler's actual scan type. Cross keeps the candidate; Circle or 15 seconds without confirmation restores the pre-test RAM/settings through checked requests. Recovery is an attempt, not a guarantee against console/firmware stalls.
4. If it stays 576i, choose restore. If a mode request fails, the app reports it and tries recovery automatically. Do not repeat the test before collecting the log.
5. Retrieve `/dev_hdd0/tmp/ps3_240p.log` before launching the app again (startup overwrites it). A read-only command is `curl.exe --fail --max-time 15 -o build/ps3_240p-console.log ftp://192.168.0.169/dev_hdd0/tmp/ps3_240p.log`.

Legacy `apply_240p_patch.ps1` live writes are retired: it cannot resolve current pages or transactionally restore all profiles. Its offline `-UnitTest`/`-WhatIf` reference remains available. App launches, installation and console power operations were performed by the owner; authorized package FTP transfers and read-only diagnostic retrievals were performed by the assistant.
