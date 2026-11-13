# CRT Toolset (ps3-240p)

Experimental 240p/288p homebrew for PS3 AV Multi Out on Evilnat 4.93 + Cobra. The current release is **v1.3.6** (package version **01.36**).

<!-- progress:start -->
## Progress

![Project progress 79%](docs/progress.svg)

**Overall: about 79%** (effort-weighted estimate; the real risk is not evenly spread).

| Stage | Weight | Done |
|---|---:|---|
| Static analysis & claim verification (hypercalls, netemu, RSX attr 0x100) | 15% | `██████████` 100% |
| AV-manager timing tables & emulator video path trace | 20% | `██████████` 100% |
| LV1 process loader & memory architecture modeling | 15% | `██████████` 100% |
| Live LV1 anchor resolution (sysmgr_ss physical segments) | 15% | `██████████` 100% |
| Encoder & RSX 15 kHz progressive scanline feasibility | 15% | `████░░░░░░` 40.0% |
| Prototype LV1 timing patch / profile override | 10% | `████░░░░░░` 35.0% |
| Native PS3 homebrew application (EBOOT.BIN / PS3-240P.pkg) | 10% | `████████░░` 80.0% |
| Hardware waveform verification (line count, sync, 15.7 kHz) | 5% | `░░░░░░░░░░` 0% |

_Last update: 2026-11-13. v1.3.6 release checks complete: 240p/288p geometry, native background correction and repeated-enable reuse validated on the console; owner also confirms the requested PAL PS1/PS-menu/return-XMB flow. Live 480i/576i selection and saved defaults work without reboot. Final package retains the tested native helper and timing behavior with release branding/version metadata. Local menu preview uses the production C renderer. Host suites, 19 startup/menu/idle cases, compiled helper audit, 54 firmware targets and finalized package checks pass; electrical parity and broad compatibility remain open._
<!-- progress:end -->

The progress block is generated from `docs/progress.json`; percentages estimate work, not electrical verification.

## v1.3.6: PAL and SD mode selection

**PROVEN (source and host checks):** this release adds a full-source
PAL 288p candidate, a background preference for either SD family, and explicit
stock **480i / 60 Hz** and **576i / 50 Hz** choices with picture confirmation
and a saved boot default. **CLAIMED (owner):** the corrected 288p test works
with a complete picture. **PROVEN (readback):** the analog head uses the
576-source / 624-frame candidate. Both stock modes and saved defaults were
verified live without reboot; the 288p helper starts and retains the picture
at XMB. See [the PAL checkpoint](docs/PAL_288P_DEVELOPMENT.md) for test limits.

**CLAIMED (owner, dev4):** the old dimming/spinner residue after the PS menu
now clears automatically. See [the repaint fix](docs/PS_MENU_REDRAW.md).

The menu groups each mode as **Test**, then **Enable enforcement**:
240p first, followed by 288p. Stock 480i/576i choices come next.

The [enforcer checkpoint](docs/ENFORCER_STATUS_2026-11-13.md) corrects earlier
helper claims: starting a thread did not prove mode enforcement. Dev6 fixes
resident-slot reuse and replaces incorrect raw syscalls with native VSH
display calls. Timed switching and a real XMB fallback correction from mode 4
back to mode 5 both passed, with 576/624 analog readback.

The recovered Gemini procedure and live-switch/reboot distinction are documented
in [SD mode switching](docs/SD_MODE_SWITCHING.md). The current low-line patch
requires CFW LV1 access; ordinary HEN is unsupported. See
[HEN compatibility and upstream evidence](docs/HEN_SUPPORT.md).

**PROVEN (package metadata):** the app is renamed **CRT Toolset**.
Its existing title ID is retained so installing the update replaces the old app.

**CLAIMED (owner):** the requested PAL PS1 game/PS-menu/return-XMB test flow
works; the title was not specified. Electrical field parity, extended stability
and broad game/core coverage remain unmeasured or untested. PAL RetroArch
settings have not yet been validated. [Release notes](docs/RELEASE_v1.3.6.md).

## Menu preview

![CRT Toolset menu](deliverables/CRT-Toolset-menu.png)

Rendered locally with the production C renderer and menu strings. No PS3 is
needed: run `python -B tools/render_menu.py` (host compiler and Pillow required).

## v1.2.2: full-frame Mode 4

**CLAIMED (owner hardware tests, 2026-11-13):** the app and XMB show a complete picture, PS1 games work with `ps1_emu.self`, and RetroArch 1.10.0 runs Super Mario All-Stars with a working PS menu after its saved resolution/viewport are corrected to 720x480. See [the evidence and geometry checkpoint](docs/GEOMETRY_STATUS_2026-11-13.md).

**PROVEN (code and captured register readback):** the Mode-4 recipe retains full 480-line source fetching, uses a 522-line frame, and keeps unity scaling. The previous scaler experiment is disabled in normal enable. Source: `app/source/timing_payloads.h`, `app/source/main.c`, and the checkpoint's private capture references.

**UNKNOWN at the v1.2.2 checkpoint:** electrical progressive-sync/field parity, broader game/core compatibility, and PAL geometry. The PAL geometry result is superseded by v1.3.6 above; electrical field parity remains unmeasured.

## Install and use

1. Reboot to clear earlier resident helpers and RAM timing edits. Download **CRT-Toolset.pkg** from [GitHub Releases](https://github.com/Gabarsolon/ps3-240p/releases/latest) and install it through normal XMB Package Manager. The app is **CRT Toolset** (`PS240P001`).
2. Select AV Multi Out and start from a supported SD mode.
3. Test the desired 240p/288p mode, then choose its **Enable enforcement** option, confirm the picture, and return to XMB. Tests restore automatically after 15 seconds unless confirmed.
4. Stock 480i/576i choices stop correction and save the selected default after picture confirmation.
5. For **240p / NTSC RetroArch**, apply the [SD settings](docs/RETROARCH_SD.md). The included fragment is for the tested 720x480 setup, not a validated PAL preset. The PKG does not edit RetroArch's config.

**PROVEN (implementation):** core timing/enforcement uses CFW LV1 access,
Cobra/PS3MAPI and native VSH interfaces. Saving boot defaults currently also
requires webMAN's named-key HTTP interface. Ordinary HEN is unsupported.

**PROVEN (development observation):** remote webMAN installation intermittently
froze this console; use normal XMB Package Manager after rebooting. The final
release changes from the tested dev6 build are version/branding text and package
metadata. Its timing and native-helper behavior are retained.

**PROVEN (implementation):** timing edits and the resident helper are runtime-only; reboot clears them. Enable again after reboot. The development helper checks slots, reuses an unchanged resident copy, and deploys by content. **Correction to v1.2.2's helper claim:** its raw polling calls were incorrect; automatic SD correction is established only for the native-interface fix documented in [the enforcer checkpoint](docs/ENFORCER_STATUS_2026-11-13.md). Sources: `plugin/vsh_240p.c`, `app/source/main.c`.

Diagnostic logs, overwritten when their respective app/helper starts:

- `/dev_hdd0/tmp/ps3_240p.log`
- `/dev_hdd0/tmp/vsh_240p.log`

## Build and validation

PS3DK 0.26.0, CMake, Ninja and Python are used for the app build. Host checks require clang or gcc.

```powershell
python -B tools/test_app.py
python -B tools/build_ps3_app.py
```

Optional comparison with a private 4.93 module:

```powershell
python -B tools/test_app.py --firmware <path-to-local-4.93-sysmgr_ss.elf>
```

**PROVEN (release checks):** timing/rollback/settings/lifecycle/native-video/UI tests, 19 startup/menu/idle scenarios, the compiled helper audit, and all 54 stock timing/code target comparisons pass. The package validator checks the finalized container and its five members. These are offline checks, not electrical measurements. Source: `tools/test_app.py`, `tools/pkg_inspect.py`, `deliverables/package_manifest.json`.

The app builder packages the tracked `plugin/vsh_240p.sprx`. Rebuilding the helper with `tools/build_plugin.py` additionally requires the sibling native-PRX linker/packer/signing tools described in that script. The UI renderer reuses Buzz Bridge code; its attribution/license is in `app/BuzzBridge-LICENSE` and in the PKG.

## Research and history

- [Current geometry evidence and test results](docs/GEOMETRY_STATUS_2026-11-13.md).
- [Earlier app review and failed experiments](docs/APP_REVIEW_2026-11-11.md).
- [Verified research](docs/RESEARCH.md), [AV-manager trace](docs/LV1_AV_TRACE.md), [PS1/PS2 video paths](docs/BC_PS1_EMULATOR_VIDEO.md), and [live page resolution](docs/LV1_SYSMGR_SS_LOADADDR.md).
- Historical [feasibility assessment](docs/FEASIBILITY_240P_RSX_ENCODER.md) and [prototype specification](docs/PROTOTYPE_LV1_TIMING_PATCH.md) contain correction notices. [Waveform verification protocol](docs/VERIFICATION_WAVEFORM.md).

Sony-derived firmware, decrypted modules, decompiler output, phone photos and console captures remain in gitignored `local_only/`. This project does not implement firmware flashing.
