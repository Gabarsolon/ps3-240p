# Experimental HDMI 1024 x 768

**PROVEN (latest checkpoint, 2026-11-15):** the corrected native-buffer
candidate shows the complete XMB at 1024 x 768, including the clock and
left/lower icons. Separate receiver diagnostics report 1024 x 768 / scan
raw `0x07`; an undisturbed repeat passes the picture/receiver/recovery
checks and holds ACTIVE for 15.108722 seconds. Stock 1080p and every owned
RAM change are restored. Evidence: ignored `20261115-013130/` and
`20261115-013357/`, sequence `4013907208`, inspected
`xga-native-020-state3.jpg`. This is an HDMI XMB test on the owner console;
app/game behavior, exact cadence and pixel-perfect scaling remain UNKNOWN.

**PROVEN (cause and correction):** allocation and logical UI canvas are
separate. Keep XMB's logical 1280 x 720 canvas, allocate 1024 x 768, and
normalize only the exact inconsistent native AV request to that real
allocation. PAF then supplies the canvas-to-framebuffer scaling. Altering
both geometry lookups removed that ratio and cropped the fixed XMB layout.
Sources: `xga_source.c`, `xga_cache.S`, private VSH
`0x0039c488..0x0039c508`, and the paired real captures below.

**PROVEN (observer limit):** the live-dimension diagnostic run had only
two mature active-frame samples and correctly failed the runner's
three-sample requirement despite console success and full-picture frames.
It was not relabeled as an overall pass. The independent undisturbed
repeat passed the existing criterion. Evidence: those two runs' JSON
reports; `tools/display_probe_test.py`.

**PROVEN (installed menu, 1.6.0-dev2):** the checked 323,760-byte development
package was installed using webMAN's keep-PKG command. All six installed
members matched the package. From a fresh boot with only webMAN resident,
the app loaded its packaged `crt_xga_probe` into empty slot 2, queued
profile 9, acknowledged the handoff and exited normally. One Cross on
“Test 1024x768 / 4:3 (CFW)” automatically ran the stock and capability
controls before the candidate. Actual saved frames show the complete XMB;
receiver reports 1024 x 768, then 1920 x 1080 after checked RAM cleanup.
Evidence: ignored `local_only/xga_scaling/install-20261115-013624/installed.json`
and `menu-20261115-014110/{app.log,helper.log,report.json}`; inspected
`frame-045-state3-kind4.jpg`. No release or permanent XGA enforcement was added.

Development package SHA-256:
`f4f32a95a5321925ccbed8045a239fb0f2df44814696dada37ea265c578b6b49`.
Packaged helper SHA-256:
`61960aa2b7ca98d53bf7c70ea75927f387a7b00b50b2c9caf5d4f1bff5e28b5c`.

For the development menu, start at stock HDMI 1080p. Open **HDMI PC
resolution presets**, pause SD enforcement if enabled, then select **Test
1024x768 / 4:3 (CFW)**. The app exits to XMB and runs the required controls
automatically; the candidate always restores after 15 seconds. This is a
4:3 raster with a scaled XMB canvas, not verified native game rendering or
integer scaling. Existing timing edits must be cleared before starting.

**PROVEN (final cleanup):** after verified callback/source/timing restoration,
a normal soft reboot cleared the temporary helper. Final readback showed
only webMAN resident, visible stock 1080p, the original empty SD-enable
marker restored, unchanged boot-plugin list and SD helper, and every
installed development-package member still matching. Evidence: ignored
`local_only/xga_scaling/cleanup-20261115-014429/{report.json,stock-xmb.jpg}`.

**PROVEN (owner console, 2026-11-14):** a timed, data-only change to the
native WXGA raster produced a visible XMB and receiver-reported live
1024 x 768, signal present, scan byte `0x07`. The helper held the target
for 15 seconds and returned to visible stock 1920 x 1080. The picture was
cropped; this first test establishes signal acceptance, not correct scaling.
Evidence: ignored `local_only/xga_scaling/automatic/20261114-232652/`,
sequence `2733819845`, `xga-raster.json`, `xga-raster.log`, inspected
`xga-raster-051-state3.jpg` and the baseline/recovery images.

**CLAIMED (upstream receiver interpretation):** scan byte `0x07` means
progressive. Its exact firmware mapping and refresh measurement remain
unvalidated. Source and limitations: [capture status](CAPTURE_INPUT_STATUS.md).
Configured OBS dimensions alone are never used as timing proof.

**PROVEN (offline binary audit):** the 4.93 `sysmgr_ss` profile 16 metadata
at `0x800392c8` describes 1280 x 768. Its raster at `0x80038ea8` uses
1664 horizontal and 798 vertical total pixels/lines. The selected native
clock entry describes 79.5 MHz. The experiment changes horizontal active
end to 1023 and preserves that clock, totals and sync edges.
Source: private `sysmgr_ss.elf`, SHA-256
`07c0bd2a2403f65b6d741e11a15106114365cb78165833bac726476f671b64c9`;
ignored `local_only/xga_scaling/{profile-inventory,raster-decoded,clock-decoded}.json`.

**INFERRED (arithmetic):** the retained parameters imply approximately
59.8702 Hz (`79,500,000 / (1664 * 798)`). This is a non-DMT prototype,
not standard 65 MHz XGA or a measured exact 60 Hz output.

**PROVEN (implementation):** the isolated CFW helper uses dynamic sysmgr
mapping discovery and a complete fingerprint of its 20 audited source
words before writing. It snapshots original bytes at runtime, verifies
each write, restores only its owned changes, and refuses unknown edits.
Source: `experiments/display_probe/xga_patch.c`. The installed release app,
its SD helper and the boot-plugin list are separate from this experiment.

**PROVEN (implementation):** two further profiles request full-frame
1024 x 768 stretch, or a centered 1024 x 576 picture preserving the source
16:9 aspect. The source remains 1280 x 720. The geometry code patches need
cache maintenance in the sysmgr domain. The self-authored `xga_cache.S`
flushes all seven modified instruction lines and invokes the original native
callback with its arguments preserved. The shared checked wrapper lifecycle
restores original geometry before the stock RPC, then detaches and clears
its own RAM arena after stock state returns. It reads native scaler
registers for diagnostics and performs no direct MMIO stores.
Sources: `experiments/display_probe/{native.c,xga_cache.S}`,
`app/source/hd540_cache.c`; tests `test_xga_patch.c`, `test_xga_cache.py`
and `test_hd540_cache.c` built with `CRT_XGA_PROBE`.

**PROVEN (first scaler trial):** sequence `1874406248` entered the target
and returned DONE/detail 0, including successful wrapper cleanup. Native
readback showed a horizontal window 0..1023, vertical window 0..767,
vertical step `0x0f000000`, FP mode SCALE and scaler flags `0x21101110`.
The picture still lost the right edge. Evidence: ignored
`local_only/xga_scaling/automatic/20261114-233238/`, `xga-stretch.log`,
`xga-stretch.json` and inspected `xga-stretch-060-state3.jpg`.

**PROVEN (source defect):** the authored scaler flags enabled Y but omitted
X: `0x1110` lacks bit 0. **CLAIMED (independent definitions):** X enable is
bit 0 and Y enable is bit 4 at FP_DEBUG_0. Source:
[Linux v6.12 nvreg.h](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/nouveau/dispnv04/nvreg.h),
`NV_PRAMDAC_FP_DEBUG_0_XSCALE_ENABLE` and `_YSCALE_ENABLE`.
**PROVEN (corrected implementation):** the candidate now requests `0x1111`;
native readiness requires both axis-enable bits, and the patch regression
checks them. Sources: `xga_patch.c`, `native.c`, `tests/test_xga_patch.c`.
**PROVEN (axis-enable trial):** both axis-enable bits read back set, but the
right edge remained cropped. Evidence: ignored
`local_only/xga_scaling/automatic/20261114-233739/`, sequence `1006586037`.
The subsequent centered-window trial also cropped the bottom despite
confirmed X/Y steps `0x14000000` and source width 1280. Evidence:
`20261114-234128/`, sequence `4161706396`; extended native readback in
`xga-aspect.log` and inspected `xga-aspect-060-state3.jpg`.

**PROVEN (private source trace):** the native unequal-width branch at
`0x80002e10` selects bit 2 alongside its explicitly calculated horizontal
step. **INFERRED:** that selects the explicit X step rather than automatic
scaling; the corresponding Y selection is bit 6. A follow-up candidate
requests `0x1155` with the same checked source, windows, clock and steps.
The meaning of these additional bits needs functional confirmation; this
is not a proven scaling fix until saved geometry is checked.

**PROVEN (explicit-selection trial):** sequence `1984528217` read back
scaler flags `0x21101155` with the same intended steps/windows. The picture
zoom changed substantially and cropped more of the source; stock recovery
and wrapper cleanup completed. Evidence: ignored
`local_only/xga_scaling/automatic/20261114-234417/`, `xga-aspect.log` and
inspected `xga-aspect-057-state3.jpg`.
**INFERRED:** these bits activate coefficient selection, but treating
`0x14000000` as a simple positive 1.25 sampling step was incorrect. A next
trial tests complement encoding `2 - source/output`: `0x0c000000` for both
axes in the centered 1024 x 576 box, or Y `0x11000000` for full-height
stretch. This formula remains a hypothesis pending image verification.

**PROVEN (complement trial):** sequence `883943173` verified the changed
coefficients but produced the same enlarged/cropped picture, then restored
and cleaned up. Evidence: ignored `20261114-234757/`, `xga-aspect.log` and
inspected `xga-aspect-060-state3.jpg`. The simple complement interpretation
did not produce a fitting result. Bits 2/6 are therefore not presented as
understood manual-step controls.

**PROVEN (RSX-specific caution):** native `0x800044c0` writes `+0x884` using
a product derived from raster totals/sync width. Desktop Nouveau's
FP_DEBUG_1 scale-test definitions are insufficient to justify overwriting
that register on RSX. Source: private 4.93 function and ignored
`local_only/activation/sysmgr_ss_extra_0x800044c0.c`. No direct `+0x884`
experiment was executed.

**PROVEN (next implementation):** the output metadata width is now also
1024 for geometry profiles, while the source remains separately checked at
1280 x 720. The extra bits 2/6 are removed. Clock/totals/sync and the bounded
timed restoration are retained. Source: `xga_patch.c`, `native.c` and
`tests/test_xga_patch.c`; picture fit remains pending this trial.

**PROVEN (metadata-only result):** sequence `3359504584` did not enter the
XGA target. VSH reported the conversion request, but independent AV cache
stayed VID 9 / 1280 active pixels rather than VID 13 / 1024. The five-second
target deadline triggered normal restoration; DONE/detail TIMEOUT, visible
1080p recovery and wrapper cleanup were recorded. Evidence: ignored
`local_only/xga_scaling/automatic/20261114-235555/`, `xga-stretch.json` and
`xga-stretch.log`. This is a failed candidate, not a successful XGA test.

**PROVEN (validator trace):** `0x80025fc8` compares requested source width
against metadata width divided by its sample multiplier; the following
branch rejects an excessive source. A 1280-wide source exceeds the new
1024-wide output metadata. Source: matching private 4.93 binary at
`0x80025f64..0x80025fd4`.
**PROVEN (bounded follow-up):** the authored comparison now caps source
width at 1280 during geometry trials. The rejection branch, source-height
checks, source pitch and allocations remain checked. This is not an
unbounded validator bypass. The additional code line `0x80025f80` is covered
by both cache flush directions; the complete 19-word source fingerprint is
`0x0cea530ae3ab8a1f`. Source: `xga_patch.c`, `xga_cache.S`, host regressions
and ignored `local_only/xga_scaling/width-policy-audit.json`. Actual fitting
remains pending the live follow-up.

**PROVEN (bounded-width result):** sequence `4228360963` reached AV VID 13
with the 1024-wide raster. Native readback selected horizontal policy bit 2
and X step `0x10000000` rather than the anticipated `0x14000000`; the guard
immediately restored instead of starting the hold. The unequal-width branch
at `0x80002e18` supplies that third X-step path. Its authored constant now
also requests `0x14000000`, and readiness accepts the observed native
horizontal policy. Complete 20-word source fingerprint:
`0x2d4f977b059d09a6`; no additional cache line is needed for `0x80002e18`.
Source: private binary at that address, `xga_patch.c`, `native.c` and ignored
`local_only/xga_scaling/automatic/20261115-000158/xga-stretch.log`.

**PROVEN (complete 720p-source path):** sequence `876895848` completed a
15-second hold, live receiver 1024 x 768 / `0x07`, exact native geometry
readback and clean stock recovery. Its actual image still enlarged/cropped
the source horizontally. Evidence: ignored
`local_only/xga_scaling/automatic/20261115-000704/`, `xga-stretch.json`,
`xga-stretch.log` and inspected `xga-stretch-020-state3.jpg`. Correct
downscaling is not established by these successful numeric checks.

**INFERRED (next direction):** the native dimension limits favor upscaling
rather than a source larger than output. Profiles 7/8 therefore investigate
a stock 720 x 480 source with the same WXGA-derived output. They retain the
original width validator. The native source mapper identifies progressive
480 as internal mode 4; existing native PC mapping supplies the WXGA output
flags, and source/readback must independently remain mode 4 / 720 x 480.
Sources: private VSH `0x000c56f4`, `native.c`, `xga_patch.c` and
`tests/test_xga_patch.c`. This combination is experimental until observed;
the existing native provider advertises PC conversion from 720p, not 480p.

**PROVEN (480p-source checks):** without a VSH source patch, sequence
`771107330` changed native mode to 4, then PC submission allocated a
1280 x 720 / pitch 5120 source. The helper rejected the mismatched target
and recovered. A checked VSH branch change at `0x00430864` subsequently
retained pitch 3072, but AV cache stayed stock VID 5 rather than PC VID 13.
That trial also timed out and restored the source instruction plus stock
1080p. Evidence: ignored `20261115-001220/` and `20261115-003404/` under
`local_only/xga_scaling/automatic/`. No successful 480p-to-XGA result is claimed.

**PROVEN (native-source lead):** VSH's geometry table maps internal mode 11
to the dimensions at `0x00430944` / `0x0043098c`. The native-source trial
generates original instructions for width 1024 and height 768 there, rather
than trying to downscale a 1280-wide source. All three audited VSH regions
are fingerprinted together (`0x68404d609f9addf6`) and originals are kept at
runtime. Writes use the existing PS3MAPI process-memory interface followed
by cache maintenance in the VSH mapping. Every failed staged write and
rollback retry is covered by `tests/test_xga_source.c`.
Source: private VSH `0x00430784` mode table, named addresses above,
`xga_source.c` and `native.c`; PS3MAPI transport in the inspected upstream
`local_only/sources/webman/ps3mapi.h`, opcode `0x0032`.

**PROVEN (native-source scope):** profile 9 starts directly from the verified
1080p baseline so changing the 720p dimension lookup cannot hide the need
to allocate new buffers. Its LV1 changes are only output width metadata
and active raster width; scaler and source-validator instructions stay
original. Readiness requires independent source cache 1024 x 768, pitch
bounds and the native full-frame valid windows. The expected framebuffer
uses fewer bytes than the prior 1280 x 720 source. Sources: `xga_patch.c`,
`native.c` and host tests. Actual rendering/receiver acceptance remain pending.

**PROVEN (first native-source result):** sequence `1377390784` allocated
native pitch 4096, but AV cache remained the prior VID 11 / 1920-wide
raster. The target timed out and restored all owned changes. Evidence:
ignored `local_only/xga_scaling/automatic/20261115-004032/xga-native.log`.

**PROVEN (request trace):** sequence `1195569860` captured the bounded first
video record read-only before the native AV callback. It requested head 0,
VID 13, **1280 x 720 with pitch 4096**. That is inconsistent with the new
1024-wide allocation and exceeds the output metadata width. Original
callback arguments and request memory were not changed. Evidence: ignored
`20261115-005258/xga-native.log`; self-authored `xga_cache.S` (456 bytes),
native log fields and cache/readback tests. No direct MMIO experiment was
added to obtain this evidence.

**PROVEN (second lookup):** VSH `0x0043054c` uses a separate source-size
lookup from allocation helper `0x00430748`. Mode 11 selects source-size
constants at `0x0043069c` / `0x004306a8`. The native trial now changes those
to 1024 / 768 alongside the allocation constants. All four regions are
checked together with fingerprint `0x614733e5b1c6da9c`; each of the four
staged-store failures has a rollback regression. Sources: matching private
VSH, ignored `trace-430540.asm`, `xga_source.c`, `tests/test_xga_source.c`.
The next live trial must confirm the actual request matches the buffer,
then separately validate the received picture and stock restoration.

**PROVEN (request mismatch persists):** sequence `4103743423` still sent
1280 x 720 with pitch 4096 after both VSH lookups changed. It timed out
and restored stock 1080p. Evidence: ignored
`20261115-010008/xga-native.{log,json}`. Changing the geometry lookups alone
does not correct this AV request.

**PROVEN (offline correction guards):** the self-authored 720-byte callback
wrapper can temporarily normalize this exact head-0 / VID-13 / format-0 /
1280 x 720 / pitch-4096 request to 1024 x 768. Only profile 9 arms it;
all other tuples pass unchanged. The native callback consumes and copies
the record synchronously (private sysmgr `0x8002622c`). The wrapper restores
the caller's original width and height immediately after the native call.
No direct MMIO stores are added. Sources: `xga_cache.S`,
`app/source/hd540_cache.c`, `tests/test_xga_cache_ghidra.py`.

**PROVEN (offline validation):** Ghidra emulation of our own payload covers
the exact armed tuple, twelve negative cases, native success/error and a
64-bit return, callback arguments, stack/TOC/nonvolatile preservation,
whole-packet restoration, cache coverage and no display-register stores.
Host tests also cover every failed payload/control/descriptor store and
checked cleanup. The larger wrapper remains in audited mapped padding:
RX `0x8003f800..0x8003fc00`, RW telemetry/control through `0xc000de67`;
runtime requires an empty arena before staging. Sources: named tests,
`tools/build_xga_cache.py`, private ELF load headers and ignored
`local_only/xga_scaling/arena-audit.json`.

**PROVEN (normalized live request):** sequence `330604358` reached ACTIVE
for the full 15 seconds. AV cache VID 13, source 1024 x 768 / pitch 4096,
full valid windows, unit steps, callback success and both request
normalization/restoration flags matched. The receiver's live report was
1024 x 768, scan raw `0x07` (upstream progressive interpretation; exact
firmware mapping/refresh unvalidated). All owned changes and stock 1080p
were restored. Evidence: ignored `20261115-011610/xga-native.{log,json}`.

**PROVEN (remaining crop):** inspection of the same test's actual
`xga-native-026-state3.jpg` still shows the clock clipped at the right edge.
The numeric success does not make this a usable full-picture preset.
The source request inconsistency is corrected; the remaining renderer or
capture geometry cause is UNKNOWN and requires separate evidence.

**PROVEN (independent geometry read):** sequence `1914460575` repeated
without a live USB timing query and still clipped the clock. Read-only VSH
snapshots showed mode 11 at `0x006fd884`, allocation width/height
1024/768 at `0x007366fc` / `0x00736700`, pitch 4096 at `0x007366f8`,
and no source-provider override at `0x007364a4`. The helper and stock
recovery passed again. Evidence: ignored `20261115-012250/` and
`local_only/xga_scaling/live-source-globals.json`.

**INFERRED (next control):** the request still selects widescreen aspect.
Profile 9 will request the native mapper's 4:3 aspect argument (1) instead
of 16:9 (2), with exact stock restoration. This is an ordinary temporary
native request, not a saved setting or extra RAM patch. Whether the PC
mapper honors it or fixes XMB layout is UNKNOWN until captured.

**PROVEN (aspect control rejected):** sequence `567754447` requested aspect
1 and the native mapper returned `-1` with zero flags/mode. This refusal
preceded source, callback and timing staging. Stock 1080p readback and
picture recovered. Evidence: ignored `20261115-012545/xga-native.{log,json}`.
The source reverts to the accepted aspect-2 mapping.

**PROVEN (logical canvas differs from allocation):** private VSH
`0x0039c488..0x0039c508` separately queries logical and allocated sizes and
computes their ratio. Layout loader `0x000bc0f8` registers the stock
1280 x 720 grid/factor pair for mode 11. The files were retrieved read-only
for inspection; no flash file was written. Sources: private VSH, ignored
`local_only/xga_scaling/layout/` and `trace-3a2a80.asm` / `trace-bc080.asm`.

**INFERRED (narrower candidate):** retain the original logical 1280 x 720
query, change only the allocated buffer to 1024 x 768, and keep the guarded
AV-request correction. This lets PAF retain its canvas-to-buffer ratio
instead of treating the fixed XMB layout as native 1024 coordinates.
`xga_source.c` now changes only the two allocation instructions, retains
the complete four-region identity, and tests both logical-query words
remain original. Actual full-picture behavior remains to be established.

**UNKNOWN:** calibrated geometry beyond the owner XMB test, app/game compatibility,
integer scaling, latency, component output, ordinary HEN support, standard
XGA clock programming and persistent enforcement. Geometry profiles must
pass independent live receiver checks, actual image inspection and exact
stock recovery before being offered as a working menu option.

```powershell
python tools/build_plugin.py --xga-probe
python tests/test_plugin.py build/xga-probe/plugin/vsh_240p.elf --module-name crt_xga_probe
python tests/test_xga_cache.py
python tools/test_xga.py --emulate
python tools/display_probe_test.py --xga --action all --profile xga-native --timing-candidate
python tools/display_probe_test.py --xga --action xga-native --profile xga-native
python tools/build_ps3_app.py --xga-presets
```

The runner requires the matching independently verified helper already
loaded in an empty slot. It records OBS Virtual Camera without audio and
uses fresh staged requests. The helper owns timed restoration even if the
PC disconnects. A live timing sample may disturb USB capture; repeat without
that sample for undisturbed picture evidence. See [AGENTS.md](../AGENTS.md).
