# ps3-240p

Question: can a PS3 (CFW, Evilnat 4.93 + Cobra) output native 240p (NTSC) / 288p (PAL) over its analog AV multi-out, or is 480i/576i a hard floor?

**Status: research only, nothing is built or tested.** The first draft in `docs/RESEARCH_agy_draft.md` was written by an AI helper and is **unverified**: it has no source links, contains at least one wrong number and some claims that could not be traced. It is kept as a starting sketch. A verification pass (every claim cited with a URL or a file and address, firmware statements re-checked against the binaries) will replace it with `docs/RESEARCH.md`.

What is already believed (still to be confirmed): the minimum mode exposed by the OS (`cellVideoOut`) and by the PS3 AV manager protocol is 480i/576i; the open question is which layer really enforces it: userland tables, the LV2 driver, the LV1 AV manager, the emulators, or the video encoder hardware.

Rules: own console, no firmware or game files in this repository (Sony-derived files stay in a local, gitignored `local_only/`), research and static analysis only unless the owner says otherwise.

Related: [ps2-netemu-disc](https://github.com/Gabarsolon/ps2-netemu-disc) (private) shares the firmware unpacking and LV1 analysis tooling this research reuses.
