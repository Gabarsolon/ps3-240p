# CRT Toolset v1.4.4-rc1 — official SD release

The owner approved promoting this tested build to the official release on
2026-11-14. The existing tag, download names and in-app version retain
`rc1`; the package is unchanged. This promotion does not add the rc2 audio
logger or the separate experimental HDMI features.

## What changes from v1.4.3

**PROVEN (implementation):** fixes the fixed-PID lookup behind this error:

```text
PATCH ERROR: 4.93 sysmgr PID 10 not found; check CFW syscalls
```

Timing patches and gamma now discover the supported `sysmgr` across all
32 process slots. Discovery verifies immutable service identity, rejects
ambiguous matches, and rechecks process/page mappings. Existing checked
writes and rollback remain in place.
[Implementation and validation](https://github.com/Gabarsolon/ps3-240p/blob/main/docs/SYSMGR_DISCOVERY.md).

**PROVEN (supplied external log):** the affected CECH-2004A setup uses PID 9
and this build resolves all 54 timing targets. **CLAIMED (tester):** timed
240p recovery and background enforcement work in XMB and the app.
**CLAIMED (owner):** the same package also works on the owner's CRT setup.
[External result](https://github.com/Gabarsolon/ps3-240p/blob/main/docs/CECH2004A_LOOKUP_REPORT.md).

**PROVEN (package scope):** includes SD 240p/288p, background enforcement,
stock 480i/576i selection and analog gamma. Title ID is `PS240P001`, package
version **01.44**, menu version **v1.4.4-rc1**. Source: the checked package
manifest and `tools/build_ps3_app.py --pid-compatibility`.
The supported timing backend remains **4.93 CFW + Cobra**. Ordinary HEN and
HDMI 240p/288p are not supported by this release; experimental HDMI 540p/PC
presets remain separate development builds.

## Installation

1. Exit CRT Toolset, install **CRT-Toolset-1.4.4-rc1.pkg** through XMB Package
   Manager, then reboot normally once to clear an older resident helper.
2. Use your analog SD/CRT connection with the usual CFW/Cobra syscalls
   enabled. Confirm **v1.4.4-rc1** in the menu.
3. Start with **Recheck timing memory**, then the timed **Test 240p** or
   **Test PAL 288p** before enabling background enforcement.

If discovery fails, stop and retain `/dev_hdd0/tmp/ps3_240p.log`.
For enforcement issues, also retain `/dev_hdd0/tmp/vsh_240p.log`.
[Gamma and TV calibration guide](https://github.com/Gabarsolon/ps3-240p/blob/main/docs/CRT_CALIBRATION.md).

## PS1 compatibility note

**CLAIMED (tester):** `ps1_emu` produced lower-pitched/crackling audio and a
zoomed/cropped picture on their setup. Switching the launcher to
**`ps1_netemu` resolved both**, including Toy Story 2, Battle Hunter and
Spider-Man 2000. Try that alternative if you encounter these symptoms.
This is a reported workaround; `ps1_emu` is not established as universally
incompatible. Exact cause, integer scaling and latency remain **UNKNOWN**.
[Report, emulator evidence and startup-path caveat](https://github.com/Gabarsolon/ps3-240p/blob/main/docs/PS1_AUDIO_REPORT.md#tester-workaround-select-ps1_netemu-2026-11-14).

## Validation and download integrity

**PROVEN (prior checks):** all 32 relocated PIDs, SD apply/exact undo,
gamma with a relocated service, decoys, duplicate identities, denied reads,
malformed lists and changing mappings passed regression checks. Private
4.93 identities, helper loader behavior and decrypted package members were
also checked. Sources: the linked discovery evidence and
`tools/test_app.py --firmware`.

**PROVEN (promotion check):** all three existing release assets were
downloaded and verified against their GitHub SHA-256 digests. The tested
PKG remains **255568 bytes**, SHA-256:

```text
e35062e49bdb6a3cb785442c49d5e8e855389d44d92590fd2bfa3aba5614165e
```

`SHA256SUMS.txt` and `package_manifest.json` accompany it. Local publication
evidence: ignored `local_only/releases/promote-rc1-20261114/`.
