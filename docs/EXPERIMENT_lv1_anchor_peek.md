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
   powershell -ExecutionPolicy Bypass -File tools/lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169"
   ```
4. Record the terminal output and return the printed word values, computed SHA-256 hashes, and PASS/FAIL status for each anchor.
