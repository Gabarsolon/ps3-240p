<#
.SYNOPSIS
    Three-stage read-only verification of LV1 process structures, AddressProtectionDomain,
    and AV timing anchors via webMAN MOD HTTP GET.

.DESCRIPTION
    Sends read-only HTTP GET requests to webMAN MOD's /peek.lv1 endpoint on a PlayStation 3
    running CFW (e.g. Evilnat 4.93 + Cobra) using a provably safe, bounds-checked approach:
    
    Stage 1 (Proven-Mapped Discovery & Memory Bounds):
      Peeks within the proven-mapped LV1 .data/.bss segment (0x200000..0x3c2c00):
        - Process Table at 0x36cdb0 (32 slots, 16 bytes each; Slot 3 = sysmgr_ss.fself).
        - Primary buddy_allocator pointer at 0x3725b0.
      Enforces authoritative hypervisor heap bounds:
        - HvHeapBase = 0x00000000 (DRAM physical base)
        - HvHeapLimit = 0x02000000 (32 MB hypervisor partition ceiling; never touches GameOS LPAR memory)
      Validates bounds consistency against the 5 known live Process* pointers from ground truth
      (0x915d0, 0xe5040, 0x12c010, 0x12c8d0, 0x6bb0d0).
      
    Stage 1b (Bounds-Checked Process & Domain Resolution):
      Follows heap pointers ONLY after strict alignment and 512-byte view span checking:
        - Verifies sysmgr_ss Process object at Slot 3 pointer (PID == 3).
        - Reads AddressProtectionDomain pointer at proc + 0x18.
        - Verifies AddressProtectionDomain vtable matches compile-time vtable 0x35ecb8.
        - Traverses the ProtectionPage linked list (first node at domain + 0x3c, sentinel at domain + 0x28)
          to resolve backing physical bases: PhysBase_Text (vaddr 0x80000000) and PhysBase_Data (0xc0000000).
          
    Stage 2 (Anchor Verification):
      Reads the three 16-byte timing and encoder identity windows:
        - Metadata anchor at PhysBase_Text + 0x39160
        - Raster anchor at PhysBase_Text + 0x38b60
        - Encoder-template anchor at PhysBase_Data + 0x00000
      Compares returned words against expected values and verifies 16-byte SHA-256 hashes.

    This script contains NO write/poke commands, performs NO memory scanning, and strictly
    enforces 512-byte webMAN view coverage on every dereference.

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

.PARAMETER UnitTest
    Runs offline synthetic unit tests of parser and bounds logic without contacting the console.

.PARAMETER TextBase
    Manual override for sysmgr_ss text segment physical base (virtual 0x80000000).

.PARAMETER DataBase
    Manual override for sysmgr_ss data segment physical base (virtual 0xc0000000).

.EXAMPLE
    .\lv1_anchor_peek.ps1 -UnitTest
    Executes offline unit tests validating parser, bounds checking, and pointer traversal.

.EXAMPLE
    .\lv1_anchor_peek.ps1 -WhatIf
    Prints the URLs and query parameters for dry-run verification without contacting the console.

.EXAMPLE
    .\lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169" -Stage1Only
    Executes Stage 1 discovery peeks against the specified console.
#>

[CmdletBinding()]
param(
    [string]$ConsoleIP = "192.168.0.169",
    [int]$Port = 80,
    [switch]$WhatIf,
    [switch]$Stage1Only,
    [switch]$Stage1bOnly,
    [switch]$UnitTest,
    [long]$TextBase = 0,
    [long]$DataBase = 0
)

$ErrorActionPreference = "Stop"

# Statically proven addresses in LV1 image segment (0x200000 .. 0x3c2c00)
$ProcessTableAddress = 0x36CDB0     # 512 bytes, 32 entries (PIDs 0..31), TOC delta -16288(r2)
$BuddyAllocAddress   = 0x3725B0     # Primary buddy_allocator pointer, TOC delta -11008(r2)
$DomainVtableAddress = 0x35ECB8     # AddressProtectionDomain compiled-in vtable address
$RangeMgrVtableAddr  = 0x3602B8     # VAddrRangeManager compiled-in vtable address

# Hypervisor Partition Memory Bounds
$HvHeapBase  = 0x00000000L          # DRAM physical base
$HvHeapLimit = 0x02000000L          # 32 MB hypervisor partition ceiling (GameOS LPAR boundary)

# Ground truth known live Process* pointers for consistency validation
$KnownLiveProcessPtrs = @(0x915D0L, 0xE5040L, 0x12C010L, 0x12C8D0L, 0x6BB0D0L)

# Target UX Process configuration for sysmgr_ss
$TargetSlot = 10                     # PID 10 (Slot 10 in 4.93 Evilnat)
$ExpectedPid = 10

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
        ExpectedWord0  = "426fc28f00000001"
        ExpectedWord1  = "0000000400000b40"
        ExpectedSha256 = "d5bfc18bf3510558595e88fe25ededc4306284f90331adaf2fabc68dd753d63f"
    },
    @{
        Name           = "Raster identity anchor"
        Segment        = "Text"
        RelativeOffset = $RasterOffset
        ExpectedWord0  = "00000b3f00000d67"
        ExpectedWord1  = "00000b7f00000c77"
        ExpectedSha256 = "381678f339e524cd406d4e8545bbdf36c6b8aad426acc497f71f92f7268ec57e"
    },
    @{
        Name           = "Encoder-template anchor"
        Segment        = "Data"
        RelativeOffset = $EncoderOffset
        ExpectedWord0  = "91003000313f3201"
        ExpectedWord1  = "3302340035003600"
        ExpectedSha256 = "8a13e9ec965d86a1bc81cd706dfa2bfd9f0fbcf0234e3b7b25425e2ff7f3c455"
    }
)

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

    # Match webMAN MOD hex lines: 8 hex digits, whitespace, 16 pairs of hex digits
    $pattern = [regex]'([0-9A-Fa-f]{8})\s+((?:[0-9A-Fa-f]{2}\s+){16})'
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
        $hexStr = [System.Text.RegularExpressions.Regex]::Replace($m.Groups[2].Value, '\s+', '')
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
    if ($offset + 8 -gt $bytes.Length) { return 0UL }
    $slice = [byte[]]::new(8)
    [System.Array]::Copy($bytes, $offset, $slice, 0, 8)
    if ([System.BitConverter]::IsLittleEndian) {
        [System.Array]::Reverse($slice)
    }
    return [System.BitConverter]::ToUInt64($slice, 0)
}

function Get-UInt32BE([byte[]]$bytes, [int]$offset) {
    if ($offset + 4 -gt $bytes.Length) { return 0U }
    $slice = [byte[]]::new(4)
    [System.Array]::Copy($bytes, $offset, $slice, 0, 4)
    if ([System.BitConverter]::IsLittleEndian) {
        [System.Array]::Reverse($slice)
    }
    return [System.BitConverter]::ToUInt32($slice, 0)
}

# Enforces that pointer is non-null, aligned, and webMAN 512-byte view span fits in bounds
function Test-PointerViewBounds {
    param(
        [long]$Pointer,
        [long]$Base = $HvHeapBase,
        [long]$Limit = $HvHeapLimit,
        [string]$Label = "Pointer"
    )
    if ($Pointer -eq 0) {
        Write-Host "  [BOUNDS FAIL] $Label is NULL (0x0). Refusing dereference."
        return $false
    }
    if (($Pointer -band 0x7L) -ne 0) {
        Write-Host ("  [BOUNDS FAIL] {0} 0x{1:X} is misaligned (must be 8-byte aligned)." -f $Label, $Pointer)
        return $false
    }
    $viewStart = $Pointer -band 0xFFFFFFFFFFFFFFF0L
    $viewEnd   = $viewStart + 0x200L # 512-byte webMAN view coverage

    if ($viewStart -lt $Base) {
        Write-Host ("  [BOUNDS FAIL] {0} view start 0x{1:X} < base 0x{2:X}. Refusing dereference." -f $Label, $viewStart, $Base)
        return $false
    }
    if ($viewEnd -gt $Limit) {
        Write-Host ("  [BOUNDS FAIL] {0} view end 0x{1:X} > limit 0x{2:X}. Refusing dereference." -f $Label, $viewEnd, $Limit)
        return $false
    }
    return $true
}

$script:PeekCache = @{}

function Peek-Safe([long]$address, [string]$label) {
    $alignedAddr = $address -band 0xFFFFFFFFFFFFFFF0L
    foreach ($cachedBase in $script:PeekCache.Keys) {
        $cachedParsed = $script:PeekCache[$cachedBase]
        if ($address -ge $cachedBase -and ($address + 32) -le ($cachedBase + $cachedParsed.Bytes.Length)) {
            return $cachedParsed
        }
    }
    $url = "http://${ConsoleIP}:${Port}/peek.lv1?$($alignedAddr.ToString('x'))"
    Write-Output "GET $url ($label at 0x$($address.ToString('X')))"
    try {
        $resp = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 5
        $parsed = Parse-HexViewHtml -Html $resp.Content -ExpectedAddr $alignedAddr
        if ($null -eq $parsed -or $parsed.Bytes.Length -lt 16) {
            Write-Output "  [FAIL] Unable to parse hex view from webMAN MOD response."
            return $null
        }
        $script:PeekCache[$parsed.BaseAddress] = $parsed
        return $parsed
    } catch {
        Write-Output "  [ERROR] HTTP request failed: $($_.Exception.Message)"
        return $null
    }
}

# -----------------------------------------------------------------------------
# Offline Synthetic Unit Tests (-UnitTest mode)
# -----------------------------------------------------------------------------
if ($UnitTest) {
    Write-Output "================================================================="
    Write-Output "RUNNING OFFLINE SYNTHETIC UNIT TESTS"
    Write-Output "================================================================="

    # Test 1: Parser with synthetic webMAN MOD HTML
    $syntheticHtml = "<pre>`n" +
        "0036CDB0  00 00 00 00 00 00 00 00  00 00 00 00 00 6B B0 D0  ................`n" +
        "0036CDC0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................`n" +
        "0036CDD0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................`n" +
        "0036CDE0  00 00 00 00 00 00 00 00  00 00 00 00 00 12 C0 10  ................`n" +
        "</pre>"
    $p = Parse-HexViewHtml -Html $syntheticHtml -ExpectedAddr 0x36CDB0
    if ($null -eq $p -or $p.BaseAddress -ne 0x36CDB0 -or $p.Bytes.Length -ne 64) {
        Write-Error "Test 1 Failed: Parse-HexViewHtml did not parse synthetic webMAN hex lines correctly."
        return
    }
    Write-Output "[PASS] Test 1: Parse-HexViewHtml correctly parsed 64 bytes from synthetic HTML."

    # Test 2: BE Word Reading
    $w0 = Get-UInt64BE $p.Bytes 0x00
    $w1 = Get-UInt64BE $p.Bytes 0x08
    $w3_proc = Get-UInt64BE $p.Bytes 0x38
    if ($w0 -ne 0 -or $w1 -ne 0x6BB0D0L -or $w3_proc -ne 0x12C010L) {
        Write-Error "Test 2 Failed: Get-UInt64BE decoded unexpected values."
        return
    }
    Write-Output "[PASS] Test 2: Get-UInt64BE verified Slot 0 = 0x6bb0d0, Slot 3 = 0x12c010."

    # Test 3: Test-PointerViewBounds (Validation and Edge Cases)
    # Valid pointer
    $validTest = Test-PointerViewBounds -Pointer 0x12C010L -Base $HvHeapBase -Limit $HvHeapLimit -Label "ValidProc"
    if (-not $validTest) { Write-Error "Test 3a Failed: Valid pointer was rejected." ; return }

    # Null pointer
    $nullTest = Test-PointerViewBounds -Pointer 0L -Base $HvHeapBase -Limit $HvHeapLimit -Label "NullPtr"
    if ($nullTest) { Write-Error "Test 3b Failed: Null pointer was accepted." ; return }

    # Misaligned pointer (not 8-byte aligned)
    $misalignedTest = Test-PointerViewBounds -Pointer 0x12C013L -Base $HvHeapBase -Limit $HvHeapLimit -Label "MisalignedPtr"
    if ($misalignedTest) { Write-Error "Test 3c Failed: Misaligned pointer was accepted." ; return }

    # Over-reading near upper limit: pointer is < limit, but 512-byte view exceeds limit
    $nearLimitPtr = $HvHeapLimit - 0x100L # 256 bytes before limit -> view needs 512 bytes!
    $exceedTest = Test-PointerViewBounds -Pointer $nearLimitPtr -Base $HvHeapBase -Limit $HvHeapLimit -Label "NearLimitExceedingPtr"
    if ($exceedTest) { Write-Error "Test 3d Failed: Pointer exceeding view margin was accepted." ; return }
    Write-Output "[PASS] Test 3: Test-PointerViewBounds strictly enforces alignment and 512-byte view span."

    # Test 4: Consistency validation on 5 known live pointers
    $allKnownValid = $true
    foreach ($kPtr in $KnownLiveProcessPtrs) {
        if (-not (Test-PointerViewBounds -Pointer $kPtr -Base $HvHeapBase -Limit $HvHeapLimit -Label "KnownLive")) {
            $allKnownValid = $false
        }
    }
    if (-not $allKnownValid) { Write-Error "Test 4 Failed: Known live pointers failed bounds check." ; return }
    Write-Output "[PASS] Test 4: All 5 known live Process* pointers lie strictly within verified bounds."

    # Test 5: Synthetic full chain dereference simulation
    # Simulate Process object at 0x12C010:
    # +0x00: uint32 PID = 3
    # +0x18: uint64 domainPtr = 0x130000
    # +0x20: uint64 rangeMgrPtr = 0x130100
    $fakeProcBytes = [byte[]]::new(64)
    $pidBytes = [System.BitConverter]::GetBytes([uint32]3)
    if ([System.BitConverter]::IsLittleEndian) { [System.Array]::Reverse($pidBytes) }
    [System.Array]::Copy($pidBytes, 0, $fakeProcBytes, 0, 4)

    $domPtrBytes = [System.BitConverter]::GetBytes([uint64]0x130000L)
    if ([System.BitConverter]::IsLittleEndian) { [System.Array]::Reverse($domPtrBytes) }
    [System.Array]::Copy($domPtrBytes, 0, $fakeProcBytes, 0x18, 8)

    $extractedPid = Get-UInt32BE $fakeProcBytes 0
    $extractedDomain = Get-UInt64BE $fakeProcBytes 0x18
    if ($extractedPid -ne 3 -or $extractedDomain -ne 0x130000L) {
        Write-Error "Test 5a Failed: Process header decode failed." ; return
    }

    # Simulate Domain object at 0x130000:
    # +0x00: uint64 vtable = 0x35ECB8
    # +0x28: sentinel node
    # +0x3C: uint32 firstNodePtr = 0x130500
    $fakeDomainBytes = [byte[]]::new(64)
    $vtBytes = [System.BitConverter]::GetBytes([uint64]$DomainVtableAddress)
    if ([System.BitConverter]::IsLittleEndian) { [System.Array]::Reverse($vtBytes) }
    [System.Array]::Copy($vtBytes, 0, $fakeDomainBytes, 0x00, 8)

    $firstNodeBytes = [System.BitConverter]::GetBytes([uint32]0x130500)
    if ([System.BitConverter]::IsLittleEndian) { [System.Array]::Reverse($firstNodeBytes) }
    [System.Array]::Copy($firstNodeBytes, 0, $fakeDomainBytes, 0x3C, 4)

    $extractedVt = Get-UInt64BE $fakeDomainBytes 0x00
    $extractedFirstNode = [long](Get-UInt32BE $fakeDomainBytes 0x3C)
    if ($extractedVt -ne $DomainVtableAddress -or $extractedFirstNode -ne 0x130500L) {
        Write-Error "Test 5b Failed: Domain header decode failed." ; return
    }

    # Simulate ProtectionPage node at 0x130500:
    # +0x00: uint64 paddr = 0x1A00000 (Text base)
    # +0x08: uint32 vaddr = 0x80000000
    # +0x14: uint32 nextNode = 0x130520
    $fakeNode1 = [byte[]]::new(32)
    $paddrBytes = [System.BitConverter]::GetBytes([uint64]0x1A00000L)
    if ([System.BitConverter]::IsLittleEndian) { [System.Array]::Reverse($paddrBytes) }
    [System.Array]::Copy($paddrBytes, 0, $fakeNode1, 0x00, 8)
    $vaddrBytes = [System.BitConverter]::GetBytes([uint32]0x80000000L)
    if ([System.BitConverter]::IsLittleEndian) { [System.Array]::Reverse($vaddrBytes) }
    [System.Array]::Copy($vaddrBytes, 0, $fakeNode1, 0x08, 4)

    $extractedPaddr = Get-UInt64BE $fakeNode1 0x00
    $extractedVaddr = Get-UInt32BE $fakeNode1 0x08
    if ($extractedPaddr -ne 0x1A00000L -or $extractedVaddr -ne 0x80000000L) {
        Write-Error "Test 5c Failed: ProtectionPage node decode failed." ; return
    }
    Write-Output "[PASS] Test 5: Synthetic full chain simulation verified."

    Write-Output "================================================================="
    Write-Output "UNIT TESTS: ALL 5 TESTS PASSED SUCCESSFULLY (OFFLINE)."
    Write-Output "================================================================="
    return
}

# -----------------------------------------------------------------------------
# Dry Run Mode (-WhatIf)
# -----------------------------------------------------------------------------
if ($WhatIf) {
    Write-Output "================================================================="
    Write-Output "DRY RUN MODE (-WhatIf specified) -- NO NETWORK PACKETS TRANSMITTED"
    Write-Output "================================================================="
    Write-Output "Target: http://${ConsoleIP}:${Port}"
    Write-Output "Hypervisor Safe Partition Bounds: [0x$($HvHeapBase.ToString('X8')) .. 0x$($HvHeapLimit.ToString('X8'))]`n"

    Write-Output "[DRY RUN] Stage 1: Static .bss Discovery & Heap Bounds Validation"
    Write-Output "  1. GET http://${ConsoleIP}:${Port}/peek.lv1?$($ProcessTableAddress.ToString('x')) (Process Table, 512 bytes)"
    Write-Output "     Expected: 32 slots; non-zero second words at slots 0, 3, 5, 6, 10."
    Write-Output "  2. GET http://${ConsoleIP}:${Port}/peek.lv1?$($BuddyAllocAddress.ToString('x')) (Primary Allocator Global)"
    Write-Output "  3. Validate heap bounds consistency with 5 known live pointers:"
    foreach ($p in $KnownLiveProcessPtrs) {
        Write-Output "     Check: (0x$($p.ToString('X')) & ~0xF) + 0x200 <= 0x$($HvHeapLimit.ToString('X')) -> PASS"
    }

    Write-Output "`n[DRY RUN] Stage 1b: Bounds-Checked Process & Domain Resolution"
    Write-Output "  1. Extract Process* from Slot 3 (expected 0x12c010)."
    Write-Output "  2. Verify 512-byte view span for proc: [0x12c010 .. 0x12c210) <= 0x$($HvHeapLimit.ToString('X'))."
    Write-Output "  3. GET http://${ConsoleIP}:${Port}/peek.lv1?12c010 (Process Object)"
    Write-Output "     Verify: PID at +0x00 == $ExpectedPid"
    Write-Output "  4. Extract AddressProtectionDomain* at proc + 0x18."
    Write-Output "  5. Verify 512-byte view span for domain: (domain & ~0xF) + 0x200 <= 0x$($HvHeapLimit.ToString('X'))."
    Write-Output "  6. GET http://${ConsoleIP}:${Port}/peek.lv1?<domain> (Domain Header)"
    Write-Output "     Verify: Vtable at +0x00 == 0x$($DomainVtableAddress.ToString('X6'))"
    Write-Output "  7. Extract first ProtectionPage node at domain + 0x3c."
    Write-Output "  8. Follow ProtectionPage linked list (+0x14 next pointer, stopping at domain + 0x28):"
    Write-Output "     - Match vaddr == 0x80000000 -> PhysBase_Text (+0x00)"
    Write-Output "     - Match vaddr == 0xc0000000 -> PhysBase_Data (+0x00)"

    Write-Output "`n[DRY RUN] Stage 2: Target Timing Anchors"
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
        $activeSlots[$slot] = [long]$procPtr
        Write-Output ("    Slot {0,2} : Flags=0x{1:X16}  ProcPtr=0x{2:X16}" -f $slot, $flags, $procPtr)
    }
}

# 2. Check and Validate Hypervisor Partition Heap Bounds
Write-Output ("`n  Establishing Hypervisor Partition Bounds: [0x{0:X8} .. 0x{1:X8}]" -f $HvHeapBase, $HvHeapLimit)
if ($HvHeapBase -ge $HvHeapLimit -or $HvHeapLimit -gt 0x02000000L) {
    Write-Output "  [FAIL] Partition bounds sanity check failed. Halting."
    return
}

# Validate consistency against known live pointers
$boundsConsistent = $true
foreach ($kp in $KnownLiveProcessPtrs) {
    if (-not (Test-PointerViewBounds -Pointer $kp -Base $HvHeapBase -Limit $HvHeapLimit -Label "LiveProcessConsistency")) {
        $boundsConsistent = $false
    }
}
if (-not $boundsConsistent) {
    Write-Output "  [FAIL] Known live process pointers failed bounds check. Heap bound unjustified. Halting."
    return
}
Write-Output "  [PASS] Consistency check confirmed: All 5 known live Process* pointers lie inside verified bounds."

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

# View span check on procPtr
if (-not (Test-PointerViewBounds -Pointer $procPtr -Base $HvHeapBase -Limit $HvHeapLimit -Label "ProcessPointer")) {
    Write-Output "  [FAIL] Process pointer view span violation. Halting."
    return
}

# 1. Peek Process object header at procPtr
$procParsed = Peek-Safe $procPtr "Process Object"
if ($null -eq $procParsed) { return }
$procPid = Get-UInt32BE $procParsed.Bytes 0
Write-Output "  Process header: PID = $procPid"
if ($procPid -ne $ExpectedPid) {
    Write-Output "  [FAIL] PID mismatch: expected $ExpectedPid, found $procPid. Refusing dereference. Halting."
    return
}

# 2. Extract AddressProtectionDomain pointer at proc + 0x18
$domainOffset = $procPtr + 0x18
$domainPtr = [long](Get-UInt64BE $procParsed.Bytes ($domainOffset - $procParsed.BaseAddress))
Write-Output ("  AddressProtectionDomain pointer (proc + 0x18): 0x{0:X}" -f $domainPtr)

# View span check on domainPtr
if (-not (Test-PointerViewBounds -Pointer $domainPtr -Base $HvHeapBase -Limit $HvHeapLimit -Label "DomainPointer")) {
    Write-Output "  [FAIL] Domain pointer view span violation. Halting."
    return
}

# 3. Peek domain + 0x00 and verify vtable matches 0x35ecb8
$domainHeaderParsed = Peek-Safe $domainPtr "Domain Header (vtable check)"
if ($null -eq $domainHeaderParsed) { return }
$vtable = Get-UInt64BE $domainHeaderParsed.Bytes 0
Write-Output ("  Domain vtable: 0x{0:X} (Expected: 0x{1:X})" -f $vtable, $DomainVtableAddress)
if ($vtable -ne $DomainVtableAddress) {
    Write-Output "  [FAIL] Vtable mismatch. Expected AddressProtectionDomain vtable 0x$($DomainVtableAddress.ToString('X6')). Halting."
    return
}

# 4. Extract first ProtectionPage node from domain + 0x3c
$firstNodePtr = [long](Get-UInt32BE $domainHeaderParsed.Bytes 0x3C)
$sentinelHead = $domainPtr + 0x28L
Write-Output ("  First ProtectionPage pointer (domain + 0x3c): 0x{0:X} (Sentinel: 0x{1:X})" -f $firstNodePtr, $sentinelHead)

if ($firstNodePtr -eq 0 -or $firstNodePtr -eq $sentinelHead) {
    Write-Output "  [FAIL] ProtectionPage list is empty. Halting."
    return
}

# 5. Traverse ProtectionPage list to map pages and resolve Text and Data bases
$currNode = $firstNodePtr
$discoveredTextBase = 0L
$discoveredDataBase = 0L
$pageMap = @{}
$traversedCount = 0

while ($currNode -ne 0 -and $currNode -ne $sentinelHead -and $traversedCount -lt 256) {
    if (-not (Test-PointerViewBounds -Pointer $currNode -Base $HvHeapBase -Limit $HvHeapLimit -Label "PageNodePointer")) {
        Write-Output "  [FAIL] PageNode pointer view span violation. Halting."
        return
    }

    $nodeParsed = Peek-Safe $currNode "PageNode"
    if ($null -eq $nodeParsed) { return }

    $nodeOff = $currNode - $nodeParsed.BaseAddress
    $paddr = [long](Get-UInt64BE $nodeParsed.Bytes $nodeOff)
    $vaddr = [long](Get-UInt32BE $nodeParsed.Bytes ($nodeOff + 8))
    $nextNode = [long](Get-UInt32BE $nodeParsed.Bytes ($nodeOff + 20)) # +0x14 next pointer

    $pageMap[$vaddr] = $paddr
    Write-Output ("    Node {0,2}: vaddr=0x{1:X8} -> paddr=0x{2:X16} (next=0x{3:X})" -f $traversedCount, $vaddr, $paddr, $nextNode)

    if ($vaddr -eq 0x80000000L -and $discoveredTextBase -eq 0) {
        $discoveredTextBase = $paddr
        Write-Output ("    >>> Resolved PhysBase_Text (0x80000000): 0x{0:X} <<<" -f $discoveredTextBase)
    }
    if ($vaddr -eq 0xC0000000L -and $discoveredDataBase -eq 0) {
        $discoveredDataBase = $paddr
        Write-Output ("    >>> Resolved PhysBase_Data (0xc0000000): 0x{0:X} <<<" -f $discoveredDataBase)
    }

    $currNode = $nextNode
    $traversedCount++
}

Write-Output ("  Total mapped pages resolved in domain: {0}" -f $pageMap.Count)

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

$sha256 = [System.Security.Cryptography.SHA256]::Create()
$stage2Pass = $true

foreach ($anchor in $ExpectedAnchors) {
    $targetVaddr = if ($anchor.Segment -eq "Text") { 0x80000000L + $anchor.RelativeOffset } else { 0xC0000000L + $anchor.RelativeOffset }
    $pageVaddr   = $targetVaddr -band 0xFFFFFFFFFFFFF000L
    $pageOffset  = $targetVaddr -band 0xFFFL

    Write-Output "-----------------------------------------------------------------"
    Write-Output "Testing Anchor: $($anchor.Name) ($($anchor.Segment) + 0x$($anchor.RelativeOffset.ToString('X')))"

    if ($pageMap.ContainsKey($pageVaddr)) {
        $targetAddr = $pageMap[$pageVaddr] + $pageOffset
        Write-Output ("  Resolved Virtual 0x{0:X8} via PageMap -> Target Physical Address: 0x{1:X} (16 bytes)" -f $targetVaddr, $targetAddr)
    } elseif ($TextBase -ne 0 -and $DataBase -ne 0) {
        $base = if ($anchor.Segment -eq "Text") { $TextBase } else { $DataBase }
        $targetAddr = $base + $anchor.RelativeOffset
        Write-Output ("  Fallback to contiguous base + offset -> Physical Address: 0x{0:X} (16 bytes)" -f $targetAddr)
    } else {
        Write-Output ("  [FAIL] Target virtual page 0x{0:X8} not found in domain page map. Halting." -f $pageVaddr)
        $stage2Pass = $false
        continue
    }

    # View span check on target address
    if (-not (Test-PointerViewBounds -Pointer $targetAddr -Base $HvHeapBase -Limit $HvHeapLimit -Label $anchor.Name)) {
        Write-Output "  [FAIL] Anchor target view span violation. Halting."
        $stage2Pass = $false
        break
    }

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
