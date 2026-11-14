# Ordinary-HEN alternative display paths

2026-11-14. This report records completed offline inspection only.
**PROVEN** means checked source or binary; **INFERRED** marks an
interpretation; **UNKNOWN** marks missing validation. No console was
contacted for this investigation. No ordinary-HEN 240p/288p route was
established. Investigation stopped without attempting to change restricted
mapping permissions.

## Findings

| Lead | Completed finding | Limit |
|---|---|---|
| Normal native video selection | **PROVEN:** the inspected service selects existing timing records; its ordinary request does not supply arbitrary raster totals or sync edges | **UNKNOWN:** any different permitted low-line path |
| Native AV-debug wrapper | **PROVEN:** VSH can serialize command `0x00050001`, but the inspected retail receiver dispatcher has no matching handler | The wrapper is not evidence of a working custom-timing or state-query API |
| Direct color-space command | **PROVEN:** this path forwards a small fixed payload through the encoder transport | No complete raster-and-encoder control path was established |
| RSX device mapping | **PROVEN:** a display-register mapping case exists, with separate LV1 restrictions | **UNKNOWN:** ordinary-HEN reachability; existence does not establish permission or a usable low-line implementation |
| Public gamma getter | **PROVEN:** returns one VSH-side scalar, with state-dependent selection | It does not return the native AV service's previous RGB exponent triple |
| Existing native PC presets | **PROVEN:** fixed PC timing profiles and a native VSH selection mechanism exist | PC presets do not establish low-line timing or HEN runtime compatibility |

## Normal requests and the AV-debug lead

**PROVEN (fresh binary inventory):** the 28 ordinary metadata/raster records
checked in the private 4.93 `sysmgr_ss.elf` contain no metadata height of
240/288 and no raster total of 262/263/312/313. Record bases are
`0x80039148` and `0x80038b28`. This narrow inventory is not a proof that
every possible firmware path is unavailable. In particular, the current
project's field-fetch candidates retain 480/576 source heights; framebuffer
height alone does not classify the electrical output.
Sources: private `inventory.py:native_profile_lowline_check`;
`app/source/timing_payloads.h`; [existing AV trace](LV1_AV_TRACE.md).

**PROVEN (request/source distinction):** Sony/Linux's standard mode request
selects a VID and surface parameters. The checked packet structures do not
contain arbitrary raster totals, porches or clock records.
Sources: [Sony/Linux AV header](https://github.com/torvalds/linux/blob/v6.12/arch/powerpc/include/asm/ps3av.h),
[mode packet builder](https://github.com/torvalds/linux/blob/v6.12/drivers/ps3/ps3av_cmd.c).
**INFERRED:** changing only a surface height or exposing an additional fixed
preset does not supply the missing low-line timing control.

**PROVEN (private VSH binary):** constructor `0x5d754c` creates the AV-debug
command object with CID `0x00050001`. Its serializer at `0x5dffbc` sends a
caller-supplied payload; reply copying is at `0x5dff5c`.
**PROVEN (private AV-service binary):** receiver dispatch `0x8000d460`
contains no matching command case and returns no handler for that CID.
Consequently, the presence of the VSH debug wrapper is insufficient to
establish a functioning retail display-control or snapshot API.
Evidence: private `vsh_5dffbc.c`, `vsh_5dff5c.c`,
`sysmgr_8000d460.c` and the corresponding bounded listings.

**PROVEN (private binaries):** the direct-color-space serializer
`0x5deecc` constructs a fixed request. Receiver `0x8001121c` forwards four
caller bytes in a fixed encoder message through `0x80010f94`.
**UNKNOWN:** a complete low-line mechanism through this command. The
checked path does not itself expose the RSX raster writer or establish
arbitrary encoder-register access.

## Restricted RSX mapping

**PROVEN (private kernel and LV1 binaries):** LV2 syscall 675 resolves to
`0x80000000000e96ac`; its mapping helper `0x80000000000e9178` invokes
hypervisor call 212. The inspected registration store at `0x200918`
binds that call to handler `0x20db58`, which delegates device-ID policy
to `0x20b4b0`.

**PROVEN (bounded policy inspection):** the policy contains a mapping case
for a display-register window, and separate restrictions apply to the
extended device IDs. This is an additional permission boundary beyond
having LV2 process access. **UNKNOWN:** whether an ordinary-HEN installation
can use the relevant mapping through its existing permitted interfaces.
No mapping, register read/write, permission change or console-side probe was
performed in this investigation.

**INFERRED:** this evidence narrows a research lead; it does not establish a
working ordinary-HEN route. Raster access would also leave encoder behavior,
state ownership, exact restoration and game transitions to be established.
The report supplies no procedure for altering the restrictions.
Evidence: private `kernel_80000000000e96ac.c`,
`kernel_80000000000e9178.asm`, `lv1_20db58.c`,
`lv1_20b4b0.c/.asm`, `inventory.json:gpu_registration`.

## Gamma snapshot/readback limitation

**PROVEN (private module):** `cellVideoOutGetGamma` NID `0x655a0364`
is at module-relative code address `0x2050`; `cellVideoOutSetGamma`
NID `0xc7020f62` is at `0x216c`. They use VSH RPCs `0x4b0c` and
`0x4b0b`. The inspected setter bounds are approximately 0.8–1.2.
Evidence: private `inventory.json:avconf_exports/public_gamma_bounds`,
`avconf_2050.c`, `avconf_216c.c` and their bounded listings.

**PROVEN (private VSH binary):** getter handler `0x124020` selects either
a scalar from a VSH object or a fallback value according to a returned
state byte. The exact meaning of that byte was not validated in this pass.
Setter handler `0x1267b8` calls `0x43e238`, which updates the
object's scalar and marks a VSH dirty flag. The dirty-state manager at
`0x43e770` dispatches subsequent processing. These findings do not establish
that the scalar is an authoritative readback of every native analog curve.
Evidence: private `vsh_124020.c`, `vsh_1267b8.c`,
`vsh_43e238.c`, `vsh_43e770.c` and their bounded listings.

**PROVEN (private AV service and project implementation):** the native LUT
path used by CRT Toolset has three channel exponents. Receiver
`0x8000ca54`, command `0x0100000c`, invokes the setter at `0x8002582c`;
this handler returns status, not the previous curve. The current helper's
independent snapshot/readback still reads the AV-service state through LV1.
Sources: private `sysmgr_8000ca54.c`, `sysmgr_8002582c.c`;
`plugin/vsh_gamma.c:read_exponents/vsh_gamma_apply`; [gamma evidence](GAMMA.md).

**INFERRED:** the public scalar getter cannot simply replace that
three-channel snapshot. **UNKNOWN:** a permitted equivalent native query
on ordinary HEN. Global gamma support remains unestablished; dropping the
snapshot, assuming neutral RGB or treating a setter's success status as
independent readback would not resolve this limitation.

## Native PC presets remain a separate opportunity

**PROVEN (project source and private profile inventory):** the fixed
1280×768, 1280×1024 and 1920×1200 profiles have existing timing/clock
records. The native preset prototype uses VSH process interfaces rather
than the low-line LV1 timing patcher. Sources:
[native preset investigation](DISPLAY_PRESETS_INVESTIGATION.md),
`experiments/display_probe/native.c`, `plugin/vsh_video.c`.
**UNKNOWN:** actual ordinary-HEN compatibility. Native PC-preset results
on CFW do not establish 240p/288p capability.

**PROVEN (current implementation):** the isolated public diagnostic can
exercise getters without the HEN/LV1 transport; the separate HEN candidate
keeps LV1 capability explicitly untested. See
[diagnostic scope and validation](HEN_DIAGNOSTIC.md).
**INFERRED next supported work:** validate the isolated diagnostic and
app-local calibration features on an actual HEN system. Keep low-line and
global-gamma support unadvertised until their missing control/readback
requirements are independently established.

## Reproducibility and limits

**PROVEN (input identity):** SHA-256 values checked during this investigation:

| Private input | SHA-256 |
|---|---|
| 4.93 VSH | `2368923a20c7ed7f4927dbcad8854e15095d0d1cabad70c6531b76e36c9cfb17` |
| 4.93 sysmgr_ss | `07c0bd2a2403f65b6d741e11a15106114365cb78165833bac726476f671b64c9` |
| libsysutil_avconf_ext PRX | `02591b5c8d68ec4d1afdd93c584076cc9d82ccd84e50270d918aa6de1ae2221e` |
| 4.93 LV2 input | `d7f9d4de3683a18bafb9669b10413cb5580bd38f1045b1021b98bdcf625d6121` |
| 4.93 LV1 input | `e8d0b532adf6ba6c6c1ea40a5c5b86407abe11ea7a3ab8624ca5b09d3e4c3d6b` |

Private scripts, inventories, decompilation and bounded listings remain in
ignored `local_only/hen_alternative_paths/`. Initial gamma traces at the
incorrect PRX addresses `0x12050/0x1216c` were invalid and are excluded from
the findings; the corrected module-relative traces are cited above. An
unbounded Ghidra function-body discovery was stopped and replaced with
explicit ranges. No inference relies on that incomplete run.

**UNKNOWN:** correspondence of every distributed HFW/HEN build to these
images, HEN runtime results, a complete ordinary-HEN low-line path, and a
native substitute for the global RGB snapshot. This report establishes
neither working full support nor universal impossibility.
