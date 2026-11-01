<#
.SYNOPSIS
    Two-stage read-only verification of LV1 process structures and AV timing anchors via webMAN MOD HTTP GET.

.DESCRIPTION
    Sends read-only HTTP GET requests to webMAN MOD's /peek.lv1 endpoint on a PlayStation 3
    running CFW (e.g. Evilnat 4.93 + Cobra) using a safe, staged approach:
    
    Stage 1 (Discovery):
      Peeks the global LV1 Process Table at 0x36cdb0 (within the proven-mapped LV1 .data/.bss
      segment 0x200000..0x3c2c00). webMAN MOD returns an aligned 512-byte hex view containing
      all 32 process slots (PIDs 0..31, 16 bytes each), identifying active UX user-mode processes
      including sysmgr_ss.fself.
      
    Stage 2 (Anchor Verification):
      When the physical base addresses for the sysmgr_ss text and data segments (PhysBase_Text
      and PhysBase_Data) are provided or resolved, reads the three 16-byte timing and encoder
      anchors:
        - Metadata anchor at PhysBase_Text + 0x39160
        - Raster anchor at PhysBase_Text + 0x38b60
        - Encoder-template anchor at PhysBase_Data + 0x00000
      Compares the returned words against expected patterns and verifies 16-byte SHA-256 hashes.

    This script contains NO write/poke/AV-mode commands and does NOT execute any state-modifying
    operations.

.PARAMETER ConsoleIP
    IPv4 address of the target PlayStation 3 console (default: 192.168.0.169).

.PARAMETER Port
    HTTP port where webMAN MOD web server listens (default: 80).

.PARAMETER WhatIf
    Dry-run mode: prints the exact URLs and query parameters without sending any network requests.

.PARAMETER Stage1Only
    Executes only Stage 1 (Process Table discovery) and halts before Stage 2.

.PARAMETER TextBase
    Discovered physical/real base address for sysmgr_ss text segment (virtual 0x80000000).

.PARAMETER DataBase
    Discovered physical/real base address for sysmgr_ss data segment (virtual 0xc0000000).

.EXAMPLE
    .\lv1_anchor_peek.ps1 -WhatIf
    Prints the URLs and query parameters for dry-run verification without contacting the console.

.EXAMPLE
    .\lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169" -Stage1Only
    Executes Stage 1 discovery peeks against the specified console to inspect the Process Table.

.EXAMPLE
    .\lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169" -TextBase 0x1A00000 -DataBase 0x1B00000
    Executes both Stage 1 discovery and Stage 2 anchor verification using specified base addresses.
#>

[CmdletBinding()]
param(
    [string]$ConsoleIP = "192.168.0.169",
    [int]$Port = 80,
    [switch]$WhatIf,
    [switch]$Stage1Only,
    [long]$TextBase = 0,
    [long]$DataBase = 0
)

$ErrorActionPreference = "Stop"

# Constants
$ProcessTableAddress = 0x36CDB0 # 512 bytes, 32 entries (PIDs 0..31), inside LV1 .data/.bss
$HexViewSize = 0x200            # 512 bytes returned by webMAN MOD /peek.lv1

# Relative offsets of the three anchors within sysmgr_ss segments:
# Text segment (virtual 0x80000000):
$MetadataOffset = 0x39160
$RasterOffset   = 0x38B60
# Data segment (virtual 0xC0000000):
$EncoderOffset  = 0x00000

# Expected anchor definitions
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
Write-Output "LV1 Staged Process Discovery & Anchor Verification (Read-Only)"
Write-Output "Target Console: http://${ConsoleIP}:${Port}"
Write-Output "Mode: $(if ($WhatIf) { 'DRY RUN (-WhatIf)' } else { 'LIVE READ' })"
Write-Output "=================================================================`n"

if ($WhatIf) {
    Write-Output "[DRY RUN] Stage 1: Process Table Discovery"
    Write-Output "  Endpoint: http://${ConsoleIP}:${Port}/peek.lv1?$($ProcessTableAddress.ToString('x'))"
    Write-Output "  Description: Reads 512-byte view of 32 process slots at 0x$($ProcessTableAddress.ToString('X6'))"
    Write-Output "  Safety: Guaranteed safe inside mapped LV1 .data/.bss segment (0x200000..0x3c2c00)`n"

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
        Write-Output "  TextBase/DataBase not supplied on command line."
        Write-Output "  In live execution, Stage 2 is executed after discovering the physical segment"
        Write-Output "  bases from Stage 1 Process Table results, or by providing -TextBase and -DataBase."
    }
    Write-Output "`nNo network packets were transmitted. Dry run complete."
    return
}

# Live execution helper functions
function Parse-HexLines([string]$html) {
    # Parses 512-byte hex view into a byte array
    $bytes = [System.Collections.Generic.List[byte]]::new()
    $lines = $html -split "`r?`n"
    foreach ($line in $lines) {
        # Match lines starting with 8 hex chars followed by 16 hex bytes:
        # e.g.: "0036CDB0  00 00 00 00 00 00 00 01  00 00 00 00 01 23 45 67  ........"
        if ($line -match '^[0-9A-Fa-f]{8}\s+((?:[0-9A-Fa-f]{2}\s+){8,16})') {
            $hexParts = ($matches[1].Trim() -split '\s+') | Where-Object { $_ -match '^[0-9A-Fa-f]{2}$' }
            foreach ($h in $hexParts) {
                $bytes.Add([Convert]::ToByte($h, 16))
            }
        }
    }
    return $bytes.ToArray()
}

function Parse-HexLine([string]$html, [long]$targetAddr) {
    $hexAddrStr = $targetAddr.ToString("X8")
    $regex = [regex]"${hexAddrStr}\s+((?:[0-9A-Fa-f]{2}\s+){16})"
    $match = $regex.Match($html)
    if (-not $match.Success) {
        $regex2 = [regex]"${hexAddrStr}[^0-9A-Fa-f]*((?:[0-9A-Fa-f]{2}[^0-9A-Fa-f]+){16})"
        $match = $regex2.Match($html)
    }
    if ($match.Success) {
        $byteStrings = ($match.Groups[1].Value.Trim() -split '\s+') | Where-Object { $_ -match '^[0-9A-Fa-f]{2}$' }
        if ($byteStrings.Count -ge 16) {
            return [byte[]]@($byteStrings[0..15] | ForEach-Object { [Convert]::ToByte($_, 16) })
        }
    }
    return $null
}

# -----------------------------------------------------------------------------
# Stage 1: Process Table Discovery
# -----------------------------------------------------------------------------
Write-Output "-----------------------------------------------------------------"
Write-Output "STAGE 1: Peeking Global Process Table at 0x$($ProcessTableAddress.ToString('X6'))"
$stage1Url = "http://${ConsoleIP}:${Port}/peek.lv1?$($ProcessTableAddress.ToString('x'))"
Write-Output "GET $stage1Url"

$stage1Bytes = $null
try {
    $response = Invoke-WebRequest -Uri $stage1Url -UseBasicParsing -TimeoutSec 5
    $stage1Bytes = Parse-HexLines $response.Content
} catch {
    Write-Output "  [ERROR] HTTP request failed: $($_.Exception.Message)"
    return
}

if ($null -eq $stage1Bytes -or $stage1Bytes.Length -lt 512) {
    Write-Output "  [FAIL] Failed to parse 512 bytes from Process Table view."
    return
}

Write-Output "  Successfully parsed 512 bytes (32 process slots):"
$activeSlots = @()
for ($pid = 0; $pid -lt 32; $pid++) {
    $offset = $pid * 16
    $flags = [System.BitConverter]::ToUInt64($stage1Bytes, $offset)
    $procPtr = [System.BitConverter]::ToUInt64($stage1Bytes, $offset + 8)

    # Invert endianness from big-endian network byte order
    if ([System.BitConverter]::IsLittleEndian) {
        $flags = [System.Net.IPAddress]::NetworkToHostOrder([long]$flags)
        $procPtr = [System.Net.IPAddress]::NetworkToHostOrder([long]$procPtr)
    }

    if ($flags -ne 0 -or $procPtr -ne 0) {
        $slotInfo = [PSCustomObject]@{
            PID     = $pid
            Flags   = "0x$($flags.ToString('X16'))"
            ProcPtr = "0x$($procPtr.ToString('X16'))"
        }
        $activeSlots += $slotInfo
        Write-Output "    PID $pid : Flags=$($slotInfo.Flags)  ProcPtr=$($slotInfo.ProcPtr)"
    }
}

if ($activeSlots.Count -eq 0) {
    Write-Output "  [NOTICE] No active process slots found (all slots zeroed)."
} else {
    Write-Output "  Found $($activeSlots.Count) active process slots."
}

if ($Stage1Only) {
    Write-Output "`nStage 1 complete (-Stage1Only specified). Exiting."
    return
}

# -----------------------------------------------------------------------------
# Stage 2: Target Anchor Verification
# -----------------------------------------------------------------------------
Write-Output "`n-----------------------------------------------------------------"
Write-Output "STAGE 2: Target Anchor Verification"

if ($TextBase -eq 0 -or $DataBase -eq 0) {
    Write-Output "  [PAUSE] Stage 2 requires TextBase and DataBase parameters."
    Write-Output "  The physical load addresses are dynamic and must be computed from Stage 1"
    Write-Output "  results before peeking. To execute Stage 2, pass -TextBase and -DataBase."
    Write-Output "  Example:"
    Write-Output "    .\lv1_anchor_peek.ps1 -ConsoleIP `"$ConsoleIP`" -TextBase <0xAddress> -DataBase <0xAddress>"
    return
}

$sha256 = [System.Security.Cryptography.SHA256]::Create()
$stage2Pass = $true

foreach ($anchor in $ExpectedAnchors) {
    $base = if ($anchor.Segment -eq "Text") { $TextBase } else { $DataBase }
    $targetAddr = $base + $anchor.RelativeOffset

    Write-Output "-----------------------------------------------------------------"
    Write-Output "Testing Anchor: $($anchor.Name) ($($anchor.Segment) + 0x$($anchor.RelativeOffset.ToString('X')))"
    Write-Output "Physical Address: 0x$($targetAddr.ToString('X')) (16 bytes)"

    $url = "http://${ConsoleIP}:${Port}/peek.lv1?$($targetAddr.ToString('x'))"
    Write-Output "GET $url"

    try {
        $response = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 5
        $bytes = Parse-HexLine $response.Content $targetAddr

        if ($null -eq $bytes) {
            Write-Output "  [FAIL] Unable to parse 16 hex bytes from webMAN MOD response at 0x$($targetAddr.ToString('X'))."
            $stage2Pass = $false
            continue
        }

        # Extract words (Big-Endian)
        $w0Bytes = $bytes[0..7]
        $w1Bytes = $bytes[8..15]
        $w0Hex = ($w0Bytes | ForEach-Object { $_.ToString("x2") }) -join ""
        $w1Hex = ($w1Bytes | ForEach-Object { $_.ToString("x2") }) -join ""

        # Compute SHA-256 of 16-byte window
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
    } catch {
        Write-Output "  [ERROR] HTTP request failed: $($_.Exception.Message)"
        $stage2Pass = $false
    }
}

Write-Output "-----------------------------------------------------------------"
if ($stage2Pass) {
    Write-Output "OVERALL STATUS: ALL ANCHORS PASSED"
} else {
    Write-Output "OVERALL STATUS: ONE OR MORE ANCHORS FAILED"
}
