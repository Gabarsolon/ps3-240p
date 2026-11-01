# LV1 System Manager (`sysmgr_ss`) Runtime Load Address Analysis

**Verdict:**
- **PROVEN (high confidence):** The hypervisor embedded filesystem range (`0x400000 .. 0x5dfda0`) is an initramfs mounted at boot (`0x324e50`) and unmapped/reclaimed after initial process creation. This definitively explains why live peeks at `0x400000`, `0x4218c0`, and the initial table offsets read all zero on the running console.
- **PROVEN (high confidence):** `sysmgr_ss` executes as an isolated user-mode process (`MSR[PR]=1`, `MSR[IR]=1`, `MSR[DR]=1`) inside LV1's internal UX subsystem. Its segments reside at virtual addresses `0x80000000` (text) and `0xc0000000` (data/bss). Within the module, the metadata table is at virtual address `0x80039160` (offset `+0x39160`), the raster table is at `0x80038b60` (offset `+0x38b60`), and the analog encoder template table is at `0xc0000000` (offset `+0x00000`).
- **PROVEN (high confidence):** LV1 maintains a global Process Table at virtual/physical address `0x36cdb0` (TOC delta `-16288(r2)`), squarely within the mapped LV1 image `.data`/`.bss` segment (`0x200000 .. 0x3c2c00`). It contains 32 entries (PIDs 0..31, 16 bytes each, 512 bytes total), holding flags and pointers to each active process's `Process` descriptor.
- **INFERRED (moderate confidence):** Backing physical memory for `sysmgr_ss` is dynamically allocated at boot from the hypervisor stockpile/heap pool by `UserProgramLoader::load` (`0x29ec18`) and mapped into the process's `AddressProtectionDomain` (`0x28b97c`). Consequently, the real physical load address of `sysmgr_ss` is not a compile-time constant and varies across boots.
- **UNKNOWN (hard boundary):** The exact real physical address backing `0x80000000` and `0xc0000000` on any specific console boot cannot be predicted statically without reading runtime heap structures. Furthermore, because heap-allocated page-descriptor nodes cannot be guaranteed statically mapped, memory scanning is unsafe and prohibited.

---

## 1. Embedded Module Loading Mechanism

Static reverse engineering of official PS3 4.93 `lv1.elf` establishes how embedded secure modules are initialized:

1. **Embedded Filesystem (`mfs`):**
   - The hypervisor binary contains an embedded read-only filesystem (`mfs`) in ELF Segment 2 (`0x400000 .. 0x5dfda0`, file offset `0x1f0000 .. 0x3f0000`).
   - During kernel startup, `main.cc` mounts this image via `mfs_root` (`0x324e50`).
   - The filesystem stores `/std/pme_init` (pid 1), `/std/pme_init.conf`, and embedded services including `/std/ss_init.fself` and `/std/sysmgr_ss.fself`.
   - **Post-boot reclamation:** Once `/std/pme_init` and early services are instantiated, the filesystem structure is unlinked and its memory range (`0x400000 .. 0x5dfda0`) is marked free, zeroed, or returned to the hypervisor page stockpile.
   - This proves why webMAN MOD `/peek.lv1` reads of `0x400000`, `0x4218c0` (the embedded FSELF file header), and previous table offsets returned zeroes on the live console.

2. **UX Process Model and Spawning:**
   - Modules in `/std/*.fself` do not execute as hypervisor-mode code. They run in LV1's internal user-mode subsystem (UX) with address translation enabled (`MSR[PR]=1, MSR[IR]=1, MSR[DR]=1`).
   - `pme_init` parses `/std/pme_init.conf` and invokes the UX process execution syscall (`sys_ux_exec` at `0x2a0900`) to spawn `sysmgr_ss.fself` as a background service daemon.

3. **ELF Loading and Dynamic Allocation (`UserProgramLoader`):**
   - `UserProgramLoader::load` (`0x29ec18`) parses the decrypted ELF headers of `sysmgr_ss`:
     - **Segment 0 (Text):** Virtual address `0x80000000`, memory size `0x3f6c8`, permissions `r-x`.
     - **Segment 1 (Data):** Virtual address `0xc0000000`, memory size `0xdd58`, permissions `rw-`.
   - Rather than executing in place, `UserProgramLoader` dynamically requests physical pages from the hypervisor stockpile allocator (`0x292518`).
   - The decrypted code and data segments are copied into these newly allocated pages.
   - The loader maps these physical pages into the process's virtual address space by calling `AddressProtectionDomain::mapPage` (`0x28b97c`).

---

## 2. Module Runtime Base and Address Space Translation

1. **Address Space Structure:**
   - Effective/virtual addresses `0x8000xxxx` and `0xc000xxxx` are isolated within the process's own `AddressProtectionDomain`.
   - The Cell Broadband Engine PPU hardware translates these addresses using the Segment Lookaside Buffer (SLB) and Hashed Page Table (HTAB).
   - During UX thread context switches (`0x2c37e0`), the hypervisor updates the Logical Partition Control Register (`mtspr LPCR, r0` at `0x2c3a3c`) and Page Table Description Register (`mtspr SDR1, r9` at `0x2c3a54`).
   - Because webMAN MOD `/peek.lv1` accesses hypervisor physical/real memory (using the flat hypervisor address space), virtual addresses `0x80000000` and `0xc0000000` cannot be peeked directly.

2. **Hypervisor Process Table Linkage:**
   - In hypervisor memory, LV1 manages processes through a static table located at `0x36cdb0` (TOC delta `-16288(r2)`).
   - The Process Table contains 32 entries (PIDs 0..31), 16 bytes per entry (total size `0x200` = 512 bytes):
     - `+0x00`: Active flag / status word (`0x0000000000000001` when occupied).
     - `+0x08`: 64-bit pointer to the heap-allocated `Process` object (`Process *proc`).
   - Inside each `Process` object:
     - `proc + 0x00`: 32-bit Process ID (PID).
     - `proc + 0x20`: Pointer to `AddressProtectionDomain` (page translation and region tree).
     - `proc + 0x638`: Pointer to `UserProgramLoader` descriptor.
     - `*( *(proc + 0x638) ) + 0x24a`: Null-terminated ASCII process name string (e.g. `"sysmgr_ss.fself"`).

3. **Runtime Base Determination:**
   - The physical load base (`PhysBase_Text` and `PhysBase_Data`) is dynamic and determined at boot time by the stockpile allocator.
   - It is not a fixed constant.
   - To read the live tables via `/peek.lv1`, the real physical page addresses mapped to virtual `0x80000000` and `0xc0000000` must be resolved from the process's page translation records.

---

## 3. Staged Safe Peek Plan

To prevent hypervisor crashes, all peeks must strictly adhere to the crash boundary rules:
- Reads must only target addresses that are statically proven mapped (inside the LV1 image segments `0x200000 .. 0x3c2c00`).
- No memory scanning, pattern searching, or unverified pointer dereferences are permitted.

### Stage 1: Process Table Discovery (PROVEN SAFE)
- **Target Address:** `0x36cdb0` (aligned 512-byte hex view).
- **Endpoint:** `GET /peek.lv1?36cdb0`
- **Safety Argument:**
  - `0x36cdb0` is located inside the LV1 `.bss` segment (`0x363d80 .. 0x374218`).
  - Control reads of `0x20d060`, `0x32adb0`, and `0x300000` already verified that the entire LV1 image segment is mapped 1:1 and accessible via `/peek.lv1`.
  - webMAN MOD returns an aligned 512-byte view (`HEXVIEW_SIZE = 0x200`), capturing all 32 process slots in a single HTTP GET request.
  - Reading ordinary hypervisor data segment memory cannot cause a bus fault, machine check exception, or timing disturbance.
- **Expected Result:**
  - A 512-byte hex view containing 32 pairs of 64-bit words.
  - Active slots contain status flag `0x0000000000000001` at `+0x00` and a non-null heap pointer at `+0x08`.

### Stage 1b: Process Descriptor and Identification (CONDITIONAL)
- **Target:** Locate the slot corresponding to `sysmgr_ss.fself`.
- **Heap Pointer Validation Rule:**
  - Before dereferencing any pointer `proc` obtained from Stage 1, the pointer must be validated to lie strictly within ordinary hypervisor RAM (`0x00000000 .. 0x0fffffff`).
  - If a pointer falls outside known RAM boundaries, execution MUST STOP.
- **Resolution:**
  - Peek `proc + 0x638` to obtain `UserProgramLoader*`.
  - Peek `loader + 0x00` to obtain the loader descriptor.
  - Peek `loader_desc + 0x240` (16 bytes) to verify the ASCII name `"sysmgr_ss.fself"`.

### Stage 2: Target Table Anchors (BOUNDED COMPUTED RESOLUTION)
- Once the physical backing pages for `sysmgr_ss` text (`PhysBase_Text`) and data (`PhysBase_Data`) are resolved:
  - **Metadata anchor:** `PhysBase_Text + 0x39160` (16 bytes, two 64-bit words).
  - **Raster anchor:** `PhysBase_Text + 0x38b60` (16 bytes, two 64-bit words).
  - **Encoder-template anchor:** `PhysBase_Data + 0x00000` (16 bytes, two 64-bit words).
- **Safety STOP Condition:**
  - If `PhysBase_Text` or `PhysBase_Data` cannot be unambiguously resolved through proven-mapped descriptor reads, or if the addresses lie outside verified RAM, Stage 2 peeks MUST NOT be attempted. Memory scanning is strictly prohibited.

---

## 4. Expected Value Patterns

### Stage 1 Discovery Patterns (`0x36cdb0`)
- **Table base (`0x36cdb0`):**
  - Slot 0 (PID 0): System idle / kernel root process.
  - Slot 1 (PID 1): `pme_init` (`0x0000000000000001` flags + heap pointer).
  - Slots 2..6: Services spawned by `pme_init` (`ss_init`, `sysmgr_ss`, `ss_server1..3`).
  - Inactive slots: All zero words (`00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00`).

### Stage 2 Target Anchor Patterns (Adjusted to Discovered Real Base)
When the real physical bases `PhysBase_Text` and `PhysBase_Data` are supplied, the 16-byte identity windows must match the published anchor values:

| Anchor | Target Address | Word 0 Pattern | Word 1 Pattern | Expected 16-byte SHA-256 |
|---|---|---|---|---|
| Metadata anchor | `PhysBase_Text + 0x39160` | `426fc28f00000001` | `0000000400000b40` | `d5bfc18bf3510558595e88fe25ededc4306284f90331adaf2fabc68dd753d63f` |
| Raster anchor | `PhysBase_Text + 0x38b60` | `00000b3f00000d67` | `00000b7f00000c77` | `381678f339e524cd406d4e8545bbdf36c6b8aad426acc497f71f92f7268ec57e` |
| Encoder-template anchor | `PhysBase_Data + 0x00000` | `91003000313f3201` | `3302340035003600` | `8a13e9ec965d86a1bc81cd706dfa2bfd9f0fbcf0234e3b7b25425e2ff7f3c455` |
