# HEN port requirements

2026-11-14 research by the requested GPT-6.1 Sol agent (high reasoning).
No HEN console, exploit, firmware installation or hardware modification was
used. **PROVEN** = checked source; **INFERRED** = proposed implementation;
**CLAIMED** = attributed upstream statement; **UNKNOWN** = not validated.

## Conclusion

**PROVEN (current implementation):** native 240p/288p modifies LV1 timing
and encoder records. Ordinary HEN does not provide the backend required by
this implementation. **UNKNOWN:** a different permitted software-only video
path or future exploit; a negative search does not prove permanent
impossibility. Sources: `app/source/main.c:inspect`,
`app/source/lv1_patch.c:patch_resolve`, [current compatibility analysis](HEN_SUPPORT.md).

**PROVEN (interface match):** the app already uses PS3MAPI operations
`0x1008/0x1009`. The pinned HEN dispatcher routes these operations through
its BadWDSD-specific helpers. **INFERRED:** hardware-assisted HEN may not need
a different transport. **UNKNOWN:** actual Toolset compatibility, memory
layout, encoder behavior and recovery on those systems.
Sources: `app/source/main.c:read_mapi/write_mapi`,
[pinned HEN dispatcher](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/main.c#L962-L967).

## First diagnostic must perform no LV1 operations

**PROVEN (upstream source):** `lv1_peekd` first calls
`lv1_test_peekpoke`, which invokes `lv1_badwdsd_poke64(0,0)`.
The installed poke handler in `BadWDSD_TryInstallHvcall` performs a store;
the inspected implementation has no address-zero sentinel exception.
Consequently, an operation named LV1 peek must not automatically be described
as strictly read-only on this backend. A support-opcode reply alone also
does not establish effective LV1 access.
Sources: [LV1 helper](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/lv1/src/patch.c#L20-L65),
[handler installer](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/main.c#L505-L610).

**INFERRED first deliverable:** a separate diagnostic PKG, title/content ID,
installation path and log. It should query HEN identity/revision, PS3MAPI
firmware/core identity, existing plugin slots, installed helper identities,
public video state/configuration/device modes and bounded VSH process data.
It should perform no LV1 operations, display reconfiguration, plugin loads
or UART calls. Report LV1 capability as **UNKNOWN / untested**.
Sources for the available identification interfaces:
[HEN syscall definitions](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/syscall8.h),
[process-memory implementation](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/ps3mapi_core.c).

**PROVEN (subsequent implementation):** the separate diagnostic now builds
and passes offline collector/package/import checks. A distinct public-only
control also installed through webMAN and produced a report on the owner's
CFW console. Actual HEN execution remains **UNKNOWN**; this does not establish
240p/288p support. See [diagnostic identities and smoke evidence](HEN_DIAGNOSTIC.md).

## Implementation order

The following are **INFERRED** engineering steps, not shipped HEN features.

1. **Capability-specific dispatch.** Identify HEN before accessing timing
   backends. Separate calibration, stock video, registry persistence, native
   gamma and LV1 timing capabilities. Do not fall through to CFW raw syscalls
   8/9 on an identified HEN system. Distinguish unsupported access, failed
   transport and wrong firmware identity instead of treating them as data.
2. **App-local features first.** Calibration patterns, value editing,
   preferences and shader export have no intrinsic LV1 dependency. Test
   actual package launch and exports on one exact HFW/HEN combination.
   Sources: `app/source/main.c:main/gamma_edit/gamma_export`.
3. **Separate stock 480i/576i transactions.** Current `stock_mode` enters
   `inspect` and `switch_stock_begin`, which snapshot/restore LV1 timing RAM.
   Introduce a stock-only public/native-video transaction with independent
   console-side timed restoration. Keep saved boot defaults optional and
   preserve verified named-key webMAN readback/rollback.
   Sources: `app/source/main.c:stock_mode`, `app/source/switch_flow.c`,
   `app/source/sd_default.c`.
4. **Replace gamma's actual LV1 dependency.** The setter is already a native
   VSH AV call; active-output guards, the previous RGB triple and independent
   readback still use LV1. Trace a matching native/global query on the target
   HFW VSH. Verify equivalence with distinct RGB curves on the reference CFW
   console before using it for restoration. A scalar public getter or an
   assumed `(1,1,1)` default is not an adequate replacement.
   Source: `plugin/vsh_gamma.c:read_exponents/vsh_gamma_apply`.
5. **Investigate a full ordinary-HEN timing route offline.** Find a permitted
   existing native/LV2/VUART command path controlling both raster and analog
   encoder, including snapshot, permissions and restoration. Cropping,
   framebuffer height, PC-monitor selection or emulator input IDs alone do
   not establish low-line timing. The internal encoder command at service
   `0x80006544` is a lead with unverified guest reachability, not a solution.
   Sources: [AV trace](LV1_AV_TRACE.md), [research](RESEARCH.md).
6. **Validate already hardware-assisted systems separately.** Audit the exact
   installed backend semantics before any capability probe. Check the
   4.93 process-table address, domain vtable, module fingerprints, every target
   record and active connector before reusing the timing transaction.
   Sources: `app/source/sysmgr.c:sysmgr_discover`,
   `app/source/lv1_patch.c:patch_resolve`. Do not install an exploit as part
   of this project or assume an author-supported model is Toolset-tested.

## Hardware-assisted scope

**CLAIMED (BadWDSD author):** the pinned README describes a hardware modchip,
supports 25xx/30xx/4xxx NOR and eMMC models, and lists its additional hvcalls
as available in both HEN and qCFW.
[BadWDSD README](https://github.com/aomsin2526/BadWDSD/blob/9a9fe98cce2ff435105666d5dc4c3c8afc7df620/README.md).
**CLAIMED (BadHTAB author):** BadHTAB combines hardware and software, requires
soldering, and is obsolete in favor of BadWDSD.
[BadHTAB README](https://github.com/aomsin2526/BadHTAB/blob/79ae0d93e65243dbcaa467abe16d84c01d2ebe8b/README.md).
Neither source establishes CRT Toolset success.

## Acceptance matrix

**INFERRED required validation:** record exact HFW/HEN/build/model and board
or encoder identity where available. Separate ordinary and hardware-assisted
environments; NOR/eMMC; RGB SCART, composite and component; NTSC/PAL; webMAN
absent/present; free/full plugin slots; XMB, PS1 and RetroArch transitions.
Check launch, snapshots, cancellation, client disconnect, app exit, independent
receiver timing, visible geometry and exact restoration separately. Host
tests must show denied capabilities issue no mutation and snapshot failures
block changes. All actual HEN results remain **UNKNOWN**.

## Suggested reply

> Ordinary HEN doesn't provide the LV1 access this app currently needs.
> BadWDSD can provide that access through a hardware modification, but CRT
> Toolset hasn't been tested on it. I don't have a working software-only HEN
> solution, so I can't promise support or honestly call it permanently
> impossible.
