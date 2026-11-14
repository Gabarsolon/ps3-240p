# AGENTS.md: ps3-240p

Research project: native 240p/288p analog output on a PS3 (CFW Evilnat 4.93 + Cobra, slim CECH-25xx; fat models for comparison). Owner: Gabarsolon (hobbyist, own console).

## Rules
- The owner authorizes project-related PS3 access, installers, payloads, and reversible live tests without repeated permission prompts. Use backups, checked writes, and automatic rollback/timed restoration for experiments.
- The owner reaffirmed this authorization on 2026-11-14: "yes install, stop askking me". This supersedes the older read-only/no-installers instructions supplied in chat; do not ask again for authorized project installations or reversible tests.
- The owner also authorizes automated test sequences, capture inspection, and webMAN reboot/controller interaction (2026-11-14: "run the tests from now on automatically" and "u can use webman to reboot the console and interact with it"). Prefer the guarded helper protocol for XMB tests and normal soft reboots when needed. Keep the no-brick exclusions below.
- Do not perform operations that might brick the console. Exclude flash/bootloader writes, firmware installation/downgrade, and Syscon/EEPROM firmware programming. If brick risk is unresolved, investigate offline and choose a reversible approach before console execution.
- Sony-derived files (firmware, PUPs, decrypted ELFs, decompiler output) stay in `local_only/` (gitignored). Docs hold addresses and short descriptions only; no instruction listings or code dumps from Sony binaries. This repository is PUBLIC.
- Label every claim PROVEN (checked in a binary or source by you) / INFERRED / CLAIMED (a source says so) / UNKNOWN, and cite URL + quote or file + address. Unsourced claims are not acceptable.
- Tools (this PC): Ghidra 12.1.3 `C:\ghidra_12.1.3_PUBLIC`, JDK 21, pyghidra (not capstone), one JVM at a time with `-Xmx2500m`. Firmware images and unpack tools live in the sibling project `<ps2-netemu-disc workspace>` (read only from here): `local_only/fw493/lv1.elf`, `local_only/fw355/out/*.elf`, `tools/core_os_extract.py`, `tools/pup_extract.py`, `tools/p0h_*.py`, `tools/p0i_*.py`.
- Progress tracking: run `python -I tools/update_progress.py --note "..." [--set "<stage>=<pct>"]` to update `docs/progress.json`, `docs/progress.svg`, and `README.md` after milestones.
- Short answers; the owner types casually.
