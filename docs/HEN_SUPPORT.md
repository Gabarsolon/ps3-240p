# CFW and HEN compatibility

The later [native video, gamma and mapping investigation](HEN_ALTERNATIVE_PATHS.md)
found no complete ordinary-HEN low-line route. Native PC presets are a
separate possible feature, with actual HEN execution still untested.

See the [concrete HEN port plan](HEN_PORT_PLAN.md) for the 2026-11-14 review,
capability separation, diagnostic requirements and hardware-assisted scope.
**PROVEN (upstream source):** this HEN revision's LV1 peek helper invokes a
poke-based precheck. Do not treat the current app's LV1 inspection as a
strictly read-only HEN diagnostic; the first diagnostic should issue no LV1
operations. Source and exact limitations are documented in the port plan.

**PROVEN (project source):** the timing patcher needs working LV1 reads and
writes to the live `sysmgr_ss` timing/encoder records. It tries PS3MAPI LV1
peek/poke, then CFW syscalls 8/9, and requires the supported 4.93 memory layout
and recognized record values before writing. Cobra-style plugin loading alone
does not supply that LV1 access. Sources: `app/source/main.c:inspect`,
`app/source/lv1_patch.c:patch_resolve`.

**CLAIMED (owner tests):** v1.2.2 works on Evilnat 4.93 CEX + Cobra 8.5,
including XMB, PS1 `ps1_emu.self`, and the tested RetroArch configuration.
See [the test checkpoint](GEOMETRY_STATUS_2026-11-13.md).
**UNKNOWN:** other CFW/firmware/model combinations until separately checked.

**PROVEN (upstream source inspected 2026-11-13):** PS3HEN's LV1 peek/poke
PS3MAPI cases are marked `BadWDSD only`. The additional installation path
checks `BadWDSD_IsExploited` before installing hypervisor calls. This does not
provide ordinary HEN installations with general LV1 read/write access.
Source: PS3HEN commit `6c529455261db97a0f9028fe96db522a97fd5ccc`,
[`payload/main.c`](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/main.c),
`BadWDSD_TryInstallHvcall` and `PS3MAPI_OPCODE_LV1_PEEK/POKE` cases.

**INFERRED:** the current low-line implementation is CFW-only for normal
users. A normal HEN port needs a different permitted video-control path or
additional LV1 capability; rebuilding the same PKG does not solve it.
**UNKNOWN:** compatibility on a separately hardware-exploited HEN system.
No such hardware/firmware modification is implemented or tested here.
The upstream [BadWDSD project](https://github.com/aomsin2526/BadWDSD)
describes it as a hardware modchip, distinct from a normal HEN installation.

**PROVEN (API/source distinction):** ordinary 480i/576i configuration uses
the public video API; it does not inherently require an LV1 timing patch.
**UNKNOWN:** HEN support for this app's combined restore/saved-default actions:
the current menu still requires the CFW timing-record preflight, and saving
defaults also requires webMAN's named-key support. These stock modes should
not be advertised as tested HEN features.

## Reassessment after native gamma (2026-11-13)

**PROVEN (upstream identity):** Rechecking GitHub's current default branch
still resolves to `6c529455261db97a0f9028fe96db522a97fd5ccc`, the commit inspected
above. The README's newer changelog language about LV1 peek/poke must be read
with the implementation's hardware-exploit prerequisite. This review is of
that source revision, not a claim that every distributed HEN build matches it.

| Feature | Ordinary HEN assessment | Current implementation |
|---|---|---|
| Native 240p / 288p timing | **INFERRED: blocked with the present backend** | Requires live LV1 timing/encoder writes; no verified substitute found |
| Background SD correction | **INFERRED: its plugin mechanism is portable** | Native VSH calls do not themselves create low-line timing; it still needs the timing patch |
| Stock 480i / 576i live selection | **INFERRED: a practical port candidate** | Public API exists, but the current combined action first snapshots/restores LV1 timing RAM |
| Saved 480i / 576i default | **INFERRED: a practical port candidate** | Uses named XRegistry access through a suitable webMAN build; needs HEN verification |
| Native analog gamma | **INFERRED: a realistic separate port candidate** | Setter is a VSH AV call, but current guards/snapshot/readback still read LV1 |
| Value editor / calibration ramps / shader export | **PROVEN: app-local implementation has no LV1 dependency** | **UNKNOWN:** actual package and shader execution on HEN |

### What ordinary HEN does provide

**PROVEN (upstream source):** Its syscall-8 dispatcher implements process
memory reads (`PS3MAPI_OPCODE_GET_PROC_MEM`, 0x31), VSH plugin inspection
(0x47), and Cobra-style VSH plugin load/unload. The loader calls the VSH
process's normal PRX loading/start machinery. Those are the facilities our
native display helper uses to find and invoke VSH interfaces; VSH plugins
are not inherently CFW-only.

Sources at the pinned HEN revision:
[dispatcher](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/main.c),
[process-memory reader](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/ps3mapi_core.c),
[VSH plugin loader](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/modulespatch.c),
[opcode definitions](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/ps3mapi_core.h).

**PROVEN (project code):** `plugin/vsh_video.c` uses process-memory reads
and native xsetting/PAF/VSH calls, not LV1 pokes. The low-line timing change
is separately performed by `app/source/lv1_patch.c`. Porting the helper
alone therefore does not make a stock 480i/576i raster progressive.

### Gamma changes the porting opportunity

**PROVEN (project code and private firmware trace):** The actual gamma setter
in `plugin/vsh_gamma.c` calls the VSH AV LUT routine. It writes no LV1 memory
and no MMIO directly. However, `read_exponents` uses PS3MAPI LV1 peek to
validate the active analog format, save the prior RGB curve and check the
result. That is why the present gamma implementation still cannot be called
ordinary-HEN compatible. See [the native gamma evidence](GAMMA.md).

**INFERRED port plan:** Keep the setter and timed service, but establish a
verified native way to obtain the active output and previous three-channel
curve, and validate the setter's result, without those LV1 reads. Check the
HFW VSH signature/export binding and package/plugin loading on a real HEN
console. Native public gamma APIs must not be assumed to return the same
global curve modified by the private AV path without verifying that behavior.
Simply dropping the snapshot/readback checks would weaken restoration and
would not establish compatibility.

**INFERRED stock-mode port plan:** Split 480i/576i selection and saved-default
handling from the CFW timing restore/preflight. Detect capabilities before
entering an LV1-dependent path. Preserve explicit confirmation and the saved
setting readback/rollback behavior. **UNKNOWN:** results until tested on HEN.

### The hardware-assisted exception

**CLAIMED (exploit authors):**
[BadHTAB](https://github.com/aomsin2526/BadHTAB) combines software with an
RP2040/RAM-signal hardware glitch and requires soldering; its author marks it
obsolete in favor of BadWDSD. It is not a software-only HEN update.
[BadWDSD](https://github.com/aomsin2526/BadWDSD) is a modchip project that
can boot qCFW on supported later consoles. Its qCFW path is a different
environment from ordinary software-only HEN.

**INFERRED:** A hardware-assisted HEN environment with working LV1 read/write,
or qCFW with suitable Cobra support, could meet the fundamental timing-access
requirement. **UNKNOWN:** our 4.93 record identities, mappings, model/encoder
behavior, helper loading and recovery on those systems. This is a possible
validation target, not tested support or a recommendation to modify hardware.

**Current conclusion:** The earlier ordinary-HEN limitation still applies to
240p/288p with this backend. A partial HEN edition offering gamma, calibration
and stock SD selection is now a more credible development target. No console
was contacted and no HEN exploit, firmware installation or device modification
was performed for this review. Downloaded source snapshots are in ignored
`local_only/sources/hen-review/`.
