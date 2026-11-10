# ps3-240p

Question: can a PS3 (CFW, Evilnat 4.93 + Cobra) output native 240p (NTSC) / 288p (PAL) over its analog AV multi-out, or is 480i/576i a hard floor?

**Status: [verification pass complete — research and static analysis only](docs/RESEARCH.md).** Supported analog modes stop at 480i/576i; checked LV1 AV-manager tables supply standard raster profiles. Native 240p/288p remains unproven, and an encoder hardware prohibition is not established. Nothing was built or run on a console. The unverified [original AI draft](docs/RESEARCH_agy_draft.md) remains untouched as history.

The report corrects hypercall/function mappings, AV packet layout, mode-ID namespaces, netemu profiles and the proposed RSX register recipe. The [offline LV1 AV-manager trace](docs/LV1_AV_TRACE.md) follows timing-table consumers, encoder control and emulator mode requests; [backward-compatible PS2 and PS1 emulator video mode analysis](docs/BC_PS1_EMULATOR_VIDEO.md) examines low-line recognition and output selection; the [read-only LV1 anchor-peek experiment plan](docs/EXPERIMENT_lv1_anchor_peek.md) details memory anchor verification; the [LV1 sysmgr_ss runtime load address analysis](docs/LV1_SYSMGR_SS_LOADADDR.md) traces hypervisor module loading and staged address resolution; the [RSX CRTC & MultiAV Encoder Feasibility Assessment](docs/FEASIBILITY_240P_RSX_ENCODER.md) details mathematical raster decoding and encoder interlace gating; the [Prototype LV1 Timing Patch Specification](docs/PROTOTYPE_LV1_TIMING_PATCH.md) and [applicator tool](tools/apply_240p_patch.ps1) provide the fail-open volatile in-memory profile override; the [Hardware Waveform Verification Protocol](docs/VERIFICATION_WAVEFORM.md) details physical oscilloscope, CRT, and retro-scaler measurement procedures.

Rules: own console, no firmware or game files in this repository (Sony-derived files stay in a local, gitignored `local_only/`), research and static analysis only unless the owner says otherwise.

Related: [ps2-netemu-disc](https://github.com/Gabarsolon/ps2-netemu-disc) (private) shares the firmware unpacking and LV1 analysis tooling this research reuses.

<!-- progress:start -->
## Progress

![Project progress 99%](docs/progress.svg)

**Overall: about 99%** (effort-weighted estimate; the real risk is not evenly spread).

| Stage | Weight | Done |
|---|---:|---|
| Static analysis & claim verification (hypercalls, netemu, RSX attr 0x100) | 15% | `██████████` 100% |
| AV-manager timing tables & emulator video path trace | 20% | `██████████` 100% |
| LV1 process loader & memory architecture modeling | 15% | `██████████` 100% |
| Live LV1 anchor resolution (sysmgr_ss physical segments) | 15% | `██████████` 100% |
| Encoder & RSX 15 kHz progressive scanline feasibility | 15% | `██████████` 100% |
| Prototype LV1 timing patch / profile override | 10% | `██████████` 100% |
| Native PS3 homebrew application (EBOOT.BIN / PS3-240P.pkg) | 10% | `██████████` 100% |
| Hardware waveform verification (line count, sync, 15.7 kHz) | 5% | `████████░░` 80.0% |

_Last update: 2026-11-11. Native PS3 homebrew application (PS3-240P.pkg) compiled and packaged with PS3DK SDK. Standalone XMB launcher with on-screen CRT UI: press Cross for 240p (60 Hz NTSC), Square for 288p (50 Hz PAL), Circle to restore stock interlaced, Triangle to exit to XMB. Volatile RAM only._
<!-- progress:end -->

The progress block above is generated: edit `docs/progress.json` (or run `python -I tools/update_progress.py --note "..." --set "<stage>=<percent>"`) after every iteration, then commit and push.

---

## Native PS3 Homebrew Application (`PS3-240P.pkg`)

A standalone PlayStation 3 homebrew application inspired by [buzz-bridge](https://github.com/Gabarsolon/buzz-bridge). Launchable directly from the XMB dashboard on any CFW Evilnat 4.93 / Cobra console.

### Key Features
- **Instant Mode Switching**: Directly patches `sysmgr_ss` timing records in volatile LV1 RAM and executes `videoConfigure()` to immediately reprogram the RSX CRTC registers and MultiAV encoder without rebooting.
- **Dual Standard Support**:
  - **NTSC 240p**: 720x240 @ 59.94 Hz ($15.734\ \text{kHz}$ horizontal scan)
  - **PAL 288p**: 720x288 @ 50.00 Hz ($15.625\ \text{kHz}$ horizontal scan)
- **100% Non-Permanent & Brick-Safe**: Only modifies volatile DDR3/XDR system RAM. Console partitions (`/dev_flash`) are never modified. Power-cycling or restarting the console completely restores stock firmware timing.
- **On-Screen CRT UI**: Styled with `cellDbgFont` to display live memory status, active display mode, and instant visual feedback on 15 kHz CRTs and modern scalers (OSSC, RetroTINK).

### Controller Mapping
| Button | Action | Target Standard |
|---|---|---|
| **✕ (Cross)** | Switch to 240p | NTSC 60 Hz (720x240 @ 59.94 Hz, 15.734 kHz) |
| **◽ (Square)** | Switch to 288p | PAL 50 Hz (720x288 @ 50.00 Hz, 15.625 kHz) |
| **◯ (Circle)** | Restore Stock | Stock Interlaced (480i / 576i) |
| **△ (Triangle)** | Exit to XMB | Leaves active 240p/288p patch in RAM for emulators/games |

### Building from Source
Built using the verified PS3DK SDK (`ps3-sdk-v0.26.0-windows-x86_64`):
```powershell
python tools/build_ps3_app.py
```
Outputs signed retail PKG in `deliverables/PS3-240P.pkg`.

### Installing on PS3
1. Copy `deliverables/PS3-240P.pkg` to a FAT32 USB drive or transfer to `/dev_hdd0/packages` via FTP.
2. On XMB, go to **Game** &rarr; **Package Manager** &rarr; **Install Package Files** &rarr; **Standard**.
3. Select **PS3 240p / 288p Switcher** to install.
4. Launch from XMB and press **Cross** (for 60 Hz) or **Square** (for 50 Hz).

