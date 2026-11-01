# Read-only experiment plan: LV1 anchor peek

**Status: READY FOR OWNER EXECUTION — NEVER RUN AUTONOMOUSLY.** Under the project rules, the assistant never contacts the console (192.168.0.169) or any local network device. This specification provides the complete interface analysis, exact URLs, safety validation, expected value patterns, and execution procedure for the console owner.

## Target environment

- Console: PlayStation 3 Slim CECH-25xx, IP `192.168.0.169` on local subnet.
- Firmware: Evilnat 4.93 CFW with Cobra payload enabled.
- Resident plugin: webMAN MOD 1.47.x (Standard or Full edition, memory debugging enabled via `DEBUG_MEM`).
- Host workstation: PC on the same LAN running PowerShell.

## Peek interface analysis

### webMAN MOD HTTP interface

Inspection of the public webMAN MOD source repository ([aldostools/webMAN-MOD](https://github.com/aldostools/webMAN-MOD)) identifies the exact implementation of the hypervisor memory peek mechanism:

1. **URL dispatch and parameter parsing:**
   - File: `include/ps3mapi/debug_mem.h`, function `ps3mapi_find_peek_poke_hexview(char *buffer, char *templn, char *param)` (lines 201–489).
   - Address parsing (line 281): `address = convertH(param + 10);`. Because the endpoint prefix `/peek.lv1?` has length 10, the query parameter is directly parsed as a hexadecimal integer.
   - Address space selection (line 294): `lv1 = strstr(param, ".lv1?") ? 1 : 0;`.
   - Base memory masking (line 299): `if(check_address || !lv1) address |= BASE_MEMORY;`. For hypervisor access (`lv1 = 1`), unless explicit safety checking (`!`) is invoked, the address is preserved as a raw 64-bit LV1 physical/effective address without imposing the LV2 kernel base offset (`0x8000000000000000`).
   - Alignment (line 384): `address &= 0xFFFFFFFFFFFFFFF0ULL;`. The requested address is masked to an aligned 16-byte boundary.
   - Response formatting (lines 423–471): Formats a 512-byte (`HEXVIEW_SIZE = 0x200`) view inside an HTML `<pre>` block, with 16 bytes per line:
     `%08X  XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX XX  ........`

2. **Hypervisor peek transport:**
   - File: `include/ps3mapi/peek_poke.h`, lines 3–28:
     - Syscall definitions: `#define SC_PEEK_LV1 (8)`, `#define SC_PEEK_LV1_COBRA (11)`.
     - Function: `static u64 lv1_peek_cfw(u64 addr) { system_call_1(SC_PEEK_LV1, addr); }`.
   - On Evilnat 4.93 with Cobra, LV1 peeking is executed via the Cobra hypervisor peek syscall, reading 64-bit words in big-endian byte order.

## Safety evaluation and crash boundaries

1. **Mapped embedded-image RAM:**
   - The 4.93 hypervisor maps its embedded filesystem at ELF virtual address `0x400000`. The embedded `sysmgr_ss.fself` binary resides at `0x4218c0`.
   - The three anchor windows (`0x46a8a0`, `0x46aea0`, `0x471d40`) reside strictly within ordinary DDR3/XDR RAM backing the loaded module image.
   - None of these addresses correspond to RSX display MMIO (`0x280xxxxxxxx` / `0x6808xx`), Syscon mailbox MMIO, or page table structures.

2. **Mechanisms of hypervisor crash risk:**
   - **Unmapped physical addresses:** In the Cell Broadband Engine architecture, an access to an unmapped physical address triggers a Machine Check Exception (MCE) or Data Storage Interrupt (DSI) in hypervisor mode. Because LV1 operates at maximum privilege, an unhandled MCE halts the processor instantly (system freeze or triple-fault shutdown).
   - **Volatile/Side-effect MMIO:** Accessing hardware FIFOs or clear-on-read device status registers disrupts running peripherals and Syscon communications, causing watchdog panic.
   - **Safety conclusion:** The six target addresses are strictly bounded within mapped, immutable read-only image RAM. Reading these locations carries zero risk of bus faults, register state alterations, or display interruption.

## The six target anchors and expected values

The experiment evaluates three 16-byte identity windows (two aligned 64-bit words per window, totaling six words).

| Anchor name | Window start | Length | Target words | Expected word pattern | Expected 16-byte SHA-256 |
|---|---:|---:|---|---|---|
| Metadata identity anchor | `0x46aea0` | 16 bytes | Word 0: `0x46aea0`<br>Word 1: `0x46aea8` | Word 0: `0x426fc28f00000001`<br>Word 1: `0x0000000400000b40` | `d5bfc18bf3510558595e88fe25ededc4306284f90331adaf2fabc68dd753d63f` |
| Raster identity anchor | `0x46a8a0` | 16 bytes | Word 0: `0x46a8a0`<br>Word 1: `0x46a8a8` | Word 0: `0x00000b3f00000d67`<br>Word 1: `0x00000b7f00000c77` | `381678f339e524cd406d4e8545bbdf36c6b8aad426acc497f71f92f7268ec57e` |
| Encoder-template anchor | `0x471d40` | 16 bytes | Word 0: `0x471d40`<br>Word 1: `0x471d48` | Word 0: `0x91003000313f3201`<br>Word 1: `0x3302340035003600` | `8a13e9ec965d86a1bc81cd706dfa2bfd9f0fbcf0234e3b7b25425e2ff7f3c455` |

### Value semantics
- **Metadata anchor (`0x46aea0`):** Encodes the standard 480i profile 1 metadata header: nominal 59.94006 Hz refresh (`0x426fc28f` IEEE 754 float), interlace scan flag `1`, horizontal pixel multiplier `4`, and active multiplied width `2880` (`0x0b40`).
- **Raster anchor (`0x46a8a0`):** Encodes horizontal active end `2879` (`0x0b3f`), total minus one `3431` (`0x0d67`), and horizontal sync edges `2943` (`0x0b7f`) and `3191` (`0x0c77`).
- **Encoder-template anchor (`0x471d40`):** Encodes analog backend register template 0 pairs: register `0x91` = `0x00`, `0x30` = `0x00`, `0x31` = `0x3f`, `0x32` = `0x01`, `0x33` = `0x02`, `0x34` = `0x00`, `0x35` = `0x00`, `0x36` = `0x00`.

### Meaning of results
- **Match (PASS on all six words and three hashes):**
  - PROVES that the installed Evilnat 4.93 hypervisor retains the uncompressed stock `sysmgr_ss` image in physical memory.
  - PROVES that webMAN MOD `/peek.lv1` directly accesses ordinary hypervisor physical RAM using effective 32-bit linear offsets.
  - Establishes a verified baseline for subsequent read-only exploration of active runtime service pages.
- **Mismatch (FAIL):**
  - Indicates that either the firmware has patched the embedded module image, the hypervisor allocates service pages under an address translation offset, or webMAN MOD interprets LV1 addresses through a different base convention.
  - Does NOT indicate hardware failure or refute 240p feasibility.

## Verification script

The PowerShell test tool is located at `tools/lv1_anchor_peek.ps1`.

### Parse verification (offline, assistant-run)
```powershell
$tokens = $null; $errors = $null;
[System.Management.Automation.Language.Parser]::ParseFile('<home>/ps3-240p/tools/lv1_anchor_peek.ps1', [ref]$tokens, [ref]$errors);
if ($errors.Count -eq 0) { Write-Output "PARSE-CHECK PASS" } else { $errors }
```

### Dry-run verification (offline, assistant-run)
```powershell
powershell -ExecutionPolicy Bypass -File tools/lv1_anchor_peek.ps1 -WhatIf
```

## Owner execution instructions

When ready to test live on the console:

1. Power on the PS3 (192.168.0.169) and verify that webMAN MOD is loaded (XMB notification appears).
2. On the local workstation, open PowerShell in the repository root.
3. Execute the test script:
   ```powershell
   # Run full staged resolution (Stage 1 -> Stage 1b -> Stage 2):
   powershell -ExecutionPolicy Bypass -File tools/lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169"

   # Or run individual stages incrementally:
   powershell -ExecutionPolicy Bypass -File tools/lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169" -Stage1Only
   powershell -ExecutionPolicy Bypass -File tools/lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169" -Stage1bOnly
   ```
4. Record the terminal output and return the printed word values, computed SHA-256 hashes, and PASS/FAIL status for each anchor.

## Stage plan (three-stage safe dynamic resolution)

Based on the static hypervisor loader and boot configuration analysis documented in `docs/LV1_SYSMGR_SS_LOADADDR.md`, the live copy of `sysmgr_ss` executes in the isolated UX subsystem with virtual segments at `0x80000000` (text) and `0xc0000000` (data), backed by dynamically allocated physical RAM pages.

Because `UserProgramLoader::load` does not write load addresses to static global variables and no static `AddressProtectionDomain` objects exist in `.bss`, segment load addresses must be resolved via the active process descriptor. To safely locate and verify the active tables without scanning memory or risking machine check exceptions, a three-stage read-only protocol is defined:

### Stage 1: Static .bss Discovery (Process Table & Memory Bounds) — PROVEN SAFE
1. **Target addresses:**
   - `0x36cdb0`: Process Table (32 slots, 16 bytes each; 512 bytes total, fits in single `/peek.lv1` request).
   - `0x3727d0`: Memory Manager global physical RAM boundaries (`RamBase` at `0x3727d0`, `RamLimit` at `0x3727d8`).
2. **Safety rationale:**
   - Both addresses reside strictly within the LV1 image's `.data`/`.bss` segment (`0x200000 .. 0x3c2c00`).
   - Control reads of `0x20d060`, `0x32adb0`, and `0x300000` proved this segment is mapped 1:1 and accessible via `/peek.lv1`.
   - webMAN MOD `/peek.lv1` formats an aligned 512-byte view (`HEXVIEW_SIZE = 0x200`), capturing the entire 32-slot table in a single HTTP GET request.
   - Reading static mapped kernel memory carries zero risk of bus faults, MMIO side-effects, or display timing interruption.
3. **Execution & Interpretation:**
   - Query: `GET http://192.168.0.169/peek.lv1?36cdb0`
   - Active process slots have `entry[+8] != 0` (holding a 64-bit pointer to the heap-allocated `Process` object). Word 0 (`+0x00`) holds process flags and is `0` for running processes (bit 0 is only set during exit/teardown).
   - In Evilnat 4.93, Slot 3 corresponds to `sysmgr_ss.fself` (spawned resident by `pme_init.conf` command `boot sysmgr_ss.fself nosync`; slot 0 is kernel root, slots 1 and 2 exited after boot). Live peek revealed `Process* = 0x12c010`.
   - Query: `GET http://192.168.0.169/peek.lv1?3727d0`
   - Retrieves authoritative hypervisor RAM limits (`RamBase` and `RamLimit`) to govern all subsequent pointer validations.

### Stage 1b: Bounds-Checked Domain & Page Resolution — CONDITIONAL & GATED
1. **Target address:** Dynamic pointer traversal starting from Slot 3 `Process*` (`0x12c010`).
2. **Strict safety gates (Zero scanning, bounds-checked dereferencing):**
   - **Gate 1 (Process pointer bounds):** Check that `0x12c010` lies strictly within `[RamBase, RamLimit - 0x100]`. If outside, HALT.
   - **Gate 2 (Domain pointer bounds):** Query `GET /peek.lv1?12c010` to read `Process` fields. Offset `+0x20` contains `domain` (`AddressProtectionDomain*`). Check that `domain` lies strictly within `[RamBase, RamLimit - 0x100]`. If outside, HALT.
   - **Gate 3 (Domain vtable verification):** Query `GET /peek.lv1?<domain>` to read the first 16 bytes of the domain object. Word 0 (`domain + 0x00`) MUST equal `0x00000000003602b8` (the static compiled vtable for `AddressProtectionDomain` in LV1 4.93). If it does not match, HALT immediately.
   - **Gate 4 (PageNode list traversal):** Offset `domain + 0x58` contains the head pointer to the mapped page list (`PageNode*`). For each node:
     - Verify `node` lies strictly within `[RamBase, RamLimit - 0x20]`.
     - Read `PageNode` (16 bytes): `+0x00` is physical address (`paddr`), `+0x08` is user virtual address (`vaddr`), `+0x10` is next pointer.
     - Record `paddr` when `vaddr == 0x80000000` (`PhysBase_Text`).
     - Record `paddr` when `vaddr == 0xc0000000` (`PhysBase_Data`).
   - If both segment bases are found and verified within `[RamBase, RamLimit]`, Stage 1b succeeds.

### Stage 2: Target Timing Anchors — COMPUTED RESOLUTION
1. **Target addresses:** Computed from discovered physical segment bases (`PhysBase_Text` and `PhysBase_Data`):
   - **Metadata identity anchor:** `PhysBase_Text + 0x39160` (16 bytes, two 64-bit words).
   - **Raster identity anchor:** `PhysBase_Text + 0x38b60` (16 bytes, two 64-bit words).
   - **Encoder-template anchor:** `PhysBase_Data + 0x00000` (16 bytes, two 64-bit words).
2. **Expected anchor values (adjusted to discovered base):**
   - Metadata anchor: Word 0 = `0x426fc28f00000001`, Word 1 = `0x0000000400000b40`, SHA-256 = `d5bfc18bf3510558595e88fe25ededc4306284f90331adaf2fabc68dd753d63f`.
   - Raster anchor: Word 0 = `0x00000b3f00000d67`, Word 1 = `0x00000b7f00000c77`, SHA-256 = `381678f339e524cd406d4e8545bbdf36c6b8aad426acc497f71f92f7268ec57e`.
   - Encoder anchor: Word 0 = `0x91003000313f3201`, Word 1 = `0x3302340035003600`, SHA-256 = `8a13e9ec965d86a1bc81cd706dfa2bfd9f0fbcf0234e3b7b25425e2ff7f3c455`.
3. **Safety STOP condition:**
   - If `PhysBase_Text` or `PhysBase_Data` cannot be deterministically verified through Stage 1b, Stage 2 peeks MUST NOT be attempted. Memory scanning is strictly prohibited.

## Result (run by the orchestrator on 2026-11-01, owner approved; read-only GET requests; Evilnat 4.93 + webMAN MOD)

**The planned anchors FAILED, and the cause is understood (PROVEN by control reads).**

- The three planned 16-byte windows (0x46aea0, 0x46a8a0, 0x471d40) read as all zero on the live console.
- Control reads prove the method works: the script's peeks of addresses in the LV1 text and data segments (0x20d060, 0x32adb0, 0x300000) matched the bytes of the local official 4.93 LV1 image exactly (the LV1 image is mapped at its ELF virtual addresses; webMAN `/peek.lv1?<hex>` returns them).
- The 0x400000..0x5fffff range of the ELF holds the embedded filesystem (including the sysmgr_ss image). On the live console 0x400000, 0x4218c0 (start of the embedded sysmgr_ss file) and all three anchors read zero, while the same bytes are present in the image file. 0x500000 shows different live data than the file. So the embedded filesystem is not resident at those addresses at runtime (INFERRED: LV1 releases or overwrites it after loading the modules).
- Consequence: the live copy of the AV-manager tables is somewhere else in memory (the sysmgr_ss module runs in its own mapping). Its real address is UNKNOWN. Scanning memory with webMAN's find/search is not safe as an experiment (reads of unmapped addresses can crash the hypervisor), so no scan was done.

**Confirmed for the project:** the anchor values in the file are as predicted (the table bytes at the planned file offsets match this document), so the static analysis of the tables stands; only the live-peek route needs another way to find the loaded copy (for example following LV1's own module-load bookkeeping, found statically in LV1 4.93, to the sysmgr_ss load address).

## Stage 1 result (run by the orchestrator on 2026-11-01, owner approved; one read-only GET of `/peek.lv1?36cdb0`)

- The read worked (no fault); the stage-1 script's own parser failed on the page, so the view was parsed separately.
- **The layout predicted by the static analysis did not match.** The 512-byte view has NO entries with a `1` flag at +0x00. Slots hold zero in the first word; the second word is non-zero only in five of the 32 slots: slot 0 = `0x6bb0d0`, slot 3 = `0x12c010`, slot 5 = `0x915d0`, slot 6 = `0xe5040`, slot 10 = `0x12c8d0`. Those look like pointers into low RAM / the heap, but what the table is, and whether these are process descriptors, is UNKNOWN.
- Stage 1b (following a pointer) and stage 2 were NOT done: the premise "active slots are flagged and point to Process objects" is not confirmed, and the claim that the whole range `0..0x0fffffff` is safe to read is not proven.

## Stage 1 follow-up: read of `0x3727d0` (run by the orchestrator on 2026-11-01, owner approved)

- The read worked (no fault). The first eight words were `0x6047e0, 0x604800, 0x604840, 0x604870, 0x600200, 0x6042b0, 0x604260, 0x6046c0`.
- **These are NOT the RAM base/limit predicted by the static analysis.** `0x6047e0` and `0x604800` differ by 32 bytes, so they cannot be a memory range, and all values look like pointers into the LV1 data/bss area just above the image. The "authoritative RAM bounds" claim for `0x3727d0` is therefore wrong (or the layout differs); stage 1b was NOT run. A trustworthy bound for following heap pointers is still missing.
