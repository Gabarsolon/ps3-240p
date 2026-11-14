# CRT Toolset v1.4.4-rc1 — sysmgr PID compatibility test

**Post-publication update (2026-11-14):** **PROVEN in the supplied log:** the
affected tester's sysmgr is PID 9 and this RC resolves all 54 targets.
**CLAIMED (tester):** 240p works in XMB/app, timed recovery and background
correction. PS1 audio pitch/crackling and crop are separately reported and
unresolved; this remains a prerelease. See the
[updated model report](https://github.com/Gabarsolon/ps3-240p/blob/main/docs/CECH2004A_LOOKUP_REPORT.md)
and [audio investigation](https://github.com/Gabarsolon/ps3-240p/blob/main/docs/PS1_AUDIO_REPORT.md).
The original publication-time assessment below is historical.

This SD prerelease targets the v1.4.3 error:

```text
PATCH ERROR: 4.93 sysmgr PID 10 not found; check CFW syscalls
```

**PROVEN (implementation):** timing patches and gamma readback now discover
the supported `sysmgr` across all 32 process slots instead of assuming PID 10.
Discovery verifies its immutable service fingerprint, checks for ambiguous
matches and rechecks the process table/mappings before returning. Existing
patch-byte checks and rollback remain in place. Logs now distinguish zero
process pointers, no verified service, duplicate identities and changed
snapshots. [Implementation and evidence](https://github.com/Gabarsolon/ps3-240p/blob/v1.4.4-rc1/docs/SYSMGR_DISCOVERY.md).

**CLAIMED (owner, 2026-11-14):** after installing this exact RC package for
CRT testing, the owner reported: “I've tested it and it works.” This is an
owner-console result, not confirmation from the affected CECH-2004A.

**UNKNOWN:** whether the external CECH-2004A failure is caused by a shifted
PID and whether this candidate fixes that console. Other firmware layouts
and ordinary HEN support have not been added. This retains the supported
4.93 CFW/Cobra backend.

## Install and test

1. Exit CRT Toolset. Install **CRT-Toolset-1.4.4-rc1.pkg** through XMB Package
   Manager, then **reboot once** to clear an older resident helper.
2. Use your normal analog SD/CRT connection, with the usual CFW/Cobra
   syscalls enabled. Open CRT Toolset and confirm **v1.4.4-rc1** in its title.
3. Select **Recheck timing memory** first. If it fails, stop there and send
   `/dev_hdd0/tmp/ps3_240p.log`.
4. If memory is recognized, select the timed **Test 240p** or **Test PAL 288p**.
   Leave the controller untouched to check its automatic 15-second return.
5. Report the console model, firmware/Cobra versions, output cable, whether
   the picture works and whether recovery completes. Include the app log;
   `/dev_hdd0/tmp/vsh_240p.log` is also useful for helper/enforcement issues.

**PROVEN (package scope):** this updates title ID `PS240P001`, package
version **01.44**, with menu version **v1.4.4-rc1**. It is an SD compatibility
build and does **not** include the experimental HDMI 540p menu from
v1.5.0-dev19. Stable v1.4.3 and the separate HDMI prerelease remain available.
Sources: package manifest, `tools/build_ps3_app.py --pid-compatibility`.

## Validation and limits

**PROVEN:** synthetic regression tests cover every PID position 0–31,
SD apply/exact undo, gamma with a relocated service, decoys, duplicate
identities, unavailable reads, malformed lists, MMIO refusal and changing
snapshots. The complete host suite, private 4.93 firmware fingerprint/target
checks, helper loader contract and decrypted package-member checks passed.
The owner's native read-only lookup found one service at PID 10, 109 pages
and all 54 timing targets. See the linked evidence document for sources.

**UNKNOWN:** an earlier HDMI capture was black while this SD app was open,
despite responsive input and completed frame/log activity. Normal exit
returned to visible XMB. That capture issue remains unexplained; the later
owner-reported CRT success does not establish HDMI behavior or other-model
compatibility.

## Download integrity

**PROVEN (artifact identity):** this is the existing installed/tested package,
without a new binary rebuild. Source: the local package manifest and checked
installation report, recorded in the linked evidence document.

`CRT-Toolset-1.4.4-rc1.pkg` — **255568 bytes**

SHA-256:

```text
e35062e49bdb6a3cb785442c49d5e8e855389d44d92590fd2bfa3aba5614165e
```

`SHA256SUMS.txt` and `package_manifest.json` accompany the package.
