# Background enforcer correction

**PROVEN (owner-requested log retrieval):** ordinary PAL timing activation
succeeded: the analog cache was 576 active / 624 total lines. Background
enforcement nevertheless failed because the app tried to unload an existing
helper using opcode `0x1ee8`, ignored that failure, then tried to load into
the same occupied slot. The load returned `0x80010011`, and the enable flag
remained absent. Private capture:
`local_only/activation/pal288-enforcer-20261113-204541/`.

**PROVEN (upstream Cobra interface):** the unload opcode is `0x364f`.
Source: [webMAN Cobra syscall8 definitions](https://github.com/aldostools/webMAN-MOD/blob/master/cobra/syscall8.h).

**PROVEN (development source):** enabling now reuses the same resident helper
when its deployed binary is unchanged. Replacement checks unload results and
rescans slots before loading. Entire name/path buffers are cleared because
PS3MAPI copies need not include a terminator; this fixes misleading names such
as `WWWD240p` and path suffixes from previous slot reads. Stock-mode choices
disarm the worker and retain it for reuse. Sources:
`app/source/plugin_lifecycle.c`, `app/source/main.c`,
`tests/test_plugin_lifecycle.c`.

## Native video calls

**PROVEN (SDK headers and firmware comparison):** the former helper's raw
syscalls 107/108 are condition-variable wait/signal operations, not video
configure/state calls. A running poll loop therefore did not establish
working background video control. Source: PS3DK 0.26.0 `sys/cond.h`;
the old `plugin/vsh_240p.c`. Earlier helper-start/armed claims establish only
thread/flag behavior, not a demonstrated display correction.

**PROVEN (private 4.93 VSH analysis and live read-only probe):** genuine
interfaces are available through export NIDs:

| Library / NID | Purpose |
|---|---|
| `xsetting / 0x4712f276`, method offset `0x0c` | Read native output state |
| `paf / 0x40410f89` | Read active display flags/mode/format/pitch |
| `vshmain / 0x3aecdd23` | Map a supported SD mode to display parameters |
| `paf / 0xd8af2727` | Submit the display change |

These exports are resolved at runtime; no VSH physical address is assumed.
Source: `plugin/vsh_video.c`. Private evidence:
`local_only/activation/vsh_exports_parsed.json`,
`xsetting4712_methods.json`, `vsh_extra_126e18.c`, and
`native-video-probe/result.log`.

**PROVEN (native namespace):** the observed native scan value for logical
interlace is 1; the app's public API uses 0. The corrected worker converts
that value rather than mixing namespaces. The native platform path retains
the current surface's format and pitch, and accepts only SD source/target
IDs. It refuses HD, preparing output, invalid pitch, and failed queries/maps.
Sources: `plugin/vsh_video.c`, `plugin/vsh_240p.c`, `tests/test_vsh_video.c`.

**PROVEN (six-second console experiment):** the native switch returned 0,
the state changed from ID 4 to 5, and the analog raster became
`0x0000023f0000026f` (576/624). The probe then restored ID 4 with a successful
readback. **CLAIMED (owner):** the timed switch worked. Private evidence:
`local_only/activation/native-video-probe/switch2-result.log`.

## Corrected build

**PROVEN (dev6 build/deployment):** all three installed app files and the
runtime helper were read back byte-for-byte against the finalized package.
Package version 01.35, SHA-256
`bbde9ad97f22929ae78ac0e2c3de8caf8b243bc581140d1ea0387cc5665e7927`.
The helper now records native readback/errors and creates a readiness marker
after binding its interfaces and reaching the poll loop. The app verifies
that marker before arming enforcement. Sources: `plugin/vsh_240p.c`,
`app/source/main.c`; private `local_only/activation/enforcer-native-dev6/`.

**PROVEN (old resident modules):** even the correct unload command returned
`0x80010005` for the loaded diagnostic module, which remained listed. Its
replacement did not run until loaded as a distinct probe. **UNKNOWN:** the
underlying stop/unload failure for these earlier artifacts. One reboot is
required to clear the old modules before validating the corrected worker.

**PROVEN (offline checks):** lifecycle reuse/replacement/failure tests,
native display guards and format/pitch tests, existing SD timing/rollback/UI
checks, 19 startup/menu/idle scenarios, the compiled helper ABI audit, and all
54 stock-target comparisons against private 4.93 firmware pass.

## Automatic correction verified

**CLAIMED (owner after reboot/dev6 enable):** 288p stays active at XMB.
**PROVEN (fresh app/helper logs and aligned hardware read):** the 9888-byte
native helper loaded in slot 2, bound successfully, created its readiness
marker, and armed with target 5. After app exit, native readback first showed
mode 5, then a fallback to mode 4. The worker logged `correct` and
`correct_sent` with return code 0; the next readback was mode 5.
The analog raster independently read `0x0000023f0000026f` (576/624), with
both armed/ready flags present and the app-active flag absent.

Private evidence: `local_only/activation/enforcer-native-dev6/`,
`verified-ps3_240p.log`, `verified-vsh_240p.log`, and `verified-xmb.json`.
This establishes an actual background correction, beyond successful table
writes, plugin loading, or thread liveness.

**CLAIMED (owner):** enabling 288p enforcement again in the same boot succeeds.
**PROVEN (second app log):** `Enforcer reused slot=2 target=5` is recorded;
there is no attempt to reload the occupied slot. Private evidence:
`local_only/activation/enforcer-native-dev6/reused-ps3_240p.log` and
`reused-vsh_240p.log`.

**CLAIMED (owner after the requested PAL PS1 check):** the game/PS-menu/return-XMB
flow works perfectly. The specific game was not named. **UNKNOWN:** extended
stability and broader PAL game/core coverage.
