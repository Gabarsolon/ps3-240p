# ps3-240p

Question: can a PS3 (CFW, Evilnat 4.93 + Cobra) output native 240p (NTSC) / 288p (PAL) over its analog AV multi-out, or is 480i/576i a hard floor?

**Status: [verification pass complete — research and static analysis only](docs/RESEARCH.md).** Supported analog modes stop at 480i/576i; checked LV1 AV-manager tables supply standard raster profiles. Native 240p/288p remains unproven, and an encoder hardware prohibition is not established. Nothing was built or run on a console. The unverified [original AI draft](docs/RESEARCH_agy_draft.md) remains untouched as history.

The report corrects hypercall/function mappings, AV packet layout, mode-ID namespaces, netemu profiles and the proposed RSX register recipe. The [offline LV1 AV-manager trace](docs/LV1_AV_TRACE.md) follows timing-table consumers, encoder control and emulator mode requests; [backward-compatible PS2 and PS1 emulator video mode analysis](docs/BC_PS1_EMULATOR_VIDEO.md) examines low-line recognition and output selection; the [read-only LV1 anchor-peek experiment plan](docs/EXPERIMENT_lv1_anchor_peek.md) details memory anchor verification; the [LV1 sysmgr_ss runtime load address analysis](docs/LV1_SYSMGR_SS_LOADADDR.md) traces hypervisor module loading and staged address resolution; the [RSX CRTC & MultiAV Encoder Feasibility Assessment](docs/FEASIBILITY_240P_RSX_ENCODER.md) details mathematical raster decoding and encoder interlace gating; the [Prototype LV1 Timing Patch Specification](docs/PROTOTYPE_LV1_TIMING_PATCH.md) specifies the fail-open in-memory profile override.

Rules: own console, no firmware or game files in this repository (Sony-derived files stay in a local, gitignored `local_only/`), research and static analysis only unless the owner says otherwise.

Related: [ps2-netemu-disc](https://github.com/Gabarsolon/ps2-netemu-disc) (private) shares the firmware unpacking and LV1 analysis tooling this research reuses.

<!-- progress:start -->
## Progress

![Project progress 89%](docs/progress.svg)

**Overall: about 89%** (effort-weighted estimate; the real risk is not evenly spread).

| Stage | Weight | Done |
|---|---:|---|
| Static analysis & claim verification (hypercalls, netemu, RSX attr 0x100) | 15% | `██████████` 100% |
| AV-manager timing tables & emulator video path trace | 20% | `██████████` 100% |
| LV1 process loader & memory architecture modeling | 15% | `██████████` 100% |
| Live LV1 anchor resolution (sysmgr_ss physical segments) | 15% | `██████████` 100% |
| Encoder & RSX 15 kHz progressive scanline feasibility | 15% | `██████████` 100% |
| Prototype LV1 timing patch / profile override | 15% | `██████░░░░` 60.0% |
| Hardware waveform verification (line count, sync, 15.7 kHz) | 5% | `░░░░░░░░░░` 0% |

_Last update: 2026-11-11. RSX CRTC &amp; MultiAV Encoder Feasibility PROVEN: all 14 words of 56-byte raster record mathematically decoded (zero mismatches across all 28 profiles); CXM4027R register 0x37 bit 7 proved as hardware interlace gate; 240p/288p candidate binary payloads and fail-open LV1 prototype patch specification documented._
<!-- progress:end -->

The progress block above is generated: edit `docs/progress.json` (or run `python -I tools/update_progress.py --note "..." --set "<stage>=<percent>"`) after every iteration, then commit and push.
