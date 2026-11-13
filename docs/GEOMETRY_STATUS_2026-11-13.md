# Geometry investigation checkpoint, 2026-11-13

## Observed state

- **CLAIMED (owner observation):** the last OpenCode test still displayed a cropped, stretched image. Source: session `ses_e30be46b5ffeJVtR41gZRa0Q2h`, last user message. This is a negative visual result, not a successful geometry fix.
- **PROVEN (retrieved logs):** the app completed the VSH load with result zero; the worker subsequently logged `alive`, `armed`, `app_active`, and `app_gone`. Source: private `local_only/activation/resume_logs_20261113_181820/{ps3_240p,vsh_240p}.log`, app timestamp 261045 and worker timestamps 264047338 through 274052918. This proves these stages ran; it does not prove a fallback correction occurred.
- **PROVEN (retrieved logs):** the second scaler experiment reached RAM and survived a subsequent inspection. Its control edit cleared mask `0x1000`; its factor was `0x08000000`. The software cache reported a 240-active/261-total raster on head 1 and a 720x480 source. Source: same app log, timestamps 244169 through 256457. Neither readback nor a software cache proves executed code, actual scaler registers, or an electrical waveform.

## Corrections to the earlier reasoning

- **PROVEN (application source):** the old diagnostic checked `scale_probe_ready` immediately after zeroing the patch context, before `patch_resolve`. Its repeated unavailable message was inevitable and supplied no evidence about live mappings. Source: `app/source/main.c`, `inspect`, before this checkpoint's fix.
- **PROVEN (private firmware):** globals at `0xc000af48` and `0xc000af70` are pointers, not two per-head descriptor arrays. The first points to the metadata table at `0x80039148`; the second points to fallback metadata at `0x80038b10`. The configuration path copies a metadata record before calling placement routine `0x80002ca4`. Source: private 4.93 `sysmgr_ss.elf`, these globals and routine `0x800005e4`. The corrected diagnostic captures the four already resolved SD metadata records and labels them table RAM.
- **PROVEN for Nouveau source, INFERRED for RSX:** [Linux Nouveau's register definitions](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/dispnv04/nvreg.h) assign mask `0x10` at `0x00680880` to vertical scaling enable and mask `0x1000` to vertical bilinear filtering. Calling the latter an established RSX "vertical exact fit" gate was unjustified. RSX hardware behavior still needs validation.
- **PROVEN for Nouveau source, INFERRED for this candidate:** [Nouveau's scaling setup](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/dispnv04/dfp.c), `nv04_dfp_mode_set`, computes manual scale ratios as source dimensions divided by output dimensions. This motivates a source-line step of 2.0 for 480 -> 240. It does not establish the encoding, direction, or behavior of RSX register `+0x89c`.
- **PROVEN (git):** historical commit `ca85d2d` already proposed mask `0x10` and a 2.0 step. Reusing those values is a controlled retest with corrected verification, not a novel or proven solution. Source: `git show ca85d2d`.

## v1.2.1 candidate and validation

- **PROVEN (source/host tests):** the flag-gated candidate sets mask `0x10` while preserving `0x1000`, and selects factor `0x20000000` at the two existing verified code targets. The candidate draws its UI at full source height. Source: `app/source/timing_payloads.h`, `app/source/lv1_patch.c`, `app/source/main.c`.
- **PROVEN (host tests):** every failed store in the NTSC scaler transaction rolls back, both code targets reject unknown bytes, and legacy experiment bytes are refused. All 17 startup scenarios, patch/raster/UI checks, and the plugin artifact audit pass. Source: `python tools/test_app.py --firmware <private 4.93 sysmgr_ss.elf>`.
- **PROVEN (independent binary comparison):** the optional firmware audit now compares all 44 targets, including the two code words. It previously stopped at 42. This confirms original bytes and locations, not the hardware interpretation. Source: `tests/audit_plan.c`, `tools/test_app.py`.
- **UNKNOWN:** whether the analog path uses these scaler registers, whether code edits become instruction-cache coherent, whether the valid window also requires adjustment, and whether this candidate fixes either the app or XMB image.

## Next hardware observation

Reboot to clear earlier RAM experiments, launch v1.2.1, and choose **Test 240p 60Hz** (second menu item). The scaler flag is `/dev_hdd0/tmp/240p_scaler.on`; the earlier metadata-height flag must be absent. Use the 15-second recovery path before trying persistent enable. This candidate's 2.0 ratio targets 480 -> 240, not PAL 576 -> 240.

Record whether the picture changes and whether the full frame is visible. Read the resulting logs to distinguish candidate rejection, completed RAM writes, successful output requests, and visual outcome. If the image remains unchanged, establish actual register state and execution/coherency before interpreting another factor change as a solution.

## Authorization and recovery artifacts

The owner explicitly authorized console actions on 2026-11-13 except actions that might hard brick it. This supersedes the repository's earlier console-access restriction for this session. This iteration changes the installed homebrew package and its experiment flag only; it does not add a boot plugin or write firmware/flash. Prior package artifacts are preserved privately in `local_only/activation/rollback_before_scaler3/`; deployment records and installed-file backups remain under `local_only/activation/`.

## Deployment result

**PROVEN (FTP comparison):** v1.2.1 package size is 196,032 bytes, SHA-256 `98e5d77235324ccd680bd774923c0744324364a6fef56cd1c335a5b1332051c5`. The uploaded package matched exactly; installed `PARAM.SFO`, `USRDIR/EBOOT.BIN`, and `USRDIR/vsh_240p.sprx` matched the decoded package bytes exactly. The scaler flag exists and the metadata-height flag is absent. Source: private `local_only/activation/scaler3_deployment/result.json`, deployment script and captured installer response. No reboot or application launch was issued; visual validation is pending with the owner.

## Live hardware results and matched-field geometry fix

- **PROVEN (hardware register reads):** the 1.2.1 code edits executed. Head 1 changed Y-scale control mask `0x10`, but the attempted `0x20000000` vertical factor read back as zero. Both heads had previously read unity. Source: private `local_only/activation/scaler3_capture/registers.json`, elapsed 4.704 through 21.735 seconds. This rules out stale instruction cache as the explanation for that capture; the exact factor field width remains UNKNOWN.
- **PROVEN (hardware readback), CLAIMED (owner visual result):** temporarily setting the live vertical factor to `0x08000000` retained that value, but the owner still saw cropping. Source: private `local_only/activation/live_halfscale/result.json` and the owner's reply during this session. The temporary write was restored; subsequent direct reads confirmed stock 480/525 timing and unity scaling.
- **PROVEN (hardware readback), CLAIMED (owner visual result):** restoring unity scaling and enabling source-field fetching with the original short raster changed the image to two vertically stacked copies, still cropped. Source: private `local_only/activation/source_fetch_test/result.json`; owner clarified "One above the other". This establishes a visible response to the fetch control but not a usable mode.
- **PROVEN (hardware readback), CLAIMED (owner visual result):** matching the source-field fetch control to a full 480-active / 522-total frame, marker 489, sync 488..494, with unity scaling, produced one correct full picture. The owner replied "YO ITS PERFECT IT WORKS" to the complete-picture test. Source: private `local_only/activation/field_timing_test/result.json`. All temporary register changes were restored without reported write errors.
- **INFERRED timing interpretation:** two 261-line field periods with an even full-frame total eliminate the prior field/raster size mismatch. At the existing horizontal period, this is the intended 240-visible-lines-per-field cadence. **UNKNOWN:** electrical odd/even field phase and sync details; a correct full picture is not an oscilloscope measurement proving 240p rather than interlaced output.

### v1.2.2 implementation

**PROVEN (source):** the NTSC metadata retains source-field fetching and the full 480-line framebuffer; the raster uses the successful 480/522 counts and 489/488/494 marker/sync values. The field-sized packed window and other timing controls remain as in the successful live test. Normal enable ignores the retired scaler flag and leaves the scaler instructions unchanged. Source: `app/source/timing_payloads.h`, `app/source/main.c`.

**PROVEN (validation):** patch rollback, the 17 startup scenarios, raster/UI checks, plugin artifact audit, all 44 firmware target comparisons, native build, and PKG member verification pass. Host assertions independently check the matched-field counts and packed window against the successful recipe. Source: `tests/test_patch.c`, `tests/test_raster.c`, and the build/test commands in this session.

**Scope:** this corrects the NTSC/Mode-4 path used by normal enable. The older PAL candidate is unchanged and has no equivalent geometry validation. The enforcer still converts a Mode-5 fallback to Mode 4. Universal game compatibility and electrical progressive-sync verification remain open.

## Owner validation and RetroArch regression

- **CLAIMED (owner, this session):** v1.2.2 normal enable works in the app and XMB, and PS1 games work perfectly with `ps1_emu.self`. The remote verification attempt did not complete before the owner enabled it manually; this owner report supplies that visual result.
- **CLAIMED (owner), PROVEN (retrieved photo):** RetroArch's menu is enlarged/cut off. The newest phone photo was retrieved at the owner's request through ADB and kept privately in `local_only/activation/retroarch_freeze_20261113_185459/`. The owner reported a freeze upon pressing PS; webMAN and FTP were still responsive during initial collection. This does not establish a complete kernel lockup.
- **PROVEN (hardware reads during the report):** the analog path retained the matched 480/522 timing, 489/488/494 marker/sync, and unity scaling. Source: private `hardware.json` in that capture directory. This narrows the picture problem beyond simply losing the timing patch.
- **PROVEN (saved configuration):** RetroArch PSX-Place CE's config requested resolution ID 1 and retained a 1280x720 custom viewport. Local PS3DK `cell/cell_video_out.h` defines ID 1 as 1080 and ID 4 as 480. [RetroArch v1.10.0's PS3 context](https://github.com/libretro/RetroArch/blob/v1.10.0/gfx/drivers_context/ps3_ctx.c) passes the configured resolution dimensions to PSGL. **INFERRED:** these settings can contribute to a mismatched rendering surface; the exact installed community build and runtime dimensions remain to be verified.
- **PROVEN (config deployment):** a verified backup of the current config was made on PC and console, then resolution ID 4, viewport 720x480, file logging, and verbose logging were written and verified byte-for-byte. Only these five settings changed. Backup: `/dev_hdd0/game/RETROARCH/USRDIR/retroarch.cfg.codex-before-sd-20261113-185459`; local hashes and before/after values are in private `deployed-config.json`.
- **CLAIMED (owner) and PROVEN (later register read):** the owner rebooted to XMB; hardware then showed stock 480/525 timing. The first post-config launch is therefore a stock-output baseline, not a 240p validation. Remote launch did not produce a running RetroArch process, so the owner was asked to start it manually. Picture and PS-overlay fixes remain UNKNOWN.

### Corrected RetroArch baseline

**PROVEN (runtime log):** the installed community build identifies as RetroArch 1.10.0, built Feb 17 2022. After the config correction and reboot, its GL context reports 720x480. The stock-output register read is 480 active / 525 total, so this is not yet a patched-timing result. Source: private `stock-retroarch.log` in the RetroArch capture directory and the register reads in this session.

**CLAIMED (owner):** RetroArch now looks fine and the PS menu opens on this baseline. The owner was asked to repeat the check after enabling the 240p app, preserving the distinction between a stock-output fix and patched-timing compatibility. The README progress block was moved directly below the introduction, and the instruction file was renamed to `AGENTS.md`, as requested.

### RetroArch patched-timing validation: passed

**PROVEN (captured runtime log and hardware reads):** RetroArch was running and its GL context reported 720x480 while the analog hardware retained the matched 480-active / 522-total setup. The enforcer log showed armed/app-gone without a correction event in the captured run. Source: private `patched-retroarch.log`, `patched-vsh_240p.log`, and `patched-timing-observed.json` in the RetroArch capture directory.

**CLAIMED (owner, requested combined test):** "Picture and PS menu work" after exiting RetroArch, enabling option 0 in v1.2.2, returning to XMB and reopening RetroArch. This confirms the reported menu distortion and PS-menu freeze were absent with the corrected config under the active timing patch. No RetroArch binary or additional timing payload change was required for this fix. Broader core/game coverage remains untested.

The configuration backup and five changed settings are recorded above. Electrical waveform verification remains a separate open task.

**CLAIMED (owner, additional game test):** Super Mario All-Stars works perfectly in RetroArch with the corrected configuration and service enabled. This adds a named SNES content test to the menu/overlay checks.
