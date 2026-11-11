# ps3-240p

Research and experimental homebrew for native 240p/288p over PS3 AV Multi Out on Evilnat 4.93 + Cobra.

**Native low-line output remains UNKNOWN.** The owner tested the original package over SCART RGB and the screen remained at 576i. Earlier claims of proven hardware feasibility, immediate mode switching and completed waveform verification were overstated. See [the app review, evidence and remaining activation gap](docs/APP_REVIEW_2026-11-11.md).

## Native application v1.1.4 (diagnostics only)

Build output: `deliverables/PS3-240P.pkg` (XMB title **PS3 240p / 288p Switcher**, SFO version **01.14**, on-screen **v1.1.4**). The XMB icon uses the owner's supplied CRT artwork, fitted without stretching into a transparent 320x176 RGBA PNG; the original is retained at `app/assets/icon-source.png`.

**PS-menu handling:** the owner reports that v1.1.3 freezes the OS overlay before a choice. Its log shows the app still alive with `system_menu=1`; no timing experiment ran. v1.1.4 follows the SDK system-menu sample by continuing flips of the frozen scene while OS UI is active. It tracks drawing begin/end as well as menu events, suspends app input/inspection, waits for button release on resume, and lets Quit interrupt GPU waits. Raw callback events are logged after the callback dispatcher returns. This addresses a presentation-policy difference; physical PS-menu behavior still requires owner verification.

**Owner result:** after reboot, v1.1.2's menu is visible. Its 288p experiment failed: the owner observed 60 Hz / 480i, black after Circle, and a whole-console freeze when opening the PS menu. The log verifies candidate writes and RAM rollback but only reports standard API mode IDs; it does not prove a usable physical signal. **v1.1.3 disables timing tests and in-app restoration transitions in production.** Use a console reboot to clear previous timing edits. The activation/recovery mechanism needs offline investigation before another hardware test.

The current build retains the menu renderer that the owner saw working after reboot. It completes both initial display buffers, defers LV1 inspection until selected, and records frame samples, flip completions and menu heartbeat. The cause of the earlier black startup was not established.

**PROVEN startup fix:** v1.1 incorrectly compared native enabled state 0 with the legacy constant 1. Current builds use the native state constants. Fifteen host entry-point scenarios cover startup, initial buffers, disabled timing actions, PS-menu open/resume/quit, drawing-only events, held closing buttons and GPU waits suspended by OS UI.

- Uses the same software renderer as the sibling Buzz Bridge PS3 app: font, colors, D-pad list, wrapped text, overscan margins, aspect handling and interlace filtering. Attribution/license is in `app/BuzzBridge-LICENSE` and inside the package.
- D-pad moves, Cross selects, Circle returns/exits. PAL/NTSC choices explain that tests are unavailable; stock timing information directs the owner to reboot. These choices perform no timing writes or output transitions.
- Explicit memory inspection resolves protection pages and recognizes all 42 words across both SD aliases. Three erroneous PAL stock fields from the original app are corrected.
- The old transaction/transition logic remains available only to offline host fixtures through `TIMING_OFFLINE_TEST`; production blocks entry before availability queries, snapshots, stores or transitions. There is no in-app enable option.
- RAM readback and `videoGetState` do not prove an actual 240p/288p waveform. A successful physical activation and recovery path has not been implemented.

The public video API has no arbitrary active-height field. The AV manager bounds request dimensions by the patched metadata, so full-height standard requests may be rejected. Resolving that activation path and confirming physical sync remain open work.

### Build and local validation

PS3DK 0.26.0, CMake, Ninja and Python are required to build. Host tests additionally need clang or gcc.

```powershell
python -B tools/test_app.py
python -B tools/build_ps3_app.py
```

Optional private firmware audit, without publishing/copying firmware:

```powershell
python -B tools/test_app.py --firmware <path-to-local-4.93-sysmgr_ss.elf>
```

The tests cover page resolution, all SD aliases, refusal of unknown bytes, rollback at each failed PAL store, video transition/recovery paths, and UI bounds at SD/HD in both aspects. The private audit compares all 42 stock target words against the local module. Package builds decrypt/inspect members and generate `deliverables/package_manifest.json` with hashes. These checks do not run firmware or measure a console's output.

### Using the diagnostics build

1. Power off/restart the console to clear the old build's RAM edits. Install the new package through XMB Package Manager from FAT32 USB or `/dev_hdd0/packages`.
2. Start in normal **576i / AV Multi Out / SCART RGB**. The menu and diagnostics are available; the failed mode test cannot run.
3. Do not reinstall an older experimental package to repeat the failing 288p/restore sequence.

Log: `/dev_hdd0/tmp/ps3_240p.log` (overwritten at app startup). Read it from the owner's console with:

```powershell
curl.exe --fail --max-time 15 -o build/ps3_240p-console.log ftp://192.168.0.169/dev_hdd0/tmp/ps3_240p.log
```

Legacy `tools/apply_240p_patch.ps1` live writes are retired because its physical addresses were tied to one boot. Offline `-UnitTest`/`-WhatIf` reference operations remain available.

## Research

- [Verified research](docs/RESEARCH.md) and untouched [original AI draft](docs/RESEARCH_agy_draft.md).
- [LV1 AV-manager trace](docs/LV1_AV_TRACE.md), [BC PS2/PS1 video paths](docs/BC_PS1_EMULATOR_VIDEO.md), [LV1 process/page resolution](docs/LV1_SYSMGR_SS_LOADADDR.md), and [read-only anchor plan](docs/EXPERIMENT_lv1_anchor_peek.md).
- Historical [feasibility assessment](docs/FEASIBILITY_240P_RSX_ENCODER.md) and [prototype specification](docs/PROTOTYPE_LV1_TIMING_PATCH.md), with correction notices; [waveform verification protocol](docs/VERIFICATION_WAVEFORM.md).

Own console only. Sony-derived inputs, firmware and instruction/decompiler listings stay in gitignored `local_only/`. Local building/testing is authorized; installing, launching or writing to the console is owner-run. Volatile timing edits are experimental; no flash modification is implemented.

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

_Last update: 2026-11-11. v1.1.3 owner reports PS overlay freezes before a choice; log shows app alive with system_menu=1 and no timing test. v1.1.4 continues frozen-scene flips like the SDK sample, handles drawing callbacks, suppresses input until release, and permits Quit during OS-paused GPU waits. Fifteen entry-point simulations and native/package checks pass; console overlay fix unverified. Timing tests stay disabled._
<!-- progress:end -->

The progress block is generated from `docs/progress.json`; percentages estimate work, not a probability of native output working.
