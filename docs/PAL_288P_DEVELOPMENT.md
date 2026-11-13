# PAL 288p development checkpoint

**PROVEN (v1.2.2 payload source):** the old menu's PAL candidate actually used
a 240-line/260-line raster and nominal 60.10 Hz metadata with 576 source lines.
It was an earlier PAL-60 experiment, not the requested 288-line 50 Hz path.

**INFERRED (working NTSC method extended to PAL):** keep PAL's full 576-line
source metadata and source-field fetching; use an even 624-line frame
(312 lines per field), retain PAL horizontal timing, clear the same scan/encoder
interlace controls, and use the 290..293 field-sized packed window.
This requests 288 active lines per field at approximately 50 Hz.
Source: `app/source/timing_payloads.h:g_patch_288p_*`; comparison with
[the working NTSC recipe](GEOMETRY_STATUS_2026-11-13.md).

**PROVEN (source/host tests):** both PAL aliases retain their own horizontal
placement/output routing. The renderer uses the full 576-line framebuffer.
The helper reads the selected SD family and can preserve Mode 5 rather than
always correcting it to Mode 4. Timing tests and stock switches preserve a
snapshot and restore it on cancel or failed video requests.
Sources: `app/source/lv1_patch.c`, `app/source/switch_flow.c`,
`app/source/enforcer_mode.h`, `plugin/vsh_240p.c`,
`tests/test_patch.c`, `tests/test_raster.c`, `tests/test_sd.c`.

**UNKNOWN:** PAL picture geometry, PS-menu/game compatibility, and electrical
progressive field parity until hardware validation is recorded. Host timing
arithmetic and register readback do not measure the analog waveform.

## First hardware attempt and special PAL record

**CLAIMED (owner, 2026-11-13):** the first development build's Test PAL 288p
showed a correct picture but remained 576i.
**PROVEN (captured log):** all eight ordinary-table/encoder writes verified,
but the active analog cache remained 574 active / 625 total lines, with stock
scan control. Private evidence:
`local_only/activation/pal288-development/user-pal-attempt.log`.

**PROVEN (private 4.93 setup at VA 0x800005e4, and live pointer readback):**
analog head 1 / internal profile 4 selects special timing/metadata pointers
at VAs `0xc000af68` / `0xc000af70`, bypassing the ordinary profile arrays.
Their targets are `0x80038ad8` (raster) and `0x80038b10` (metadata).
The stock records use 574 source/active lines and 625 total lines, matching
the failed attempt exactly. Private capture:
`local_only/activation/pal288-development/special-path/pointers.json`.

**PROVEN (dev2 source):** ten additional data words now cover this special path
in the same preflight/transaction/rollback, after checking both pointer identities.
The complete plan has 54 targets.

**PROVEN (deployment observation):** the dev2 package uploaded and verified,
but the HTTP installer request timed out and subsequent HTTP/FTP reads also
timed out. **CLAIMED (owner):** the screen was black after being in the app.
**UNKNOWN:** whether this occurred during idle, the remote app-exit request,
or installation. Do not attribute it to an idle timer without reproducing it.
Collect the surviving app log and installed-file identity after recovery,
before relaunching the app. That installer attempt did not verify dev2 installation.

**PROVEN (post-reboot collection):** the surviving log records an OS exit
request, `gpu_ok=1`, and `pending_recovery=0` before process cleanup. The
installed SFO/EBOOT still belong to the first development build, version 01.30;
dev2 was not installed. Private evidence:
`local_only/activation/pal288-development/after-reboot/`.
This narrows the incident to the exit/update handoff; it does not prove the
exact failing cleanup or installer call. The next build adds cleanup-stage
log entries. An eight-minute idle/OS-exit host scenario also passes, which
does not exclude a hardware-only idle issue.

## Corrected build: successful PAL picture

**PROVEN (deployment):** while the console was at XMB, the three app files
were updated directly on HDD and read back byte-for-byte against the decoded
package. SFO version is 01.31, title **CRT Toolset**, menu **v1.3.0-dev2**.
This avoided the failed installer handoff. Package SHA-256:
`f3ff7d14839a51ccd2b3808116bb1a2c67adc0b8e4ab6e6f4b5ca6ecd6c4180a`.

**PROVEN (live readback):** the PAL test verified 12 stores and activated the
analog head's 576-active / 624-total cache with scan control `0x00010000`.
Aligned hardware reads independently showed:

| Address | Value | Interpretation |
|---|---|---|
| `0x28000682800` | `0x0000023f0000026f` | 576 source lines / 624 frame lines |
| `0x28000682808` | `0x0000024500000244` | marker 581 / sync start 580 |
| `0x28000682810` | `0x0000024a00000000` | sync end 586 |
| `0x28000682898` | `0x1000000010000000` | unity source step |
| `0x28000602600` | `0x0000110700000000` | source field-fetch bit retained |

Private evidence: `local_only/activation/pal288-development/dev2-direct/`,
`deployment.json`, `pal-active.log`, and `pal-active-registers.json`.

**CLAIMED (owner, 2026-11-13):** 288p works and the picture fits. The app log
also records that the owner kept the candidate.
**UNKNOWN:** electrical sync/field parity, PAL game/core coverage, and the
new helper/default-selection behavior until separately tested.


## Name refresh and default-save correction

**PROVEN (dev3 deployment):** after normal app exit to an already idle XMB,
webMAN's installer returned successfully. Installed SFO/EBOOT/helper matched
all decoded package bytes, and a fresh v1.3.0-dev3 log completed two frames
after launch through `/play.ps3?col=game&seg=PS240P001`.
Package version 01.32; SHA-256
`2ff55b45965539cb5d1bca132d4d27db55e9f417030d281607af365b029f7b97`.
Private evidence: `local_only/activation/pal288-development/dev3/`.

**INFERRED:** the earlier stale XMB app name resulted from replacing HDD files
without an installer/database refresh. The installed SFO had already held the
new title. **CLAIMED (owner after dev3 installation):** XMB now says
**CRT Toolset**, and its picture fits with 288p active.

**PROVEN (log):** dev2's default-save handling incorrectly rejected the
webMAN setter's bare decimal echo. Target 5 returned -2 and restored the
previous picture; an unchanged target 4 returned 0. Dev3 fixes this by
independently reading the setting after each write, with failure/rollback
cases covered in `tests/test_sd.c`. Live mode changes and saved defaults
must be reported separately when describing these development results.

**PROVEN (dev3 console checks):** stock 576i and 480i each activated their
stock analog raster, saved/read back `tvSystem=1` and `0` respectively,
and returned `rc=0` without rebooting. See
[the mode-switching record](SD_MODE_SWITCHING.md).

**PROVEN (dev3 background/XMB check):** enabling both families with a PAL
preference verified 24 stores, activated 576/624 on analog, deployed the
8368-byte helper by hash, loaded it successfully in slot 2, and wrote
`5\\n` to the preference file. Normal app exit logged all cleanup stages;
the helper reached its poll loop, armed with target 5, and saw `app_gone`.
Analog register `0x28000682800` remained `0x0000023f0000026f` at XMB.
Private evidence: dev3 `enable288.log`, `xmb288-app.log`,
`xmb288-helper.log`, and `xmb288.json`.

**UNKNOWN:** a deliberately induced opposite-family fallback, extended
idle stability, and PAL game/core coverage. The successful XMB picture is
an owner observation; electrical field parity remains unmeasured.

## Final development build: PS-menu repaint

**CLAIMED (owner during the low-line workflow):** the PS menu opens/closes, but the pre-existing
dim/spinner residue initially needed D-pad input. Dev4 fixes the repaint;
the owner confirms it now clears automatically. See
[the redraw checkpoint](PS_MENU_REDRAW.md).

**PROVEN (dev4 PS-menu log):** this particular repaint check reported public
resolution ID 4 after reopening the app. It validates the redraw fix, not
a separate PAL-mode PS-menu measurement. The app pauses the helper while
managing its own transitions; reopening can start in the saved NTSC default.
An explicitly PAL-mode PS-menu/game compatibility test remains open.

**PROVEN (dev4 deployment):** package version 01.33 installed through webMAN
after a normal app exit and XMB settle. Installed files match decoded package
members, and a fresh v1.3.0-dev4 log completes both initial frames.
SHA-256: `8f5f250597f7be6ea09233da54d2d1216d59a41cdb349b1a93e04509f2d6b94a`.
The patch/raster/settings/UI host suite, 19 startup/menu/idle scenarios,
compiled helper audit and all 54 stock-target comparisons with private 4.93
firmware pass. This build retains the validated PAL and SD-default recipes.

## Menu grouping and repeated remote installer hang

**PROVEN (dev5 source):** the first four actions are Test 240p, Enable 240p,
Test 288p, Enable 288p. The existing host suite passes with this order.
Package version 01.34; SHA-256
`f5f292f6204b0265c9627f6fdb49a5183582b8ef35537073f503034c6da06f23`.

**PROVEN (deployment observation):** the remote installer timed out again
during the dev5 update, followed by an FTP timeout, despite normal Circle exit
and a four-second XMB settle. **CLAIMED (owner):** black screen and frozen
console. This repeated during updating, not a demonstrated idle timeout.
**UNKNOWN:** the underlying VSH/installer failure. Do not mark dev5 installed
without checking its bytes after recovery. Further development updates use
verified direct HDD app-file replacement from an idle XMB, with background
enforcement disarmed; the intermittent remote installer route is stopped.

**PROVEN (dev5 recovery):** after reboot, surviving app logs showed every
cleanup stage completed; the helper log contained no correction request near
the hang. The previous SFO/EBOOT were still installed. After disarming
enforcement, direct replacement of the three HDD app files was verified
against decoded package bytes. The automated launch shortcut did not select
the app; the owner took over launching it manually. Private evidence:
`local_only/activation/pal288-development/dev5-recovery/`.
This completes the menu change while avoiding the unresolved installer path.
