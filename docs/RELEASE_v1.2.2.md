# v1.2.2 - Full-frame Mode 4 and RetroArch SD setup

Fixes the cropped/stretched Mode-4 picture by matching full 480-line source-field fetching to a 522-line frame. Normal enable no longer applies the unsuccessful scaler experiment. Timing edits and the helper remain runtime-only; enable again after reboot.

## Owner-tested results

On the owner's Evilnat 4.93 + Cobra / AV Multi Out setup:

- Complete picture in the switcher and XMB.
- PS1 games work with `ps1_emu.self`.
- RetroArch PSX-Place CE 1.10.0 runs Super Mario All-Stars, with a correctly sized picture and working PS menu, after correcting its saved display settings to 720x480.

These are owner-reported visual/functionality results. Electrical progressive-sync/field parity remains unmeasured, PAL geometry remains unvalidated, and broader game/core compatibility is not established. [Detailed evidence](https://github.com/Gabarsolon/ps3-240p/blob/v1.2.2/docs/GEOMETRY_STATUS_2026-11-13.md).

## Assets and use

- **PS3-240P.pkg**: tested v1.2.2 package, SFO version 01.22, 195,968 bytes.
- **package_manifest.json**: package/member sizes and SHA-256 hashes.
- **SHA256SUMS.txt**: release asset checksums.
- **retroarch-sd-settings.cfg**: three settings to merge into an existing RetroArch config; do not replace the whole config with this file. [Instructions](https://github.com/Gabarsolon/ps3-240p/blob/v1.2.2/docs/RETROARCH_SD.md).

Install the PKG through XMB Package Manager. With AV Multi Out selected, launch the switcher, choose the first enable option, then return to XMB. Reboot first if earlier experiments remain in RAM. The background helper corrects Mode-5 fallbacks to Mode 4; the PAL test option is still experimental.

## Validation and repository changes

Patch/raster/UI tests, 17 startup scenarios, the compiled helper audit, all 44 private-firmware target comparisons, and finalized PKG member checks passed. The published PKG is the same artifact tested on the console, not a subsequent rebuild.

The release also includes corrected metadata diagnostics, firmware-audit coverage for both code targets, the progress section near the top of README, and the instruction-file rename from CLAUDE.md to AGENTS.md.

PKG SHA-256: `be8f1629db1ca818986d814c2082aa087ccc2a2dea4517b4bc004623448eae7e`.
