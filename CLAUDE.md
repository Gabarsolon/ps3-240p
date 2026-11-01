# CLAUDE.md: ps3-240p

Research project: native 240p/288p analog output on a PS3 (CFW Evilnat 4.93 + Cobra, slim CECH-25xx; fat models for comparison). Owner: Gabarsolon (hobbyist, own console).

## Rules
- Never contact or write to the PS3 unless the owner asks for a specific read-only step. No installers, no payloads.
- Sony-derived files (firmware, PUPs, decrypted ELFs, decompiler output) stay in `local_only/` (gitignored). Docs hold addresses and short descriptions only; no instruction listings or code dumps from Sony binaries. This repository is PUBLIC.
- Label every claim PROVEN (checked in a binary or source by you) / INFERRED / CLAIMED (a source says so) / UNKNOWN, and cite URL + quote or file + address. Unsourced claims are not acceptable.
- Tools (this PC): Ghidra 12.1.3 `C:\ghidra_12.1.3_PUBLIC`, JDK 21, pyghidra (not capstone), one JVM at a time with `-Xmx2500m`. Firmware images and unpack tools live in the sibling project `<ps2-netemu-disc workspace>` (read only from here): `local_only/fw493/lv1.elf`, `local_only/fw355/out/*.elf`, `tools/core_os_extract.py`, `tools/pup_extract.py`, `tools/p0h_*.py`, `tools/p0i_*.py`.
- Short answers; the owner types casually.
