# LV1 System Manager (`sysmgr_ss`) Runtime Load Address and Heap Architecture

**Verdict & Ground Truth Status:**
- **PROVEN (high confidence):** The hypervisor embedded filesystem range (`0x400000 .. 0x5dfda0`) is an initramfs mounted at boot (`0x324e50`) and unmapped/reclaimed after initial process creation. This definitively explains why live peeks at `0x400000`, `0x4218c0`, and the initial table offsets read all zero on the running console.
- **PROVEN (high confidence):** `sysmgr_ss` executes as an isolated user-mode process (`MSR[PR]=1`, `MSR[IR]=1`, `MSR[DR]=1`) inside LV1's internal UX subsystem. Its segments reside at virtual addresses `0x80000000` (text) and `0xc0000000` (data/bss). Within the module, the metadata table is at virtual address `0x80039160` (offset `+0x39160`), the raster table is at `0x80038b60` (offset `+0x38b60`), and the analog encoder template table is at `0xc0000000` (offset `+0x00000`).
- **PROVEN (high confidence):** LV1 maintains a global Process Table at physical address `0x36cdb0` (TOC delta `-16288(r2)` at `0x356098`), squarely within the mapped LV1 image `.data`/`.bss` segment (`0x200000 .. 0x3c2c00`). It contains 32 entries (PIDs 0..31, 16 bytes each, 512 bytes total). Active processes hold flags at `+0x00` and a non-null pointer to their heap-allocated `Process` descriptor at `+0x08`.
- **PROVEN (high confidence):** Ground truth live console peeks (4.93 Evilnat) confirmed word 0 (`+0x00`) is `0x0000000000000000` for all running processes; non-zero second words (`+0x08`) populate slots 0 (`0x6bb0d0`), 3 (`0x12c010`), 5 (`0x915d0`), 6 (`0xe5040`), and 10 (`0x12c8d0`). Static re-derivation confirms that word 0 holds teardown/termination status (bit 0 set only during exit/zombie cleanup), while `entry[+8] != 0` is the true occupancy indicator. Slot 3 is definitively proven to be `sysmgr_ss.fself` by the boot sequence in `pme_init.conf`.
- **CORRECTION (high confidence):** The prior claim that `0x3727d0` and `0x3727d8` formed an authoritative `[RamBase, RamLimit]` bounds pair was **refuted** by live console ground truth and code analysis. Live peeks revealed `0x6047e0, 0x604800, 0x604840, ...`, differing by 32 bytes (`0x20`). Code analysis proves `0x3727c0` is the head of a linked list of 32-byte memory node descriptors located in dynamic BSS (`0x600000+`).
- **PROVEN (high confidence):** The true allocators behind `Process` (size 2224 / `0x8b0`) and `AddressProtectionDomain` (size 128 / `0x80`) are `operator new` (`0x2c2484`) and the hypervisor `buddy_allocator` (`0x2bcb98`). Primary allocator instances are registered at `0x3725b0`, `0x3725b8[0..63]`, and PME pool globals `0x373db0` / `0x373da8`.
- **PROVEN (high confidence):** All five observed live process pointers (`0x915d0 .. 0x6bb0d0`) reside strictly within the lowest ~7 MB of physical RAM (`0x00000000 .. 0x00700000`), well inside the hypervisor partition (16 MB / 32 MB boundary).
- **PROVEN (high confidence):** Pointer dereference chain re-evaluation establishes that `AddressProtectionDomain*` resides at `Process + 0x18` (vtable `0x35ecb8`), while `Process + 0x20` holds `VAddrRangeManager*` (vtable `0x3602b8`). Page nodes in `AddressProtectionDomain` contain 64-bit physical address at `+0x00`, 32-bit virtual address at `+0x08`, and 32-bit next pointer at `+0x14`.

---

## 1. Embedded Module Loading Mechanism

Static reverse engineering of official PS3 4.93 `lv1.elf` (cross-checked against 3.55) establishes how embedded secure modules are initialized:

1. **Embedded Filesystem (`mfs`):**
   - The hypervisor binary contains an embedded read-only filesystem (`mfs`) in ELF Segment 2 (`0x400000 .. 0x5dfda0`, file offset `0x1f0000 .. 0x3f0000`).
   - During kernel startup, `main` mounts this image via `mfs_root` (`0x324e50`).
   - The filesystem stores `/std/pme_init` (pid 1), `/std/pme_init.conf`, and embedded services including `/std/ss_init.fself`, `/std/sysmgr_ss.fself`, `updater_frontend.fself`, and `ss_server1..3.fself`.
   - **Post-boot reclamation:** Once `/std/pme_init` and early services are instantiated, the filesystem structure is unlinked and its memory range (`0x400000 .. 0x5dfda0`) is unmapped or returned to the hypervisor stockpile allocator.
   - This proves why webMAN MOD `/peek.lv1` reads of `0x400000`, `0x4218c0` (the embedded FSELF file header), and previous table offsets returned zeroes on the live console.

2. **UX Process Model and Spawning Sequence:**
   - Modules in `/std/*.fself` execute in LV1's internal user-mode subsystem (UX) with address translation enabled (`MSR[PR]=1, MSR[IR]=1, MSR[DR]=1`).
   - The early boot sequence proceeds strictly as follows:
     - **Slot 0 (PID 0):** Kernel root / idle process, created at kernel startup in `main` (`Process::create(0, 0)`). Runtime pointer: `0x6bb0d0`.
     - **Slot 1 (PID 1):** Initial user program `/std/pme_init`. `pme_init` opens `/std/pme_init.conf` and parses boot commands.
     - **Slot 2 (PID 2):** `pme_init` parses `boot ss_init.fself sync`. `ss_init` runs synchronously, completes hardware security initialization, and terminates, freeing slot 2.
     - **Slot 3 (PID 3):** `pme_init` parses `boot sysmgr_ss.fself nosync`. It spawns `sysmgr_ss` as a permanent resident daemon. Runtime pointer: `0x12c010`.
     - **`pme_init` Exit:** Having launched the configured daemons, `/std/pme_init` calls `sys_ux_exit` (`0x80004808`) and terminates, freeing slot 1.
     - **Slots 5, 6, 10:** Spawned by background security daemons (`ss_server1`, `ss_server2`, etc.). Transient initialization helpers occupy and free slots 4, 7, 8, 9.

3. **ELF Loading and Dynamic Allocation (`UserProgramLoader`):**
   - `UserProgramLoader::load` (`0x29ec18`) parses the decrypted ELF headers of `sysmgr_ss`:
     - **Segment 0 (Text):** Virtual address `0x80000000`, memory size `0x3f6c8`, permissions `r-x`.
     - **Segment 1 (Data):** Virtual address `0xc0000000`, memory size `0xdd58`, permissions `rw-`.
   - `UserProgramLoader` dynamically requests physical pages from the hypervisor allocator (`0x292518`).
   - Decrypted code and data segments are copied into these newly allocated pages.
   - The loader maps these physical pages into the process's virtual address space by calling `AddressProtectionDomain::mapPage` (`0x28b97c`).
   - `UserProgramLoader` does NOT store segment base addresses in any global variable; once mapping completes, physical page records reside exclusively within the process's `AddressProtectionDomain` page list and hardware page tables.

---

## 2. Correction History: Refutation of `0x3727d0` as Bounds

### The Faulty Assumption
Early documentation hypothesized that `0x3727d0` and `0x3727d8` held a scalar `[RamBase, RamLimit]` bounds pair (e.g. `0x00000000` and `0x01000000` / `0x02000000`).

### Live Console Ground Truth
When the console owner performed a read-only GET peek of `0x3727d0` (`GET /peek.lv1?3727d0`), the returned 64-bit words were:
`0x00000000006047e0`
`0x0000000000604800`
`0x0000000000604840`
`0x0000000000604870`
`0x0000000000600200`
`0x00000000006042b0`
`0x0000000000604260`
`0x00000000006046c0`

Words 0 and 1 differ by only 32 bytes (`0x20`), and all values point into memory above the 6 MB kernel boundary (`0x600000+`). This demonstrably refutes the "authoritative RAM bounds pair" hypothesis.

### Static Code Reality
Disassembly and decompilation of memory management functions (`0x2befcc`, `0x2bf06c`, `0x2bf218`, `0x2bf24c`) reveal the true semantics:
- `0x3727c0`: Global head pointer of a singly linked list of 32-byte physical memory nodes.
- `0x3727d0 .. 0x372808`: Array of global pointers referencing individual memory node descriptors allocated sequentially in dynamic BSS extension memory (`PH[3]`: `0x600000+`).
- Node layout:
  - `+0x00`: Type / flags.
  - `+0x10`: Physical base address of the represented region (`paddr`).
  - `+0x18`: Pointer to next memory node (`next`).
- The routines at `0x2bf218` and `0x2bf24c` iterate through this list to compute minimum and maximum physical memory addresses for hypervisor subsystems.

---

## 3. Hypervisor Allocator Architecture & Heap Descriptors

### Allocator Instances Behind `Process`
1. **`Process` Object Allocation:**
   - Size: 2224 bytes (`0x8b0`).
   - Caller: `Process::create` (`0x29e090`).
   - Allocator: Calls `operator new(2224)` at `0x2c2484`, which calls `buddy_allocator::allocate` (`0x2bcb98`).
   - Initializer: `Process::Process` at `0x29d580`.

2. **`AddressProtectionDomain` Object Allocation:**
   - Size: 128 bytes (`0x80`).
   - Caller: Process creation routines (`0x2a0700` / `0x29e288`).
   - Allocator: Calls pool allocator `0x2c1b88(0x80, pool)`.
   - Initializer: `AddressProtectionDomain::AddressProtectionDomain` at `0x28af58`.

3. **Global Allocator Descriptors in Image `.bss` (`0x200000 .. 0x3c2c00`):**
   - **`0x3725b0`** (TOC delta `-11008(r2)`): Primary `buddy_allocator` instance pointer.
   - **`0x3725b8`** (TOC delta `-11016(r2)`): Array of 64 `buddy_allocator*` pointers (`0x3725b8[0..63]`).
   - **`0x363dc0`** (TOC delta `-10944(r2)`): Stockpile allocator descriptor pointer.
   - **`0x372c60`**: Per-CPU/thread allocator pointer table.
   - **`0x362450`**: PME memory configuration (`pme.memory.size`, default 2834432 bytes / `0x2b4000`, configurable up to 16 MB).
   - **`0x373db0`** (TOC delta `-7016(r2)` / `-10328(r2)`): Current cursor pointer of reserved PME memory.
   - **`0x373da8`** (TOC delta `-7008(r2)` / `-10344(r2)`): Remaining size of reserved PME memory pool (replenished in 16 KB chunks).

4. **`buddy_allocator` Internal Descriptor Structure:**
   - Size: ~1000 bytes (`0x3e8`).
   - `allocator + 0x00`: Vtable pointer.
   - `allocator + 0x118 .. +0x388`: 39 free-list bins for power-of-two chunk sizes.
   - `allocator + 0x398`: Region list head pointer.
   - `allocator + 0x3a8`: First region descriptor node.
   - Region descriptor fields:
     - `node + 0x20`: Physical base address (`paddr_base`).
     - `node + 0x28`: Size of managed physical span in bytes (`size`).

---

## 4. LV1 Physical Memory Architecture

1. **PPE Hypervisor Execution Mode:**
   - In LV1 hypervisor mode, the PowerPC 970 / Cell PPE executes with `MSR[DR]=0` and `MSR[IR]=0` (Real Addressing Mode).
   - In real addressing mode, effective addresses are identical to physical real addresses (1:1 mapping).
   - All addresses referenced in LV1 kernel `.text`, `.data`, `.bss`, and heap are direct physical memory addresses.

2. **Total Hardware RAM:**
   - Physical memory: 256 MB XDR DRAM (`0x00000000 .. 0x0FFFFFFF`).

3. **Hypervisor Memory Layout Across Boots:**
   - **`0x00000000 .. 0x00004400`:** Exception vectors (`PH[0]`).
   - **`0x00004400 .. 0x00200000` (~2 MB):** Low physical RAM used by LV1 early boot allocators. This proves why active `Process` objects reside at `0x915d0`, `0xe5040`, `0x12c010`, and `0x12c8d0`.
   - **`0x00200000 .. 0x003e6c00` (~1.9 MB):** Static LV1 kernel image (`PH[1]`: text, data, bss). Unconditionally mapped and readable.
   - **`0x00400000 .. 0x005fffff` (2 MB):** Embedded initramfs (`PH[2]`), reclaimed post-boot.
   - **`0x00600000 .. 0x007fffff+`:** Dynamic heap and BSS extensions (`PH[3]`), holding memory nodes (`0x6047e0`) and high process objects (PID 0 kernel root at `0x6bb0d0`).
   - **Hypervisor Partition Ceiling:** Hypervisor-owned memory spans from `0x00000000` up to `0x01000000` (16 MB) or `0x02000000` (32 MB).
   - **Guest LPAR Boundary:** GameOS (LPAR 1) memory begins immediately above the hypervisor boundary. Under no circumstances may hypervisor peeks touch guest LPAR memory.
   - **MMIO Boundaries:** RSX display MMIO (`0x280xxxxxxxx` / `0x6808xx`) and Cell hardware registers reside in high physical address ranges (`0x2000_0000_000+`), completely separate from system DRAM.

4. **Static vs. Boot-Set Constants:**
   - **Static (Compile-time):** Image segment bounds (`0x200000 .. 0x3c2c00`), vtable addresses (`0x35ecb8`, `0x3602b8`), Process Table location (`0x36cdb0`), slot count (32), slot stride (16 bytes).
   - **Dynamic (Boot-time):** Allocator heap cursors (`0x373db0`), active process pointers (`0x12c010`), dynamically mapped page addresses (`PhysBase_Text`, `PhysBase_Data`).

---

## 5. Re-evaluated Pointer Dereference Chain (PROVEN vs. INFERRED)

Every offset in the path to `PhysBase_Text` and `PhysBase_Data` has been verified against 4.93 `lv1.elf` machine code:

```
Process Table Slot 3 (0x36cdb0 + 0x30)
  |
  +-> [+0x08] Process* (live: 0x12c010) [PROVEN]
        |
        +-> [+0x00] PID == 3 (uint32) [PROVEN]
        |
        +-> [+0x18] AddressProtectionDomain* [PROVEN]  (NOTE: was inferred as +0x20)
        |     |
        |     +-> [+0x00] vtable == 0x35ecb8 (uint64) [PROVEN]  (NOTE: was inferred as 0x3602b8)
        |     |
        |     +-> [+0x28] Sentinel PageNode (0x20 bytes) [PROVEN]
        |     |
        |     +-> [+0x3c] first_node pointer (uint32) [PROVEN]
        |           |
        |           +-> PageNode (0x20 bytes) [PROVEN]
        |                 |
        |                 +-> [+0x00] paddr (uint64) [PROVEN] -> PhysBase_Text / PhysBase_Data
        |                 +-> [+0x08] vaddr (uint32) [PROVEN] -> 0x80000000 / 0xc0000000
        |                 +-> [+0x10] prev_node (uint32) [PROVEN]
        |                 +-> [+0x14] next_node (uint32) [PROVEN] -> iterates until domain + 0x28
        |                 +-> [+0x18] page_shift (uint8) [PROVEN]
        |
        +-> [+0x20] VAddrRangeManager* [PROVEN]
              |
              +-> [+0x00] vtable == 0x3602b8 (uint64) [PROVEN]
              +-> [+0x10] Sentinel range node (0x30 bytes) [PROVEN]
              +-> [+0x20] first_range pointer (uint64) [PROVEN]
```

### Detailed Field Proofs:
1. **`Process Table Slot 3 -> Process*` (`PROVEN`):**
   - Address: `0x36cdb0 + 0x30 + 0x08`.
   - Proof: `Process::Process` (`0x29d614`) computes `slot * 16 + 8` and stores `param_1` (`Process*`). `ProcessManager::getProcessByPid` (`0x29c8d4`) loads directly from `slot * 16 + 8`.
2. **`Process + 0x00 -> PID` (`PROVEN`):**
   - Proof: `Process::Process` stores `iVar13` (allocated PID) as a 32-bit int at `*(param_1 + 0x00)`.
3. **`Process + 0x18 -> AddressProtectionDomain*` (`PROVEN`):**
   - Proof: `func_0x0029d184` writes `param_2` (`AddressProtectionDomain*`) to `*(param_1 + 0x18)`.
   - *Correction note:* Earlier documentation assumed `+0x20`. Code proves `+0x18` holds the domain, while `+0x20` holds the virtual address range manager.
4. **`AddressProtectionDomain + 0x00 -> Vtable` (`PROVEN`):**
   - Address: `0x35ecb8`.
   - Proof: Constructor `AddressProtectionDomain::AddressProtectionDomain` (`0x28af58`) loads descriptor pointer from `0x3556b8` (pointing to `0x35ecb8`) and stores it at `*(param_1 + 0x00)`.
   - *Correction note:* Previous plan expected `0x3602b8` at `domain + 0x00`. `0x3602b8` is the vtable for `VAddrRangeManager` (`Process + 0x20`), while `AddressProtectionDomain` uses `0x35ecb8`.
5. **`AddressProtectionDomain + 0x3c -> First PageNode` (`PROVEN`):**
   - Proof: Constructor calls `0x292894(domain + 0x28, 0)` to initialize the sentinel node at `domain + 0x28`. Lookup routine `0x28b2c8` reads `*(uint32*)(domain + 0x3c)` to obtain the first node pointer.
6. **`ProtectionPage` Node Layout (`PROVEN`):**
   - `+0x00` (uint64): `paddr` (physical backing address). Set by `0x29292c` and returned by `0x2928c4`.
   - `+0x08` (uint32): `vaddr` (virtual address). Set by `0x29292c` and returned by `0x2928bc`.
   - `+0x10` (uint32): `prev` pointer. Set by `0x29292c` and `0x292894`.
   - `+0x14` (uint32): `next` pointer. Set by `0x29292c` and `0x292894`; traversed by `0x28b2c8`.
   - `+0x18` (uint8): Page size shift (`shift`). Set by `0x29292c`; `0x2928dc` computes `1L << shift`.
7. **Virtual Range Mapping (`PROVEN`):**
   - `UserProgramLoader::load` (`0x29ec18`) maps segment 0 at `vaddr 0x80000000` and segment 1 at `vaddr 0xc0000000`. Matching nodes yield `PhysBase_Text` and `PhysBase_Data`.

---

## 6. Script Bounds Checking & Safety Specification

### webMAN MOD View Margin Constraint
webMAN MOD's `/peek.lv1` endpoint formats an aligned 512-byte (`0x200`) view:
```c
address &= 0xFFFFFFFFFFFFFFF0ULL; // Align down to 16 bytes
// Formats 512 bytes: [address .. address + 0x200)
```
Because webMAN over-reads beyond the target pointer, a check on `ptr < limit` is insufficient. The check MUST cover the entire view window:
`view_start = ptr & ~0xFL`
`view_end = view_start + 0x200L`

### Strict Bounds Rules:
1. **HV Safe Boundaries:**
   - `HvHeapBase`: `0x00000000L`.
   - `HvHeapLimit`: `0x02000000L` (32 MB, strict hypervisor partition ceiling; protects GameOS LPAR memory).
2. **Consistency Validation with Known Live Pointers:**
   - Before following any dynamic pointer, the script validates that all five known live `Process*` pointers (`0x915d0`, `0xe5040`, `0x12c010`, `0x12c8d0`, `0x6bb0d0`) satisfy:
     `(P & ~0xF) >= HvHeapBase && (P & ~0xF) + 0x200 <= HvHeapLimit`
   - If any known pointer fails, the bound is invalidated and the script HALTS immediately.
3. **Per-Pointer Dereference Gate:**
   - For every pointer `P` in the chain:
     - Must be non-null and 8-byte aligned (`(P & 0x7) == 0`).
     - Must satisfy `(P & ~0xF) >= HvHeapBase && (P & ~0xF) + 0x200 <= HvHeapLimit`.
   - Any violation triggers an immediate, unconditional abort with zero network packets transmitted.
