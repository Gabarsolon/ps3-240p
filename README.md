# ps3-240p

Question: can a PS3 (CFW, Evilnat 4.93 + Cobra) output native 240p (NTSC) / 288p (PAL) over its analog AV multi-out, or is 480i/576i a hard floor?

**Status: [verification pass complete — research and static analysis only](docs/RESEARCH.md).** Supported analog modes stop at 480i/576i; checked LV1 AV-manager tables supply standard raster profiles. Native 240p/288p remains unproven, and an encoder hardware prohibition is not established. Nothing was built or run on a console. The unverified [original AI draft](docs/RESEARCH_agy_draft.md) remains untouched as history.

The report corrects hypercall/function mappings, AV packet layout, mode-ID namespaces, netemu profiles and the proposed RSX register recipe. The [offline LV1 AV-manager trace](docs/LV1_AV_TRACE.md) follows timing-table consumers, encoder control and emulator mode requests; [backward-compatible PS2 and PS1 emulator video mode analysis](docs/BC_PS1_EMULATOR_VIDEO.md) examines low-line recognition and output selection; the [read-only LV1 anchor-peek experiment plan](docs/EXPERIMENT_lv1_anchor_peek.md) details memory anchor verification; the [LV1 sysmgr_ss runtime load address analysis](docs/LV1_SYSMGR_SS_LOADADDR.md) traces hypervisor module loading and staged address resolution. No console access is authorized by this project status.

Rules: own console, no firmware or game files in this repository (Sony-derived files stay in a local, gitignored `local_only/`), research and static analysis only unless the owner says otherwise.

Related: [ps2-netemu-disc](https://github.com/Gabarsolon/ps2-netemu-disc) (private) shares the firmware unpacking and LV1 analysis tooling this research reuses.
