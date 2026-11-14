# Isolated HEN diagnostic 0.1.0

**PROVEN (project source and offline checks):** this is a separate query-only
application. It collects API replies into its own JSON report and exits; it
has no graphics UI, timing patch, gamma setter or resident helper.
Sources: `experiments/hen_diagnostic/{main.c,diagnostic.c,native.c,video_imports.S}`.
**UNKNOWN:** actual installation, launch and query results on a HEN console.
No console was contacted while implementing or building this diagnostic.
The later CFW smoke result below uses the separate public-only variant.

## Public-only CFW smoke control

**PROVEN (compile-time variant and artifact checks):** the public-only
package compiles out the HEN collector's transports and `hd_native_raw`.
Its diagnostic objects contain zero syscall instructions. It calls only
the four public video getters, writes its own report and exits normally.
HEN identity, PS3MAPI, plugin slots, process reads and LV1 capability remain
explicitly **UNTESTED**. Sources: the `HD_PUBLIC_ONLY` branches in
`diagnostic.c`, `main.c`, `native.c`; `test_hen_diagnostic_artifact.py`.

| Public-only item | Value |
|---|---|
| XMB title | CRT Public Video Diagnostic |
| Title ID / installation directory | `CRTHEN002` / `/dev_hdd0/game/CRTHEN002` |
| Content ID | `UP0001-CRTHEN002_00-CRTPUBDIAG000001` |
| Package | `build/hen-diagnostic-public/deliverables/CRT-Public-Video-Diagnostic-0.1.0.pkg` |
| Manifest | `build/hen-diagnostic-public/deliverables/manifest.json` |
| Report | `/dev_hdd0/tmp/crt_hen_diagnostic_public.json` |
| Temporary report | `/dev_hdd0/tmp/crt_hen_diagnostic_public.pending` |

**PROVEN (owner-console smoke test, 2026-11-14):** webMAN's
`/install_ps3/dev_hdd0/packages/CRT-Public-Video-Diagnostic-0.1.0.pkg`
installed this distinct public-only package from idle XMB. XMB reported
completion, all four installed files matched decrypted package members,
and the staged package remained. One launch through `/play.ps3?CRTHEN002`
produced a complete JSON report and returned to a visibly readable XMB;
the diagnostic process was absent from the subsequent process listing.
The four public getters succeeded: state/configuration/device rc 0,
one HDMI device, current resolution ID 1 with pitch 8192, seven advertised
modes. HEN, PS3MAPI and LV1 fields remained UNTESTED; requested LV1
operations were zero. This validates the public getter imports, package,
report and exit path on the owner's CFW, not the HEN collector or HEN
compatibility.

Tested public-only PKG: **113120 bytes**, SHA-256
`7dd39575c1c90e4259c4a6cb6c825bf5099ac5accbd1381f57f18c1e52dcf10f`.
Evidence: ignored `local_only/display_presets/public-diagnostic-ui/`
`installed.json`, `install-webman.html`, `crt_hen_diagnostic_public.json`,
`processes-after.html` and `capture-20261114-184651/screen.jpg`.

```powershell
python -I tools/build_hen_diagnostic.py --public-only
python -I tests/test_hen_diagnostic.py --public-only
```

**PROVEN (host test):** even when supplied a working mock transport, the
public-only collector invokes it zero times. Its artifact evidence is kept
under `build/hen-diagnostic-public/`, separate from the HEN candidate below.

## Identity and files

| Item | Value |
|---|---|
| Title | CRT HEN Diagnostic |
| Title ID / installation directory | `CRTHEN001` / `/dev_hdd0/game/CRTHEN001` |
| Content ID | `UP0001-CRTHEN001_00-CRTHENDIAG000001` |
| Package | `build/hen-diagnostic/deliverables/CRT-HEN-Diagnostic-0.1.0.pkg` |
| Manifest | `build/hen-diagnostic/deliverables/manifest.json` |
| Report | `/dev_hdd0/tmp/crt_hen_diagnostic.json` |
| Temporary report | `/dev_hdd0/tmp/crt_hen_diagnostic.pending` |

**PROVEN (builder/package checks):** these identities differ from CRT
Toolset's `PS240P001`. The package contains only PARAM.SFO, ICON0.PNG,
USRDIR/EBOOT.BIN and USRDIR/DIAGNOSTIC.txt. It packages no plugin and does
not replace the existing app or helper. Source: `tools/build_hen_diagnostic.py`;
proof: `tests/test_hen_diagnostic_artifact.py`.

## Run and interpret

**INFERRED run procedure; runtime remains UNKNOWN:** on an already enabled
HEN console matching the audited protocol, install the diagnostic through
the normal XMB package manager, then launch **CRT HEN Diagnostic** once.
The program writes its report and returns normally. Retrieve the JSON using
an existing file-transfer facility. Preserve a previous diagnostic report
before another run if its results are needed; successful runs replace this
diagnostic's own report. A failed publication may leave `.pending` instead.
No PC connection is needed while the diagnostic runs.

**PROVEN (collector/report source):** the report includes:

- HEN identity and revision; PS3MAPI core/minimum/firmware version replies.
- Existing VSH plugin slots 1–6: return codes, module names and filename
  basenames. Full arbitrary paths are omitted to avoid home/account paths.
- Primary public video state/configuration and device 0's declared modes,
  capped at the API structure's 32 entries. No HDCP/KSV or EDID bytes.
- One fixed 16-byte VSH ELF-identification read, only after a successful
  process-list query and exactly one process matching `vsh` or
  `main_vsh.self`. The raw bytes and other process names are not logged.
- LV1 capability **UNTESTED**, regardless of the other replies.

**PROVEN (report semantics):** a null query return code means no query was
issued. `vsh_read.status` distinguishes `UNTESTED`, `READ_FAILED`,
`HEADER_MISMATCH` and `HEADER_MATCH`. Firmware and core values are raw hex
replies, not a claim of supported timing layout. Slot filenames identify
the loaded module's reported name, not its binary hash or helper ABI.
Source: `diagnostic.c:hd_collect/hd_write_json`.

**UNKNOWN:** independent receiver timing, global gamma, 240p/288p support,
model/encoder compatibility and game behavior. Public mode metadata and an
ELF header match do not establish any of these.

## Closed transport contract

**PROVEN (source and host tests):** the diagnostic permits only these
syscall-8 selectors through `hd_checked_call`:

| Selector | Purpose / restriction |
|---|---|
| `0x1337` | HEN identity, all remaining arguments zero |
| `0x1339` | HEN revision, only after matching HEN identity |
| `0x7777 / 0x11` | PS3MAPI core identity, only after matching HEN |
| `0x7777 / 0x12, 0x14` | Minimum core / firmware identity |
| `0x7777 / 0x47` | Plugin information, slots 1–6 only |
| `0x7777 / 0x21, 0x22` | Process list / name, used only for exact VSH selection |
| `0x7777 / 0x31` | Selected VSH PID, address `0x10000`, exactly 16 bytes |

**PROVEN (gating):** all PS3MAPI queries after core identity require a
nonzero version-shaped core reply. No fallback transport is attempted.
There is no LV1 peek/poke, LV2 poke, plugin load/unload, UART command, direct
hypervisor instruction, GCM initialization, display reconfiguration,
registry access, boot-list edit or network request in the diagnostic code.
Only its own two report paths are opened for writing.
Sources: `diagnostic.c:hd_checked_call`, `native.c`, `main.c`;
tests: `test_hen_diagnostic.py`, `test_hen_diagnostic_artifact.py`.

**PROVEN (upstream source):** the inspected PS3HEN dispatcher implements
these query selectors. Its process-name query passes `get_process_name`
through without normalizing the spelling. Sources at commit
`6c529455261db97a0f9028fe96db522a97fd5ccc`:
[dispatcher](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/main.c),
[selector definitions](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/syscall8.h),
[process reader](https://github.com/PS3Xploit/PS3HEN/blob/6c529455261db97a0f9028fe96db522a97fd5ccc/payload/ps3mapi_core.c#L94-L103).
The two exact name aliases are conservative candidates; a different name
causes the read to be skipped.

**UNKNOWN (other environments):** this contract is scoped to matching HEN
dispatch semantics. An arbitrary CFW/modified syscall-8 implementation can
interpret an unfamiliar selector differently, even the initial HEN identity
query. Stopping after a failed identity does not prove that first query is
side-effect-free on every other payload. This is a HEN diagnostic, not a
universal CFW capability probe. See [the port plan](HEN_PORT_PLAN.md) for the
upstream LV1-peek precheck that this executable deliberately never calls.

**PROVEN (CFW source audit):** official Cobra 8.5's 4.93 CEX and Evilnat PEX
dispatchers at commit `608ac4aa052e69ad50e84d9509b8010257b7f331` do not have a
HEN-identity case. Their default path can call `lv1_peekd(function)`, including
for selector `0x1337`. Therefore the ordinary HEN build is not a zero-LV1
control on that CFW. A public-getter-only control must omit this selector.
Sources: [CEX dispatcher](https://github.com/xXEvilnatXx/Cobra-PS3/blob/608ac4aa052e69ad50e84d9509b8010257b7f331/8.5/4.93/CEX/SRC/stage2/main.c#L1065-L1094),
[Evilnat PEX dispatcher](https://github.com/xXEvilnatXx/Cobra-PS3/blob/608ac4aa052e69ad50e84d9509b8010257b7f331/8.5/4.93/EVILNAT/PEX/SRC/stage2/main.c).
The HEN and Cobra PS3MAPI firmware-type strings both identify CEX COBRA in
the inspected headers, so that string cannot establish which dispatcher is
active. Sources: the same pinned repositories' `ps3mapi_core.h` files.

## Build and offline proof

**PROVEN (implementation):** builds stay under `build/hen-diagnostic/`.
The existing SDK tools and `tools/pkg_inspect.py` are reused read-only;
existing application/plugin builders are not modified.

```powershell
python -I tools/build_hen_diagnostic.py
python -I tests/test_hen_diagnostic.py
```

**PROVEN (offline validation):** 13 host scenarios cover denied transports,
identity/API failures, absent or ambiguous VSH, both exact VSH aliases,
rejection of substring matches, duplicate PIDs, memory errors, invalid ELF
identification, JSON escaping/path redaction and the 32-mode limit. The
compiled artifact check verifies package/content/title identities, decrypted
members, NPDRM content ID, four public video-getter imports, denied setter
symbols, a single direct syscall-8 transport site, no direct hypervisor call
and no networking/mutating references from the diagnostic objects.

**PROVEN (import isolation):** the installed SDK's monolithic sysutil stub
also imports video setters. `video_imports.S` instead declares only these
public getter NIDs: GetState `0x887572d5`, GetConfiguration `0x15b0b0cd`,
GetNumberOfDevice `0x75bbb672`, GetDeviceInfo `0x1e930eef`. It uses the SDK's
compact function-descriptor ABI. Artifact tests require exactly this
cellSysutil import table. SDK/newlib runtime symbols may include unused
socket helpers; the diagnostic objects reference none of them. This is
static/offline proof, not console execution proof.

**PROVEN (audit implementation):** symbol and undefined-reference checks read
ELF `.symtab`/`.dynsym` directly with pyelftools. They do not launch SDK BFD
utilities or scan SDK plugins. Source: `test_hen_diagnostic_artifact.py:symbol_names`.

Evidence files are `build/hen-diagnostic/tests/host-results.json`,
`build/hen-diagnostic/artifact-checks.json` and the manifest above. The
manifest records the package SHA-256 and source hashes. **UNKNOWN:** real
HEN package acceptance and runtime behavior until separately tested.
