# Offline LV1 AV-manager trace

**Verdict (2026-11-01): PROVEN, high confidence:** The AV manager in `sysmgr_ss.elf` converts video IDs into separate RSX raster/clock records and analog-encoder register templates. It directly writes RSX timing MMIO and sends backend settings through `/dev/sc0`; guest GPU hypercall 225 is not the raster-table consumer. **INFERRED, moderate confidence:** Its data structures can describe a low-line progressive raster, making coordinated AV-manager table replacement the smallest software candidate; merely requesting height 240/288 or changing an emulator ID cannot do that. **UNKNOWN:** Whether RSX plus CXM4024R/CXM4027R will generate usable native analog 240p/288p, which encoder bits permit it, and whether installed CFW peek/poke reaches the active service pages. No console was contacted or code executed on one. Hardware impossibility is not established.

## Evidence and scope

PROVEN = checked source or binary; INFERRED = interpretation of checked control/data flow; CLAIMED = attributed external statement; UNKNOWN = unresolved. Addresses are ELF virtual addresses unless stated otherwise. Sony-derived inputs remain in their original folders; raw analysis stays in ignored `local_only/`. Public text contains descriptions and addresses only. The historical draft is not evidence.

## Layer assessment

| Layer | Representable? | Validation | Smallest change |
|---|---|---|---|
| VSH / cellVideoOut | PROVEN supported API has no 240/288 mode; undocumented paths UNKNOWN | Supported IDs only in checked public interface; retail implementation not fully traced | INFERRED bypass its selector with an existing AV request, or change policy; this alone cannot create timing |
| LV2 driver | UNKNOWN for arbitrary rasters | Retail LV2 AV/syscall validation not decoded in this pass | INFERRED bypassable by emulator-style direct VUART access in an authorized guest; no demonstrated LV2-only solution |
| LV1 GPU handler | PROVEN checked attribute 0x100 does not accept custom timing | Call 225 reaches return-only target: 3.55 0x222468 / 4.93 0x222f28 | A new code implementation would be needed for that route; AV manager already has the relevant writer |
| LV1 AV manager | INFERRED yes in its numerical raster representation; no stock low-line profile | VID whitelist; head, surface, format/pitch checks; separate backend whitelist and RSX timing check | Repurpose an existing profile's raster plus matching metadata, and encoder template if necessary; add a new ID only with dispatcher/code changes |
| Encoder commands | PROVEN byte-register images/pairs can be represented; low-line semantics UNKNOWN | Analog backend selects fourteen templates; Syscon/chip-side validation UNKNOWN | Potential template/register-data change; whether Syscon or encoder code must change is UNKNOWN |
| RSX | PROVEN active/total/edge registers receive selected records; 15 kHz progressive hardware result UNKNOWN | Setup checks a measured timing result against the selected raster/clock | INFERRED coherent table data could reach its writer; no verified hardware write recipe |

The supported VSH interface and earlier GPU-handler finding are documented with their source/address evidence in [RESEARCH.md](RESEARCH.md). This trace establishes the deeper AV-manager representation, not an independent rejection floor at every layer.

**INFERRED smallest software candidate:** Reuse an existing accepted SD ID and replace its 56-byte raster record plus consistent 24-byte metadata. The common SD clock record may be reusable if the horizontal sample rate is retained; a new clock must be coherent with the timing check. An encoder-compatible 68-byte template may also be necessary. This is a coordinated data change, not a single height/scan-bit edit. Adding an ID instead requires changing the conversion/validation dispatcher and backend mapping. If new sync behavior cannot be expressed by existing controls, code or hardware changes would be needed; that requirement remains **UNKNOWN**. Cached per-head state also has to be understood before any later activation method is chosen.

**PROVEN public access distinction, UNKNOWN live access:** The checked [Cobra CFW source](https://github.com/Joonie86/COBRA-7.3/blob/master/484/REX/stage2/main.c) implements LV1 peek/poke operations. The checked [PS3HEN source](https://github.com/PS3Xploit/PS3HEN/blob/master/payload/main.c) comments out the analogous standalone syscalls and labels a fallback as partial peek support. Neither proves arbitrary active AV-service memory access on the owner's installed firmware. The service's 0x800xxxxx/0xc000xxxx virtual addresses must be translated to the LV1 memory interface's address space; they are not directly usable peek/poke offsets. Access to an embedded module image is also different from access to its running writable data. No HEN route to unrestricted LV1 writes is established here.

## Input inventory and method

**PROVEN inventory:** 4.93 CORE_OS extraction contains `lv1.elf`, encrypted `lv2_kernel.self`, `sc_iso.self` and other CORE_OS pieces, but no PS1/PS2 emulator ELFs. Its already extracted `sysmgr_ss.elf` is in the sibling workspace's `local_only/analysis/p0i/modules/`. The 3.55 `fw355/out/` folder contains `lv1.elf`, `lv2_kernel.elf`, `vsh.elf`, `ps2_emu.elf`, `ps2_gxemu.elf`, `ps2_softemu.elf`; its AV module is under `analysis/p0h/modules/`. The separate 4.93 `stock.elf` netemu is available. A recursive inventory of the sibling `local_only/` found `variants/ps1_emu.self` but no PS1 ELF, `ps1_netemu` or `ps1_newemu`. Encrypted SELF presence does not provide a decoded PS1 result. No firmware inputs were copied or modified.

**PROVEN identity:** netemu baseline SHA-256 is `63f12b7e00959c758f8aedfd23d1b8bc78e8ae7d922048e4765aaaef1dbbc6d3`. Fresh section-aware reads identify the AV modules as 3.55 `4b2583fcba7c0a3486323508d3f6785e1274717e41a91da20b60f8b2af7adaf5` and 4.93 `07c0bd2a2403f65b6d741e11a15106114365cb78165833bac726476f671b64c9`. The sibling P0I narrative's different 4.93 module hash is not used as identity evidence. Existing Ghidra listings were cross-checked against these ELF mappings; new focused imports use one JVM at a time with a 2500 MB heap and keep projects under this workspace's ignored scratch.

## Timing-profile structure and consumers

**PROVEN structure:** The VID conversion anchors 3.55 0x800249d8 / 4.93 0x80025d10 select an internal profile index. They are code dispatch points, not the raster tables themselves. Setup at 0x800005e4 consumes three distinct table families:

- Metadata: 24-byte records, six 32-bit fields: floating-point nominal refresh, interlace indicator, horizontal sample multiplier, multiplied active width, active height, and a 50/60-family selector. Tables: 3.55 0x80037500; 4.93 0x80039148. Field meanings are **INFERRED** from SD/HD comparisons and downstream arithmetic; widths and access sites are **PROVEN**.
- Raster: 56-byte records, fourteen 32-bit words. The first nine encode horizontal active end, total, two horizontal sync edges, vertical active end, total and three vertical edge/blanking positions; the remaining words include three packed timing values and two output-control words. Exact register semantics of every packed bit remain **UNKNOWN**. Tables: 3.55 0x80036f88; 4.93 0x80038b28. Setup copies the selected record, and 4.93 0x80003bcc writes its fields to RSX timing registers.
- Clock: 12-byte records selected through a per-profile byte index. Each holds a floating-point frequency and two configuration words; one configuration word also contains an upper-byte branch flag. Tables: 3.55 0x80037758; 4.93 0x800393e8 / 0x80039460. The 4.93 selector at 0x800006c0 chooses between two clock banks based on device state. Consumer 0x800015b4 unpacks divider fields and programs the head clock block. SD selects the same 54.0 frequency record in both versions; MHz units are **INFERRED** from horizontal totals and the measurement formula at 0x80001f28. The branch flag distinguishes a fixed/special-clock path from the divider-programming path; precise electrical clock routing remains **UNKNOWN**.

**PROVEN SD comparison:** Internal profiles 1/2 are 480-line interlaced, 3/4 are 576-line interlaced, 5 is 480-line progressive, and 6 is 576-line progressive. Both firmware versions have byte-identical first 25 metadata and raster records. Interlaced and progressive SD profiles retain full-frame vertical totals of 525/625; horizontal sample multipliers differ. There is no 262/312-total low-line entry in these checked records. The internal metadata scan flag, packed raster controls and clock configuration are separate values; changing the flag alone is not demonstrated to change sync cadence.

**INFERRED field names, PROVEN offsets/access sizes:** Within a raster record, offsets 0x00/04 are horizontal active end/total-minus-one; 0x08/0c are horizontal sync edges; 0x10/14 are vertical active end/total-minus-one; 0x18 is an additional vertical timing marker; 0x1c/20 are vertical sync edges. All are 32-bit stored words, with the writer preserving the destination registers' upper halves for these fields. Offsets 0x24/28/2c supply packed timing words; 0x30/34 supply full control words. Sync widths are derived from edge differences, not stored as a separate width field. The marker and packed-word names should remain provisional until the RSX register specification is matched.

**INFERRED rates from checked SD records:** The 480i and 576i families use approximately 15.73/15.63 kHz lines; 480p/576p use approximately 31.47/31.25 kHz lines. The first two share 525/625 total lines with the latter two because interlaced frame totals and progressive frame totals describe different cadences. The representation has room for 240/288 active lines and 262/312 totals, but a coherent low-line profile also needs compatible edges, scan controls, sampling and encoder behavior.

## Producer and consumer call graph

**PROVEN 4.93 flow:** AVB frontend loop 0x8000dab8 calls 0x8002622c, which reaches mode setup 0x800005e4. Request validation at 0x80025c9c converts VID into an internal index and normalizes surface parameters; setup selects/copies metadata, clock and raster records. At 0x80000d08 it calls 0x80003bcc with the raster record; subsequent calls 0x80003a24 and 0x80003b1c consume metadata-derived scan/family information. This is direct mapped-device programming, not a call to guest GPU hypercall 225. Encoder/backend requests are a separate branch of the AVB bundle, detailed below.

**PROVEN register destinations:** 4.93 0x80003bcc writes 32-bit values into head-relative RSX offsets in the 0x680800 timing block, with the second head displaced by 0x2000. Active/total/edge words occupy the low halves of registers; packed values go to additional 0x6808xx registers. This establishes a software timing representation wider than the public VID API. It does not prove that every represented value is electrically accepted. No MMIO was accessed on a console.

**PROVEN other consumers:** 0x800015b4 consumes clock configuration; 0x80002ca4 consumes surface dimensions, metadata and raster controls for placement/scaling; 0x800044c0 derives another output parameter from horizontal total and vertical-edge separation, halving for interlace. 0x80003a24 changes head control in the 0x60014c / 0x600600 blocks from the scan indicator; 0x80003b1c handles the family selector. Cached per-head raster records are saved near the end of mode setup. An unchanged internal profile index can skip raster programming, so modifying table memory alone does not guarantee a reload.

**PROVEN validation boundary:** 4.93 validator 0x80025c9c accepts only mapped IDs, head 0/1, supported color/order/format combinations, suitable pitch/alignment and dimensions bounded by metadata. Height at 0x80025fd0 is an upper bound with special tall-frame cases, not a 480/576 equality requirement. Width at 0x80025f9c has alignment/bounds and a special case. No raw totals/porches/clock come from the public VIDEO_MODE packet. Setup's hardware-timing check at 0x80001c6c / 0x80001f28 compares a measured RSX result against a value derived from the selected horizontal total and clock, with a small tolerance; setup retries and returns a failure after unsuccessful checking. This is an additional consistency gate, not a discovered explicit minimum-525-line validator. An arbitrary raster is neither accepted nor disproved solely by the VID range check.

**PROVEN state restrictions:** Prechecks at 3.55 0x80024994 / 4.93 0x80025ccc additionally restrict VIDs 0x12..0x15, a reserved field and head-specific VESA requests in one service state. The mapped VESA profiles are fixed table entries, not a raw custom-modeline path. Other untraced/private CIDs remain **UNKNOWN**; this report does not claim exhaustive absence of a diagnostic escape.

## Encoder-control path

**PROVEN transport:** In 4.93, constructor 0x80005fdc opens `/dev/sc0`. Header builder 0x800061fc constructs a 16-byte Syscon envelope for service 0x30; it stores version/type, transaction and two 16-bit length fields. Send/read wrappers 0x80006298 / 0x800062e0 reach process syscalls through 0x800370ac / 0x80037070. Those are process write/read calls, distinct from guest VUART hypercalls 163/162. [SC Communication](https://www.psdevwiki.com/ps3/SC_Communication) independently identifies service 0x30 as A/V and the underlying Syscon mailbox MMIO. The physical Syscon-to-encoder I2C connection is **CLAIMED** by the [CXM4027R pinout](https://www.psdevwiki.com/ps3/CXM4027R), not independently traced on a board here; the host-side trace does not establish an SPI hop.

**PROVEN analog template path:** AVB handler 4.93 0x8000d8ec processes frontend VIDEO_MODE members before backend AV_VIDEO_CS members. Backend builder 0x8000f9ac accepts analog ports 0x10/0x11, checks backend VID <= 0x17, then calls 0x8000f4e0. The latter converts a backend format code through a whitelist to one of fourteen 68-byte templates at service VA 0xc0000000. Each template contains 34 pairs of one-byte register index and one-byte value. At 0x8000f7a8, output color/sync-related choices alter selected pair values. The analog branch of 0x8000f9ac strips the known register indices and packs 34 values in the fixed order into a 40-byte backend subrecord, allowing 4 bytes of subheader and 2 further bytes beyond the copied values. AVB assembly at 0x8000dd64 calls 0x8000bdcc, which wraps the backend bundle in service 0x30, command 8, and writes `/dev/sc0`. Thus the normal mode-setting command transmits a fixed register image, not a public arbitrary modeline. The purpose/initialization of those two additional bytes is not resolved here.

**PROVEN additional primitive:** 0x80006544 builds service-0x30 command 5 with a one-byte target, a one-byte pair count and interleaved one-byte register/value pairs. Reply handling checks command acknowledgement. Its existence proves an internal arbitrary-register transport primitive; guest permission/reachability for arbitrary register writes is **UNKNOWN**. It must not be confused with a public custom-timing API.

**INFERRED encoder parameters:** Comparing templates shows the NTSC/PAL interlaced pair changes the family-setting register 0x30; moving to progressive SD changes a group including register 0x91 and timing-related controls. The exact scan-bit allocation, units and any hidden line-count registers are **UNKNOWN**. No explicit host command argument named total-lines, active-lines or half-line phase is visible in this path. That does not prove the fixed register image lacks such fields. There is no checked CXM programming specification or Syscon firmware handler decode here, so whether the encoder derives sync from RSX, synthesizes sync from a standard selector, or combines both remains **UNKNOWN**. Identical-field content also does not establish identical-field sync.

## Emulator mode IDs

**PROVEN scope:** The following are IDs placed in output requests in the checked dispatchers, not every integer resembling a video ID in an ELF. Frontend VIDEO_VID, backend AV_VID and emulator input-format enums are different namespaces. [Linux's AV header](https://github.com/torvalds/linux/blob/master/arch/powerpc/include/asm/ps3av.h) supplies the public packet/standard-ID names; firmware conversion and raster records independently resolve the special IDs.

| Emulator image | Checked dispatcher / request producer | Frontend IDs found (hex) | Low-line output finding |
|---|---|---|---|
| 4.93 `stock.elf` / netemu | 0x1173d4; jump table 0x1174ac; sender 0x11810c | 05,06,07,08,09,0a,0b,0c,10,11 | PROVEN these select standard profiles; no 240/288 raster in this dispatcher |
| 3.55 `ps2_gxemu.elf` | 0x45a70; jump table 0x45c64; AVB construction 0x45d3c; sender 0x494a4 | 05,06,07,08,09,0a,0b,0c,10,11 | PROVEN same standard-mode family, including conditional SD special IDs |
| 3.55 `ps2_softemu.elf` | 0x407f8; jump table 0x408dc; AVB construction 0x4097c; sender 0x42c74 | 05,06,07,08,09,0a,0b,0c,10,11 | PROVEN same standard-mode family |
| 3.55 `ps2_emu.elf` (hardware BC) | 0x491c0; jump table 0x4923c; producers 0x47344, 0x47ba4, 0x47ffc | 01,03,05,06,07,08,09,0a,0b,0c,10,12 | PROVEN checked output selection remains standard; 0x12 selects 1080i60, not low-line SD |
| `ps1_emu` | Only an encrypted SELF found outside the firmware ELF folders | UNKNOWN | No decoded PS1 output-mode result |
| `ps1_netemu`, `ps1_newemu` | Absent from inventory | UNKNOWN | Not analyzed; absence of inputs is not evidence of their behavior |

**PROVEN shared netemu/gxemu/softemu selection:** Their dispatch index 0 selects frontend 0x10 or 5 and backend 0 or 1; index 1 selects 5/1; index 2 selects 0x11 or 6 and backend 5 or 6. Index 3 is the failure entry. Indices 4..9 select frontend/backend pairs 0x0a/7, 9/2, 8/8, 7/3, 0x0c/9, 0x0b/4: respectively 720p50, 720p60, 1080i50, 1080i60, 1080p50, 1080p60. The SD surface heights are 480/576; the HD heights are 720/1080. These same index numbers must not be imposed on `ps2_emu`.

**PROVEN BC selection:** Its dispatcher index 0 fails. Indices 1..10 choose frontend/backend 1/0, 5/1, 3/5, 6/6, 9/2, 0x12-or-7/3, 0x0b/4, 0x0a/7, 8/8, 0x0c/9. Producer 0x47ffc rewrites frontend 1 to 0x10; 0x47344 also constructs a secondary head request using 0x10. Every one resolves to an ordinary SD/HD timing profile in both AV managers.

**PROVEN useful BC distinction:** The input-format switch at 0x777bc maps emulator input enum 0x16 to the 240p-recognition path at 0x779d4, and enum 0x17 to the 288p-recognition path at 0x779c8. The translator at 0x49b20 maps these through ordinary output selections: its 0x49c2c jump table routes 240p input like NTSC interlaced input and 288p input like PAL interlaced input, while configuration can select progressive/HD standard output. This directly explains why finding “240p” in BC video-input handling does not establish native 240p on MultiAV. The content scaling and exact field-reuse algorithm remain **UNKNOWN**.

**PROVEN identities:** The checked 3.55 emulator SHA-256 values are `ps2_emu` `dacd4985acd478221be687c5fe213fc5c3343c66c3747773f67ab099710f5eae`, `ps2_gxemu` `aaf007417c27b0a416bcea315da62721586cf1084544449c1394b39bf8478242`, and `ps2_softemu` `e92f70fcdd1e94a45a5612224579ae94f24fb0644ba78336a5559a6f23416487`. Cross-version emulator comparison is limited to 4.93 netemu versus the available 3.55 variants; these are different programs, not a same-program firmware diff.

## 3.55 versus 4.93

**PROVEN fresh comparison:** The first 25 metadata/raster entries and all fourteen 68-byte analog templates are byte-identical. 4.93 adds three metadata/raster slots and VID aliases, plus a second clock bank; it does not add a checked low-line progressive profile. The extra metadata heights 721/481/577 pair with tall 1500-line raster timing, so they are not 288p/240p candidates.

| Function / data | 3.55 VA | 4.93 VA | Evidence |
|---|---:|---:|---|
| Mode setup | 0x800005e4 | 0x800005e4 | PROVEN same role, changed control flow |
| VID validator | 0x80024964 | 0x80025c9c | PROVEN range grows from 1..0x1a to 1..0x29, with holes |
| Raster writer | 0x80003ac4 | 0x80003bcc | PROVEN matching consumer |
| Metadata scan consumer | 0x8000391c | 0x80003a24 | PROVEN matching consumer |
| Metadata family consumer | 0x80003a14 | 0x80003b1c | PROVEN matching consumer |
| Syscon header builder | 0x800060e4 | 0x800061fc | PROVEN matching transport |
| Register-pair command | 0x8000642c | 0x80006544 | PROVEN matching transport |
| Analog template selector | 0x8000f224 | 0x8000f4e0 | PROVEN matching whitelist |
| Backend bundle sender | 0x8000bca0 | 0x8000bdcc | PROVEN matching transport |
| AVB handler | 0x8000d5b8 | 0x8000d8ec | PROVEN same packet-processing role |

**PROVEN ID resolution:** VIDEO_VID 0x10 selects internal profile 1 (480-line interlace); 0x11 selects profile 4 (576-line interlace). Standard VIDEO_VID 1 selects profile 2, while VID 3 also selects profile 4. The 0x10 variant has different horizontal placement from profile 2, not half the vertical total. IDs 0, 2, 4 and out-of-range IDs fail this conversion. Neither a mapped ID nor Linux's smaller enumeration is a custom/unknown-timing escape.

## Ranked next steps

1. **Locate the active service pages, then identify live records read-only.** Offline, trace the LV1 ELF/process loader or page tables for the `sysmgr_ss` mappings; determine whether read-only timing tables alias the embedded image or are copied. This resolves whether existing CFW LV1 access can reach the consumer's actual data. The bounded identity check below is a prerequisite, not a substitute.
2. **Decode Syscon's service-0x30 command-8 analog handler.** Establish the fixed slot-to-register mapping and the meanings of encoder registers affected by SD interlace/progressive selection, especially family/scan/sync controls. The available host `sc_iso.self` name is not proof that it contains the encoder's final firmware handler. Obtain the corresponding Syscon firmware or a trustworthy CXM programming specification, keeping derived artifacts private.
3. **Finish the RSX control-bit model offline.** Match 0x6808xx timing/control words and the 0x60014c/0x600600 scan controls to the observed SD differences; reconstruct a coherent hypothetical 262/312-line profile and verify every consumer's arithmetic and cache behavior. Confirm which head feeds analog on the target board. No writes are needed for this design review.
4. **Fill the PS1 evidence gap.** Acquire/decrypt the appropriate stock PS1 emulators in a separately writable firmware workspace, then trace actual AV producers. For BC PS2, expand from the checked selection path to frame scheduling and the GS/RSX input path only if exact field reuse matters.
5. **Only after those results, design a separately authorized measured hardware test.** Start with one output type and distinguish component/RGB from composite/S-video. A valid native result needs measured line period, integer lines per refresh and field phase; a 240-line surface or TV label is insufficient. Encoder acceptance may differ by output and chip revision. This report contains no console-write experiment.

## Minimal future read-only experiment

**INFERRED experiment design; not performed:** On an already running Evilnat/Cobra system, using only an already installed and independently verified LV1 peek facility, read three 16-byte ordinary-RAM windows from the embedded 4.93 module image. Each window needs two aligned 64-bit peeks. Keep the six returned words local, restore their original big-endian byte order, and compare their SHA-256 values offline. Repeat those same six reads once to check stability. No app/game launch, new payload, AV command, mode change, Syscon request, MMIO read or memory write is part of this experiment. If the facility's addressing convention or permission is not established, stop before reading; this is not an instruction to install or enable anything.

**PROVEN offline address derivation:** The 4.93 LV1 embedded filesystem begins at file offset 0x1f0000, mapped at LV1 ELF VA 0x400000. Its `sysmgr_ss.fself` begins at VA 0x4218c0. Parsing the FSELF section descriptors maps the AV metadata, raster and encoder templates into the following image windows. These are **LV1 ELF VAs/offsets for ordinary embedded-image RAM**, not the AV-service virtual addresses and not RSX MMIO. The API-specific LV1 peek address convention still needs confirmation on the installed CFW; no high-bit alias is guessed here.

| Purpose | 4.93 LV1 ELF window start | Length | Expected SHA-256 of window |
|---|---:|---:|---|
| Metadata identity anchor | 0x46aea0 | 16 bytes | `d5bfc18bf3510558595e88fe25ededc4306284f90331adaf2fabc68dd753d63f` |
| Raster identity anchor | 0x46a8a0 | 16 bytes | `381678f339e524cd406d4e8545bbdf36c6b8aad426acc497f71f92f7268ec57e` |
| Encoder-template identity anchor | 0x471d40 | 16 bytes | `8a13e9ec965d86a1bc81cd706dfa2bfd9f0fbcf0234e3b7b25425e2ff7f3c455` |

The windows are aligned anchors and may begin slightly before a record boundary; they are not patch locations. The local derivation also records 3.55 anchors separately; never apply the 4.93 addresses to 3.55.

**Interpretation:** Three stable matches support stock embedded-data identity and the chosen LV1 RAM addressing convention. They do not show which profile is active, that those bytes back the running process, or that an encoder accepts custom timing. A mismatch means CFW differences, incorrect mapping, unavailable memory or byte-order/read errors must be resolved; it does not refute 240p feasibility. Once active service mapping is established offline, the next owner-run peek-only observation can read the selected per-head record/cache at an already chosen normal mode. That second phase has no asserted absolute address yet.

## Open questions

- **UNKNOWN:** Exact semantics of packed RSX timing/control words, the extra vertical marker and interfield half-line behavior. Direct total/edge writes narrow the problem but do not answer it.
- **UNKNOWN:** Encoder register meanings, ownership of output sync, fixed-register slot decoding inside Syscon, and low-line acceptance of CXM4024R versus CXM4027R. A standard-mode selector is visible; a hardware minimum is not.
- **UNKNOWN:** Active AV-service address translation, image alias/copy behavior, actual Evilnat 4.93 patches and installed peek API behavior. Embedded-image anchors deliberately do not resolve all four.
- **UNKNOWN:** PS1 requested modes because no decoded PS1 ELF is present. Checked BC PS2 output requests are standard despite explicit low-line input recognition; no all-path hardware-bypass impossibility is claimed.
- **UNKNOWN:** Whether a complete new progressive-SD profile needs only data changes or additional code changes at the encoder/clock/check stages. “Smallest candidate” is not a working patch.
- **Scope limit:** No full retail VSH/LV2 audit, Syscon-firmware execution/decoder trace, console communication, waveform measurements or firmware execution. Ghidra decompilation did not automatically recover every jump table; mode mappings above were resolved separately from numeric table entries and decoded branch targets. No generic NVIDIA indexed-VGA write recipe was assumed; [envytools' PCRTC page](https://envytools.readthedocs.io/en/latest/hw/display/nv3/pcrtc.html) remains incomplete.

## Local evidence and verification

The report skeleton and README pointer were written before deeper tracing and updated as findings accumulated. Private helpers, SHA checks, table comparisons, numeric jump-table resolution, focused Ghidra projects and listings are under `local_only/av_trace/` and adjacent ignored scripts. No firmware inputs were written or copied, and no console or local-network device was contacted. Only this report and README are intended public changes. No commits, index changes or other modifying Git commands were run.

**PROVEN final checks:** Fresh netemu and both LV1 hashes match the verified input identities. Both extracted AV modules' loadable segments match their embedded FSELF contents in the corresponding LV1. All proposed image-anchor hashes match direct LV1 ELF reads. Public-document checks found consistent table columns, no local usernames/absolute paths, no instruction/code dumps and no unfinished placeholders. The historical draft hash remains unchanged. Git status contains only the README edit and this new report; raw artifacts are ignored.
