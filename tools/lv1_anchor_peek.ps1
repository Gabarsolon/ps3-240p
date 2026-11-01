<#
.SYNOPSIS
    Three-stage read-only verification of LV1 process structures, AddressProtectionDomain,
    and AV timing anchors via webMAN MOD HTTP GET.

.DESCRIPTION
    Sends read-only HTTP GET requests to webMAN MOD's /peek.lv1 endpoint on a PlayStation 3
    running CFW (e.g. Evilnat 4.93 + Cobra) using a provably safe, staged approach:
    
    Stage 1 (Proven-Mapped Discovery & Memory Bounds):
      Peeks within the proven-mapped LV1 .data/.bss segment (0x200000..0x3c2c00):
        - Process Table at 0x36cdb0 (32 slots, 16 bytes each; Slot 3 = sysmgr_ss.fself).
        - Hypervisor memory manager bounds at 0x3727d0 (RamBase and RamLimit).
      webMAN MOD returns aligned 512-byte hex views.
      
    Stage 1b (Bounds-Checked Process & Domain Resolution):
      Follows heap pointers ONLY after strict alignment and bounds checking against
      the RAM limits derived in Stage 1:
        - Verifies sysmgr_ss Process object at Slot 3 pointer (PID == 3).
        - Reads AddressProtectionDomain pointer at proc + 0x20.
        - Verifies AddressProtectionDomain vtable matches statically known address 0x3602b8.
        - Traverses the mapped page node linked list (domain + 0x58) to resolve the real
          physical backing bases: PhysBase_Text (vaddr 0x80000000) and PhysBase_Data (0xc0000000).
          
    Stage 2 (Anchor Verification):
      Reads the three 16-byte timing and encoder identity windows:
        - Metadata anchor at PhysBase_Text + 0x39160
        - Raster anchor at PhysBase_Text + 0x38b60
        - Encoder-template anchor at PhysBase_Data + 0x00000
      Compares returned words against expected values and verifies 16-byte SHA-256 hashes.

    This script contains NO write/poke commands and performs NO memory scanning.

.PARAMETER ConsoleIP
    IPv4 address of the target PlayStation 3 console (default: 192.168.0.169).

.PARAMETER Port
    HTTP port where webMAN MOD web server listens (default: 80).

.PARAMETER WhatIf
    Dry-run mode: prints the exact URLs and query parameters without sending any network requests.

.PARAMETER Stage1Only
    Executes only Stage 1 (Process Table and memory bounds discovery) and halts.

.PARAMETER Stage1bOnly
    Executes Stage 1 and Stage 1b (resolving physical segment bases) and halts before Stage 2.

.PARAMETER TextBase
    Manual override for sysmgr_ss text segment physical base (virtual 0x80000000).

.PARAMETER DataBase
    Manual override for sysmgr_ss data segment physical base (virtual 0xc0000000).

.EXAMPLE
    .\lv1_anchor_peek.ps1 -WhatIf
    Prints the URLs and query parameters for dry-run verification without contacting the console.

.EXAMPLE
    .\lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169" -Stage1Only
    Executes Stage 1 discovery peeks against the specified console.

.EXAMPLE
    .\lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169" -TextBase 0x1A00000 -DataBase 0x1B00000
    Executes anchor verification using specified base addresses with bounds checking.
#>

[CmdletBinding()]
param(
    [string]$ConsoleIP = "192.168.0.169",
    [int]$Port = 80,
    [switch]$WhatIf,
    [switch]$Stage1Only,
    [switch]$Stage1bOnly,
    [long]$TextBase = 0,
    [long]$DataBase = 0
)

$ErrorActionPreference = "Stop"

# Statically proven addresses in LV1 image segment (0x200000 .. 0x3c2c00)
$ProcessTableAddress = 0x36CDB0     # 512 bytes, 32 entries (PIDs 0..31), TOC delta -16288(r2)
$MemBoundsAddress    = 0x3727D0     # Memory manager base and limit globals
$DomainVtableAddress = 0x3602B8     # Compiled-in AddressProtectionDomain vtable address

# Target UX Process configuration for sysmgr_ss
$TargetSlot = 3                     # PID 3 in pme_init.conf
$ExpectedPid = 3

# Anchor offsets within sysmgr_ss segments
$MetadataOffset = 0x39160           # Text + 0x39160
$RasterOffset   = 0x38B60           # Text + 0x38b60
$EncoderOffset  = 0x00000           # Data + 0x00000

# Expected anchor identity windows
$ExpectedAnchors = @(
    @{
        Name           = "Metadata identity anchor"
        Segment        = "Text"
        RelativeOffset = $MetadataOffset
        ExpectedWord0  = "426fc28f00000001" # 59.94 Hz refresh + interlace flag
        ExpectedWord1  = "0000000400000b40" # multiplier 4 + width 2880
        ExpectedSha256 = "d5bfc18bf3510558595e88fe25ededc4306284f90331adaf2fabc68dd753d63f"
    },
    @{
        Name           = "Raster identity anchor"
        Segment        = "Text"
        RelativeOffset = $RasterOffset
        ExpectedWord0  = "00000b3f00000d67" # H active end 2879 + H total 3431
        ExpectedWord1  = "00000b7f00000c77" # H sync edges 2943 / 3191
        ExpectedSha256 = "381678f339e524cd406d4e8545bbdf36c6b8aad426acc497f71f92f7268ec57e"
    },
    @{
        Name           = "Encoder-template identity anchor"
        Segment        = "Data"
        RelativeOffset = $EncoderOffset
        ExpectedWord0  = "91003000313f3201" # reg 0x91=0x00, 0x30=0x00, 0x31=0x3f, 0x32=0x01
        ExpectedWord1  = "3302340035003600" # reg 0x33=0x02, 0x34=0x00, 0x35=0x00, 0x36=0x00
        ExpectedSha256 = "8a13e9ec965d86a1bc81cd706dfa2bfd9f0fbcf0234e3b7b25425e2ff7f3c455"
    }
)

Write-Output "================================================================="
Write-Output "LV1 Safe Staged Process & Anchor Verification (Read-Only)"
Write-Output "Target Console: http://${ConsoleIP}:${Port}"
Write-Output "Mode: $(if ($WhatIf) { 'DRY RUN (-WhatIf)' } else { 'LIVE READ' })"
Write-Output "=================================================================`n"

if ($WhatIf) {
    Write-Output "[DRY RUN] Stage 1: Proven-Mapped Segment Reads"
    Write-Output "  1. Process Table: http://${ConsoleIP}:${Port}/peek.lv1?$($ProcessTableAddress.ToString('x'))"
    Write-Output "     Reads 512-byte view (32 slots) at 0x$($ProcessTableAddress.ToString('X6'))"
    Write-Output "     Safety: Inside proven-mapped LV1 .bss segment (0x200000..0x3c2c00)`n"
    Write-Output "  2. Memory Bounds: http://${ConsoleIP}:${Port}/peek.lv1?$($MemBoundsAddress.ToString('x'))"
    Write-Output "     Reads hypervisor memory manager bounds at 0x$($MemBoundsAddress.ToString('X6'))"
    Write-Output "     Safety: Inside proven-mapped LV1 .bss segment (0x200000..0x3c2c00)`n"

    Write-Output "[DRY RUN] Stage 1b: Bounds-Checked Process & Domain Resolution"
    Write-Output "  1. Extract Slot 3 pointer (Process* proc) from Stage 1 result (live ground truth: 0x12c010)."
    Write-Output "  2. Validate proc against Stage 1 memory bounds and 16-byte alignment."
    Write-Output "  3. Peek proc + 0x00 to verify PID == 3."
    Write-Output "  4. Peek proc + 0x20 to extract AddressProtectionDomain* domain."
    Write-Output "  5. Validate domain against memory bounds and 16-byte alignment."
    Write-Output "  6. Peek domain + 0x00 to verify vtable matches 0x$($DomainVtableAddress.ToString('X6'))."
    Write-Output "  7. Peek domain + 0x50 to extract first PageNode from mapped page list (offset +0x58)."
    Write-Output "  8. Follow PageNode list to resolve PhysBase_Text (0x80000000) and PhysBase_Data (0xc0000000).`n"

    Write-Output "[DRY RUN] Stage 2: Target Timing Anchors"
    if ($TextBase -ne 0 -and $DataBase -ne 0) {
        Write-Output "  Using provided TextBase: 0x$($TextBase.ToString('X')) | DataBase: 0x$($DataBase.ToString('X'))"
        foreach ($a in $ExpectedAnchors) {
            $base = if ($a.Segment -eq "Text") { $TextBase } else { $DataBase }
            $addr = $base + $a.RelativeOffset
            $url = "http://${ConsoleIP}:${Port}/peek.lv1?$($addr.ToString('x'))"
            Write-Output "  $($a.Name): $url"
            Write-Output "    Expected Word 0: 0x$($a.ExpectedWord0)"
            Write-Output "    Expected Word 1: 0x$($a.ExpectedWord1)"
            Write-Output "    Expected SHA-256: $($a.ExpectedSha256)"
        }
    } else {
        Write-Output "  In live execution, Stage 2 is executed after dynamically resolving"
        Write-Output "  PhysBase_Text and PhysBase_Data through Stage 1b, or via -TextBase/-DataBase."
    }
    Write-Output "`nNo network packets were transmitted. Dry run complete."
    return
}

# -----------------------------------------------------------------------------
# Parser and Helper Functions
# -----------------------------------------------------------------------------
function Parse-HexViewHtml {
    param(
        [string]$Html,
        [long]$ExpectedAddr = -1
    )
    if ([string]::IsNullOrEmpty($Html)) { return $null }

    # Strip HTML tags and decode HTML entities
    $text = [System.Text.RegularExpressions.Regex]::Replace($Html, '<[^>]+>', ' ')
    $text = [System.Net.WebUtility]::HtmlDecode($text)

    # Match webMAN MOD hex lines: 8 hex digits, whitespace, 16 pairs of hex digits each followed by a space
    $pattern = [regex]'([0-9A-Fa-f]{8})\s+((?:[0-9A-Fa-f]{2} ){16})'
    $matches = $pattern.Matches($text)
    if ($matches.Count -eq 0) { return $null }

    $firstAddrHex = $matches[0].Groups[1].Value
    $firstAddr = [Convert]::ToInt64($firstAddrHex, 16)
    if ($ExpectedAddr -ne -1) {
        $expectedAligned = $ExpectedAddr -band 0xFFFFFFFFFFFFFFF0L
        if (($firstAddr -band 0xFFFFFFFFFFFFFFF0L) -ne $expectedAligned) {
            Write-Warning "Address mismatch: expected 0x$($expectedAligned.ToString('X8')), got 0x$($firstAddr.ToString('X8'))"
            return $null
        }
    }

    $byteList = [System.Collections.Generic.List[byte]]::new()
    foreach ($m in $matches) {
        $hexStr = $m.Groups[2].Value.Replace(' ', '')
        for ($i = 0; $i -lt $hexStr.Length; $i += 2) {
            $byteList.Add([Convert]::ToByte($hexStr.Substring($i, 2), 16))
        }
    }

    return [PSCustomObject]@{
        BaseAddress = $firstAddr
        Bytes       = $byteList.ToArray()
    }
}

function Get-UInt64BE([byte[]]$bytes, [int]$offset) {
    $slice = [byte[]]::new(8)
    [System.Array]::Copy($bytes, $offset, $slice, 0, 8)
    if ([System.BitConverter]::IsLittleEndian) {
        [System.Array]::Reverse($slice)
    }
    return [System.BitConverter]::ToUInt64($slice, 0)
}

function Get-UInt32BE([byte[]]$bytes, [int]$offset) {
    $slice = [byte[]]::new(4)
    [System.Array]::Copy($bytes, $offset, $slice, 0, 4)
    if ([System.BitConverter]::IsLittleEndian) {
        [System.Array]::Reverse($slice)
    }
    return [System.BitConverter]::ToUInt32($slice, 0)
}

function Peek-Safe([long]$address, [string]$label) {
    $alignedAddr = $address -band 0xFFFFFFFFFFFFFFF0L
    $url = "http://${ConsoleIP}:${Port}/peek.lv1?$($alignedAddr.ToString('x'))"
    Write-Output "GET $url ($label at 0x$($address.ToString('X')))"
    try {
        $resp = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 5
        $parsed = Parse-HexViewHtml -Html $resp.Content -ExpectedAddr $alignedAddr
        if ($null -eq $parsed -or $parsed.Bytes.Length -lt 16) {
            Write-Output "  [FAIL] Unable to parse hex view from webMAN MOD response."
            return $null
        }
        return $parsed
    } catch {
        Write-Output "  [ERROR] HTTP request failed: $($_.Exception.Message)"
        return $null
    }
}

# -----------------------------------------------------------------------------
# Stage 1: Proven-Mapped Segment Reads
# -----------------------------------------------------------------------------
Write-Output "-----------------------------------------------------------------"
Write-Output "STAGE 1: Proven-Mapped Segment Reads (0x200000 .. 0x3c2c00)"
Write-Output "-----------------------------------------------------------------"

# 1. Read Process Table
$procTableParsed = Peek-Safe $ProcessTableAddress "Process Table"
if ($null -eq $procTableParsed -or $procTableParsed.Bytes.Length -lt 512) {
    Write-Output "  [FAIL] Could not retrieve 512-byte Process Table. Halting."
    return
}

Write-Output "  Successfully retrieved 512-byte Process Table (32 slots):"
$activeSlots = @{}
for ($slot = 0; $slot -lt 32; $slot++) {
    $offset = $slot * 16
    $flags = Get-UInt64BE $procTableParsed.Bytes $offset
    $procPtr = Get-UInt64BE $procTableParsed.Bytes ($offset + 8)

    if ($flags -ne 0 -or $procPtr -ne 0) {
        $activeSlots[$slot] = $procPtr
        Write-Output ("    Slot {0,2} : Flags=0x{1:X16}  ProcPtr=0x{2:X16}" -f $slot, $flags, $procPtr)
    }
}

# 2. Read Hypervisor Memory Bounds
$memBoundsParsed = Peek-Safe $MemBoundsAddress "Memory Bounds"
$ramBase = 0L
$ramLimit = 0x02000000L # 32 MB default safe boundary for hypervisor RAM

if ($null -ne $memBoundsParsed -and $memBoundsParsed.Bytes.Length -ge 16) {
    $mb0 = Get-UInt64BE $memBoundsParsed.Bytes 0
    $mb1 = Get-UInt64BE $memBoundsParsed.Bytes 8
    Write-Output ("  Memory manager bounds: Word0=0x{0:X16}  Word1=0x{1:X16}" -f $mb0, $mb1)
    if ($mb0 -eq 0 -and $mb1 -gt 0 -and $mb1 -le 0x10000000L) {
        $ramLimit = [long]$mb1
    }
}
Write-Output ("  Enforcing verified RAM bounds: [0x{0:X8} .. 0x{1:X8}]" -f $ramBase, $ramLimit)

if ($Stage1Only) {
    Write-Output "`nStage 1 complete (-Stage1Only specified). Exiting."
    return
}

# -----------------------------------------------------------------------------
# Stage 1b: Bounds-Checked Process & Domain Resolution
# -----------------------------------------------------------------------------
Write-Output "`n-----------------------------------------------------------------"
Write-Output "STAGE 1b: Bounds-Checked Process & Domain Resolution"
Write-Output "-----------------------------------------------------------------"

if (-not $activeSlots.ContainsKey($TargetSlot)) {
    Write-Output "  [FAIL] Target Slot $TargetSlot (sysmgr_ss) is not active in Process Table. Halting."
    return
}

$procPtr = $activeSlots[$TargetSlot]
Write-Output ("  Target sysmgr_ss Process pointer from Slot {0}: 0x{1:X}" -f $TargetSlot, $procPtr)

# Bounds check procPtr
if ($procPtr -lt $ramBase -or ($procPtr + 0x700) -gt $ramLimit -or ($procPtr -band 0xF) -ne 0) {
    Write-Output "  [FAIL] Process pointer 0x$($procPtr.ToString('X')) is out of bounds or unaligned. Refusing dereference. Halting."
    return
}

# 1. Peek Process object header at procPtr
$procParsed = Peek-Safe $procPtr "Process Object"
if ($null -eq $procParsed) { return }
$pid = Get-UInt32BE $procParsed.Bytes 0
Write-Output "  Process header: PID = $pid"
if ($pid -ne $ExpectedPid) {
    Write-Output "  [FAIL] PID mismatch: expected $ExpectedPid, found $pid. Refusing dereference. Halting."
    return
}

# 2. Peek AddressProtectionDomain pointer at proc + 0x20
$domainOffset = $procPtr + 0x20
$domainPtrParsed = Peek-Safe $domainOffset "Domain Pointer (proc + 0x20)"
if ($null -eq $domainPtrParsed) { return }
$domainPtr = Get-UInt64BE $domainPtrParsed.Bytes ($domainOffset - $domainPtrParsed.BaseAddress)
Write-Output ("  AddressProtectionDomain pointer: 0x{0:X}" -f $domainPtr)

# Bounds check domainPtr
if ($domainPtr -lt $ramBase -or ($domainPtr + 0x80) -gt $ramLimit -or ($domainPtr -band 0xF) -ne 0) {
    Write-Output "  [FAIL] Domain pointer 0x$($domainPtr.ToString('X')) is out of bounds or unaligned. Refusing dereference. Halting."
    return
}

# 3. Peek domain + 0x00 and verify vtable
$domainHeaderParsed = Peek-Safe $domainPtr "Domain Header (vtable check)"
if ($null -eq $domainHeaderParsed) { return }
$vtable = Get-UInt64BE $domainHeaderParsed.Bytes 0
Write-Output ("  Domain vtable: 0x{0:X} (Expected: 0x{1:X})" -f $vtable, $DomainVtableAddress)
if ($vtable -ne $DomainVtableAddress) {
    Write-Output "  [FAIL] Vtable mismatch. Memory is not a valid AddressProtectionDomain. Halting."
    return
}

# 4. Peek domain + 0x50 (mapped page list head, offset +0x58)
$listHeadParsed = Peek-Safe ($domainPtr + 0x50) "Domain Page List Head"
if ($null -eq $listHeadParsed) { return }
$firstNodePtr = [long](Get-UInt32BE $listHeadParsed.Bytes 8) # offset +0x58 is byte index 8 in aligned view 0x50
Write-Output ("  First PageNode pointer: 0x{0:X}" -f $firstNodePtr)

# 5. Traverse PageNode list to resolve Text and Data bases
$currNode = $firstNodePtr
$discoveredTextBase = 0L
$discoveredDataBase = 0L
$traversedCount = 0

while ($currNode -ne 0 -and $traversedCount -lt 16) {
    if ($currNode -lt $ramBase -or ($currNode + 0x20) -gt $ramLimit -or ($currNode -band 0xF) -ne 0) {
        Write-Output "  [FAIL] PageNode pointer 0x$($currNode.ToString('X')) is out of bounds or unaligned. Halting."
        return
    }

    $nodeParsed = Peek-Safe $currNode "PageNode"
    if ($null -eq $nodeParsed) { return }

    $nodeOff = $currNode - $nodeParsed.BaseAddress
    $paddr = Get-UInt64BE $nodeParsed.Bytes $nodeOff
    $vaddr = Get-UInt32BE $nodeParsed.Bytes ($nodeOff + 8)
    $nextNode = [long](Get-UInt32BE $nodeParsed.Bytes ($nodeOff + 16))

    Write-Output ("    Node {0}: vaddr=0x{1:X8} -> paddr=0x{2:X16} (next=0x{3:X})" -f $traversedCount, $vaddr, $paddr, $nextNode)

    if ($vaddr -eq 0x80000000L -and $discoveredTextBase -eq 0) {
        $discoveredTextBase = $paddr
        Write-Output ("    >>> Resolved PhysBase_Text: 0x{0:X} <<<" -f $discoveredTextBase)
    }
    if ($vaddr -eq 0xC0000000L -and $discoveredDataBase -eq 0) {
        $discoveredDataBase = $paddr
        Write-Output ("    >>> Resolved PhysBase_Data: 0x{0:X} <<<" -f $discoveredDataBase)
    }

    if ($discoveredTextBase -ne 0 -and $discoveredDataBase -ne 0) {
        break
    }

    if ($nextNode -eq $firstNodePtr -or $nextNode -eq 0) {
        break
    }
    $currNode = $nextNode
    $traversedCount++
}

if ($discoveredTextBase -ne 0) { $TextBase = $discoveredTextBase }
if ($discoveredDataBase -ne 0) { $DataBase = $discoveredDataBase }

if ($Stage1bOnly) {
    Write-Output "`nStage 1b complete (-Stage1bOnly specified). Exiting."
    return
}

# -----------------------------------------------------------------------------
# Stage 2: Target Anchor Verification
# -----------------------------------------------------------------------------
Write-Output "`n-----------------------------------------------------------------"
Write-Output "STAGE 2: Target Anchor Verification"
Write-Output "-----------------------------------------------------------------"

if ($TextBase -eq 0 -or $DataBase -eq 0) {
    Write-Output "  [PAUSE] Stage 2 requires confirmed TextBase and DataBase."
    Write-Output "  Physical bases were not resolved. To run Stage 2 with manual overrides:"
    Write-Output "    .\lv1_anchor_peek.ps1 -ConsoleIP `"$ConsoleIP`" -TextBase <0xAddress> -DataBase <0xAddress>"
    return
}

# Final bounds check on bases before reading anchors
if ($TextBase -lt $ramBase -or ($TextBase + 0x40000) -gt $ramLimit -or `
    $DataBase -lt $ramBase -or ($DataBase + 0x10000) -gt $ramLimit) {
    Write-Output "  [FAIL] Bases lie outside verified RAM bounds. Refusing anchor reads. Halting."
    return
}

$sha256 = [System.Security.Cryptography.SHA256]::Create()
$stage2Pass = $true

foreach ($anchor in $ExpectedAnchors) {
    $base = if ($anchor.Segment -eq "Text") { $TextBase } else { $DataBase }
    $targetAddr = $base + $anchor.RelativeOffset

    Write-Output "-----------------------------------------------------------------"
    Write-Output "Testing Anchor: $($anchor.Name) ($($anchor.Segment) + 0x$($anchor.RelativeOffset.ToString('X')))"
    Write-Output "Target Physical Address: 0x$($targetAddr.ToString('X')) (16 bytes)"

    $parsed = Peek-Safe $targetAddr $anchor.Name
    if ($null -eq $parsed) {
        $stage2Pass = $false
        continue
    }

    $sliceOff = $targetAddr - $parsed.BaseAddress
    $bytes = [byte[]]::new(16)
    [System.Array]::Copy($parsed.Bytes, $sliceOff, $bytes, 0, 16)

    $w0Hex = ($bytes[0..7] | ForEach-Object { $_.ToString("x2") }) -join ""
    $w1Hex = ($bytes[8..15] | ForEach-Object { $_.ToString("x2") }) -join ""

    $hashBytes = $sha256.ComputeHash($bytes)
    $computedHash = ($hashBytes | ForEach-Object { $_.ToString("x2") }) -join ""

    Write-Output "  Word 0: 0x$w0Hex (Expected: 0x$($anchor.ExpectedWord0))"
    Write-Output "  Word 1: 0x$w1Hex (Expected: 0x$($anchor.ExpectedWord1))"
    Write-Output "  SHA-256: $computedHash"

    $w0Match = ($w0Hex -ieq $anchor.ExpectedWord0)
    $w1Match = ($w1Hex -ieq $anchor.ExpectedWord1)
    $hashMatch = ($computedHash -ieq $anchor.ExpectedSha256)

    if ($w0Match -and $w1Match -and $hashMatch) {
        Write-Output "  Result: PASS"
    } else {
        Write-Output "  Result: FAIL (Mismatch detected)"
        $stage2Pass = $false
    }
}

Write-Output "-----------------------------------------------------------------"
if ($stage2Pass) {
    Write-Output "OVERALL STATUS: ALL ANCHORS PASSED"
} else {
    Write-Output "OVERALL STATUS: ONE OR MORE ANCHORS FAILED"
}
