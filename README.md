# CRT Toolset (ps3-240p)

240p/288p output and native analog gamma correction for PS3 AV Multi Out,
verified on Evilnat 4.93 CEX + Cobra. Current official release:
**[v1.4.4-rc1](https://github.com/Gabarsolon/ps3-240p/releases/tag/v1.4.4-rc1)**
(package version **01.44**; the tested build keeps its original `rc1` name).

**CLAIMED (owner):** gamma works together with both 240p and 288p on the tested
CRT setup. [Current release notes](docs/RELEASE_v1.4.4-rc1.md) and
[gamma evidence and instructions](docs/GAMMA.md).

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

Current feature status (separate from the weighted research estimate):

| Feature | Status / remaining work | Evidence |
|---|---|---|
| 240p / 288p and analog gamma | Released; owner reports success. v1.6.0-dev6 pre-release adds single-SD-family support: dev5 passes the 480i+576i availability gate (PROVEN: owner PAL unit); dev6 falls back to AV head cache invalidation when the other SD family cannot be configured (UNKNOWN until an NTSC/component console reports; fails safe to stock). Wider connector/game coverage and electrical waveform measurement remain open. | [Release and calibration notes](docs/CRT_CALIBRATION.md); [SD results](docs/RELEASE_v1.4.4-rc1.md); [dev6 notes](docs/RELEASE_v1.6.0-dev6.md); [mechanism](docs/SD_SINGLE_FAMILY_240P.md) |
| Sysmgr PID compatibility | PROVEN in supplied external log: sysmgr PID 9 and all 54 targets resolved. CLAIMED: CECH-2004A 240p works; switching to ps1_netemu resolves its reported PS1 audio/crop issues. | [PID investigation](docs/SYSMGR_DISCOVERY.md) |
| HDMI 540p | PROVEN: full XMB picture and receiver-reported progressive output in timed dev19 test. UNKNOWN: HD CRT, component and game compatibility; no permanent enforcement. | [540p prerelease](docs/RELEASE_v1.5.0-dev19.md) |
| HDMI 240p / 288p | CLAIMED: stock HDMI 480p is visible directly on the owner monitor. PROVEN: timed stock recovery and direct USB-video captures; both OBS and FFmpeg are black through the splitter/capture chain. UNKNOWN: failing link/card behavior and working HDMI 240p/288p; no low-line candidate applied. | [HDMI low-line investigation](docs/HDMI_LOWLINE.md) |
| Resolution presets and scaling | PROVEN: previous native PC/XGA XMB tests and revised in-app 1280x768 / 1024x768 previews with stock recovery. Unified five-mode preview/keep controller and GPU handshake pass offline checks. In-app 540p reached software preview but receiver had no signal; a corrected source-height candidate is built, untested. Live keep/enforcement, remaining app presets, scaling, games, component and HEN are unverified; repeated power outages interrupted testing. | [Custom menu development](docs/HDMI_CUSTOM_RESOLUTIONS.md); [PC presets](docs/PC_PRESETS_DEVELOPMENT.md); [XGA](docs/XGA_DEVELOPMENT.md); [aspect research](docs/HD_ASPECT_RESEARCH.md) |
| HEN diagnostic and port | PROVEN: diagnostic builds and public-only CFW smoke pass. Additional native/gamma/mapping paths inspected offline; no ordinary-HEN 240p/288p route established. Actual HEN runtime untested. | [Alternative paths](docs/HEN_ALTERNATIVE_PATHS.md); [diagnostic scope](docs/HEN_DIAGNOSTIC.md) |
| Community compatibility and measurements | CLAIMED: external tester resolves PS1 audio/crop by selecting ps1_netemu; owner reports normal CRT audio. PROVEN: rc1 assets retain their checked digests; the unchanged rc1 build is now the official Latest SD release. UNKNOWN: exact emulator/setup difference, integer scaling and latency. | [PS1 report](docs/PS1_AUDIO_REPORT.md); [rc2 diagnostic](docs/RELEASE_v1.4.4-rc2.md); [tracked requests](docs/DISPLAY_PRESETS_ROADMAP.md#other-community-requests-to-track) |
| SCART RGB DVD/Blu-ray green picture | PROVEN: green = AV encoder in component mode; two LV1 patches force RGB output and remove the green (XMB unaffected). PROVEN: the encoder has no YCbCr-to-RGB conversion (letterbox bars turn magenta) and RSX scanout byte 9 is colorimetry only, so the conversion must happen in the player. UNKNOWN: the bdp_BDVD patch that makes the player output RGB; reverse engineering in progress. No release. | [SCART RGB DVD investigation](docs/SCART_RGB_DVD.md) |

_Last update: 2026-11-16. v1.6.0-dev6: single-SD-family 240p via AV head cache invalidation (shipped, awaiting NTSC/component test); SCART RGB DVD green-fix research added_
<!-- progress:end -->

The progress block is generated from `docs/progress.json`; percentages estimate work, not electrical verification.

## Official SD release: PID compatibility

[v1.4.4-rc1](https://github.com/Gabarsolon/ps3-240p/releases/tag/v1.4.4-rc1)
is now the official SD release, using the unchanged tested package. It
addresses the v1.4.3 "sysmgr PID 10 not found" error.
**PROVEN (source/tests):** timing and gamma use checked service discovery
across 32 slots. **PROVEN (supplied tester log):** the affected console uses
PID 9 and the RC resolves all 54 targets. **CLAIMED (tester):** app/XMB 240p,
timed recovery and background correction work. Their PS1 audio/crop issues
were resolved by switching to **ps1_netemu**; this is a reported workaround,
not proof of universal ps1_emu incompatibility. [Audio report](docs/PS1_AUDIO_REPORT.md).
[Install/test steps and evidence](docs/RELEASE_v1.4.4-rc1.md).
This SD build excludes experimental HDMI modes. The separate
[v1.4.4-rc2 diagnostic prerelease](https://github.com/Gabarsolon/ps3-240p/releases/tag/v1.4.4-rc2)
adds bounded logging; it is unnecessary for users whose setup already works.

## Experimental HDMI 540p prerelease

[v1.5.0-dev19 prerelease](https://github.com/Gabarsolon/ps3-240p/releases/tag/v1.5.0-dev19)
adds a **15-second HDMI 540p test**.
**PROVEN (capture/logs):** the full XMB picture fits and stock 1080i recovery
completes. **CLAIMED (owner):** the manual test works.
HD CRT/component compatibility and game coverage remain **UNKNOWN**;
the HD test currently accepts HDMI only. It does not keep 540p permanently.
[Install steps, required checks and tester feedback](docs/RELEASE_v1.5.0-dev19.md).
The official SD release above remains the normal download.

## Experimental HDMI PC presets

**PROVEN (owner-console capture/logs):** the native VSH path now selects
1280x768 (5:3), 1280x1024 (5:4) and 1920x1200 (16:10), with 15-second tests
and exact stock recovery. Live receiver dimensions confirmed WUXGA despite
a stale cached 1080p report. The isolated `1.6.0-dev1` build adds a preset
menu and restores 1080p automatically; no permanent setting is saved.
[Build scope, evidence and limits](docs/PC_PRESETS_DEVELOPMENT.md).

These modes convert normal 720p/1080p rendering sources. Games, geometry
accuracy, custom 4:3 timings, scaling controls, component and HEN remain
**UNKNOWN**. This development build is separate from the stable and 540p
release artifacts.

## v1.4.3: native analog gamma

**PROVEN (release source):** enter a correction value from **0.50 to 4.00**,
compare grayscale/shadow ramps, and test the native analog curve with automatic
recovery. **1.00 is neutral; higher values brighten shadows.** The dedicated
2.20 preset has been removed; use the value editor. Saving remembers the editor
value and does not apply gamma at boot.

Choose **Test system gamma**, then Cross during the countdown to keep it or
Circle to restore the previous curve. The helper's 15-second timer continues
while the PS menu is open. **Restore system gamma** restores the original
session baseline. [Full instructions](docs/GAMMA.md).

For calibration, set the TV's **Contrast/Picture**, adjust **Brightness** so
black is dark while near-black detail remains visible, then choose gamma.
Recheck black level and gamma after changing the TV controls.
[CRT calibration guide and research](docs/CRT_CALIBRATION.md).

**CLAIMED (owner, 2026-11-13):** gamma works "perfect so far" and alongside
both 240p and 288p. **UNKNOWN:** measured gamma response and broad game/model
coverage. The release keeps the tested helper unchanged and adds host-checked
preference replacement for repeated saves. [Validation and limits](docs/RELEASE_v1.4.3.md).

**PROVEN (implementation):** optional RetroArch Cg export writes a separate
content shader without editing existing configuration. Its execution in
RetroArch is untested. Use it as an alternative to native correction, rather
than applying both. [PC gamma preview](deliverables/CRT-Toolset-gamma-preview.png).

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

1. Exit CRT Toolset and install **CRT-Toolset.pkg** from [GitHub Releases](https://github.com/Gabarsolon/ps3-240p/releases/latest) through normal XMB Package Manager. Reboot after upgrading to clear old resident helpers/RAM edits, then open **CRT Toolset** (`PS240P001`).
2. Select AV Multi Out and start from a supported SD mode.
3. Test the desired 240p/288p mode, then choose its **Enable enforcement** option, confirm the picture, and return to XMB. Tests restore automatically after 15 seconds unless confirmed.
4. Stock 480i/576i choices stop correction and save the selected default after picture confirmation.
5. For gamma, open **Gamma / shadow correction**, edit the value, then choose **Test system gamma** and confirm during the countdown. [Instructions](docs/GAMMA.md).
6. For **240p / NTSC RetroArch**, apply the [SD settings](docs/RETROARCH_SD.md). The included fragment is for the tested 720x480 setup, not a validated PAL preset. The PKG does not edit RetroArch's config.

**PROVEN (implementation):** core timing/enforcement uses CFW LV1 access,
Cobra/PS3MAPI and native VSH interfaces. Saving boot defaults currently also
requires webMAN's named-key HTTP interface. Ordinary HEN is unsupported.

**PROVEN (development observation):** remote webMAN installation intermittently
froze this console; use normal XMB Package Manager. The v1.4.3 helper is byte-identical
to the owner-tested gamma development build. The final app changes are release
version text and the preference-save replacement fix; final checks were offline.

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

**PROVEN (release checks):** gamma math/storage/IPC/recovery/native guards and timing/rollback/settings/lifecycle/native-video/UI tests, 21 startup/menu/overlay scenarios, the compiled helper audit, and all 54 stock timing/code target comparisons pass. The package validator checks the finalized container and its five members. These are offline checks, not electrical measurements. Source: `tools/test_app.py`, `tools/pkg_inspect.py`, `deliverables/package_manifest.json`.

The app builder packages the tracked `plugin/vsh_240p.sprx`. Rebuilding the helper with `tools/build_plugin.py` additionally requires the sibling native-PRX linker/packer/signing tools described in that script. The UI renderer reuses Buzz Bridge code; its attribution/license is in `app/BuzzBridge-LICENSE` and in the PKG.

## Research and history

- [Native HDMI PC-preset probe](docs/DISPLAY_PRESET_PROBE.md): **PROVEN on the owner setup:** 1280x768 with a visible XMB picture, receiver-reported progressive scan, a 15-second hold and stock 1080p restoration. This isolated developer helper is not an app-menu release; other PC modes, game compatibility and HEN remain untested.
- [HEN port plan](docs/HEN_PORT_PLAN.md): capability separation and an isolated diagnostic are being developed; no ordinary-HEN 240p/288p support is established.

- [Resolution presets and scaling roadmap](docs/DISPLAY_PRESETS_ROADMAP.md) and [offline retail 4.93 investigation](docs/DISPLAY_PRESETS_INVESTIGATION.md). **PROVEN in source/binary:** existing PC-mode entries include 1280x768, 1280x1024 and 1920x1200. **UNKNOWN:** retail GameOS activation, live output and game compatibility; no new preset is released.

- [540p for HD CRTs: research](docs/HD_CRT_540P.md), [dev18 detected progressive HDMI output](docs/HD540_DEV18.md) and [dev19 app-menu pitch fix](docs/HD540_DEV19.md). **PROVEN on the tested capture setup:** the full XMB picture fits and the receiver reports 1920x540/progressive. Dev19 fixes a stale app-pitch check that aborted owner-launched tests; its actual app-menu run held the candidate for 15.220 seconds and restored stock 1080i/source pitch with no reported error. Native scanout samples alternate rows of the unchanged 1080-line source. **UNKNOWN:** measured refresh, HD CRT/component behavior and game coverage. Experimental; absent from stable v1.4.3.
- [CECH-2004A lookup failure and RC result](docs/CECH2004A_LOOKUP_REPORT.md). **PROVEN (supplied log):** the affected tester's service is PID 9 and the RC resolves it. **CLAIMED:** 240p works; separate PS1 audio/crop issues are open. See [discovery evidence](docs/SYSMGR_DISCOVERY.md).
- [Current geometry evidence and test results](docs/GEOMETRY_STATUS_2026-11-13.md).
- [Earlier app review and failed experiments](docs/APP_REVIEW_2026-11-11.md).
- [Verified research](docs/RESEARCH.md), [AV-manager trace](docs/LV1_AV_TRACE.md), [PS1/PS2 video paths](docs/BC_PS1_EMULATOR_VIDEO.md), and [live page resolution](docs/LV1_SYSMGR_SS_LOADADDR.md).
- Historical [feasibility assessment](docs/FEASIBILITY_240P_RSX_ENCODER.md) and [prototype specification](docs/PROTOTYPE_LV1_TIMING_PATCH.md) contain correction notices. [Waveform verification protocol](docs/VERIFICATION_WAVEFORM.md).

Sony-derived firmware, decrypted modules, decompiler output, phone photos and console captures remain in gitignored `local_only/`. This project does not implement firmware flashing.
