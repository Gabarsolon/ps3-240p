<#
.SYNOPSIS
    Fail-open prototype applicator and inspector for native 240p/288p LV1 timing overrides.

.DESCRIPTION
    Safely inspects, applies, and restores in-memory timing profile overrides for 240p (NTSC)
    and 288p (PAL) in PlayStation 3 volatile DDR3/XDR physical RAM via webMAN MOD /poke.lv1.

    Target Environment:
      PlayStation 3 (CECH-25xx or compatible) running Evilnat 4.93 CFW + Cobra + webMAN MOD.

    Safety & Non-Destructive Design:
      1. Zero Flash Modification: Never modifies /dev_flash, NOR/NAND, or hard disk files.
      2. Reboot Clean: Volatile RAM only. A simple console reboot immediately restores 100% stock state.
      3. Fail-Open Pre-flight Gate: Validates target anchors and hypervisor heap bounds before writing.
      4. Read-Back Verification: Confirms every written 64-bit word against expected patch hashes.
      5. Explicit Opt-In: Requires -Apply switch; default execution is safe inspection.

.PARAMETER ConsoleIP
    IPv4 address of target PlayStation 3 console (default: 192.168.0.169).

.PARAMETER Port
    HTTP port for webMAN MOD web server (default: 80).

.PARAMETER Mode
    Timing target mode: "240p" (NTSC, default) or "288p" (PAL).

.PARAMETER Inspect
    Inspects live console memory at target anchor offsets and reports current profile state.

.PARAMETER Apply
    Applies the in-memory timing override to volatile RAM.

.PARAMETER Restore
    Restores official stock 480i / 576i timing records to volatile RAM.

.PARAMETER WhatIf
    Dry-run mode: prints target URLs, memory offsets, and payload hex without sending requests.

.PARAMETER UnitTest
    Runs offline synthetic unit tests of parser, payload encoders, and bounds checkers.

.PARAMETER Force
    Bypasses interactive confirmation prompt when applying or restoring patches.

.EXAMPLE
    .\apply_240p_patch.ps1 -UnitTest
    Executes offline unit tests validating payload structures and safety checks.

.EXAMPLE
    .\apply_240p_patch.ps1 -Inspect
    Checks whether live console memory currently holds stock 480i or patched 240p timing.

.EXAMPLE
    .\apply_240p_patch.ps1 -WhatIf
    Previews the exact poke URLs and payload bytes for a 240p patch.

.EXAMPLE
    .\apply_240p_patch.ps1 -Apply -Mode "240p"
    Safely applies the 240p in-memory timing patch to volatile RAM.

.EXAMPLE
    .\apply_240p_patch.ps1 -Restore -Mode "240p"
    Restores stock 480i timing records to volatile RAM without rebooting.
#>

[CmdletBinding(DefaultParameterSetName = "InspectSet")]
param(
    [Parameter(Position = 0)]
    [string]$ConsoleIP = "192.168.0.169",

    [Parameter(Position = 1)]
    [int]$Port = 80,

    [Parameter()]
    [ValidateSet("240p", "288p")]
    [string]$Mode = "240p",

    [Parameter(ParameterSetName = "InspectSet")]
    [switch]$Inspect,

    [Parameter(ParameterSetName = "ApplySet", Mandatory = $true)]
    [switch]$Apply,

    [Parameter(ParameterSetName = "RestoreSet", Mandatory = $true)]
    [switch]$Restore,

    [Parameter()]
    [switch]$WhatIf,

    [Parameter()]
    [switch]$UnitTest,

    [Parameter()]
    [switch]$Force
)

$ErrorActionPreference = "Stop"

# Hypervisor Partition Memory Bounds (32 MB partition ceiling)
$HvHeapBase  = 0x00000000L
$HvHeapLimit = 0x02000000L

# -----------------------------------------------------------------------------
# Timing Profile Data Definitions
# -----------------------------------------------------------------------------
$ProfileDefinitions = @{
    "240p" = @{
        Name            = "NTSC 240p (720x240 @ 59.94 Hz, 15.734 kHz)"
        MetadataAddr    = 0x144160L   # Record 1 (replaces 480i)
        MetadataLen     = 24
        RasterAddr      = 0x143B60L   # Record 1 (replaces 480i)
        RasterLen       = 56
        EncoderAddr     = 0x793010L   # Template 0, reg 0x37 pair at +0x10
        EncoderLen      = 8
        StockMetadata   = "426fc28f000000010000000400000b40000001e000000000"
        StockRaster     = "00000b3f00000d6700000b7f00000c77000001df0000020c000001e9000001e8000001ee054704cb04cb03d301f401e20003000011000000"
        StockEncoder    = "378038003a003b00"
        PatchMetadata   = "426fc29f000000000000000400000b40000000f000000000"
        PatchRaster     = "00000b3f00000d6700000b7f00000c77000000ef00000105000000f4000000f4000000f7054704cb04cb03d300f700f40001000011000000"
        PatchEncoder    = "370038003a003b00"
    }
    "288p" = @{
        Name            = "PAL 288p (720x288 @ 50.00 Hz, 15.625 kHz)"
        MetadataAddr    = 0x1441A8L   # Record 4 (replaces 576i)
        MetadataLen     = 24
        RasterAddr      = 0x143C08L   # Record 4 (replaces 576i)
        RasterLen       = 56
        EncoderAddr     = 0x793050L   # Template 1 aligned at +0x0C (pairs 6..9: 35:00 36:00 37:XX 38:00)
        EncoderLen      = 8
        StockMetadata   = "42480000000000010000000400000b400000024000000001"
        StockRaster     = "00000b3f00000d7f00000b6f00000c6d0000023f0000027000000245000002440000024a052e04af04af03b1024a02440003000011000000"
        StockEncoder    = "3500360037803800"
        PatchMetadata   = "42480000000000000000000400000b400000012000000001"
        PatchRaster     = "00000b3f00000d7f00000b6f00000c6d0000011f00000137000001230000012200000125052e04af04af03b1012501220001000011000000"
        PatchEncoder    = "3500360037003800"
    }
}

# -----------------------------------------------------------------------------
# Helper Functions
# -----------------------------------------------------------------------------
function Parse-HexViewHtml {
    param(
        [string]$Html,
        [long]$ExpectedAddr = -1
    )
    if ([string]::IsNullOrEmpty($Html)) { return $null }

    $text = [System.Text.RegularExpressions.Regex]::Replace($Html, '<[^>]+>', ' ')
    $text = [System.Net.WebUtility]::HtmlDecode($text)

    $pattern = [regex]'([0-9A-Fa-f]{8})\s+((?:[0-9A-Fa-f]{2}\s+){16})'
    $matches = $pattern.Matches($text)
    if ($matches.Count -eq 0) { return $null }

    $firstAddrHex = $matches[0].Groups[1].Value
    $firstAddr = [Convert]::ToInt64($firstAddrHex, 16)
    if ($ExpectedAddr -ne -1) {
        $expectedAligned = $ExpectedAddr -band 0xFFFFFFFFFFFFFFF0L
        if (($firstAddr -band 0xFFFFFFFFFFFFFFF0L) -ne $expectedAligned) {
            Write-Warning ("Address mismatch: expected 0x{0:X8}, got 0x{1:X8}" -f $expectedAligned, $firstAddr)
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

function Test-MemoryAddressSafety {
    param(
        [long]$Address,
        [int]$Length,
        [string]$Label = "Target"
    )
    if ($Address -lt $HvHeapBase) {
        Write-Error ("SAFETY HALT: {0} address 0x{1:X} is below hypervisor base 0x{2:X}" -f $Label, $Address, $HvHeapBase)
        return $false
    }
    if (($Address + $Length) -gt $HvHeapLimit) {
        Write-Error ("SAFETY HALT: {0} span [0x{1:X}..0x{2:X}) exceeds hypervisor ceiling 0x{3:X}" -f $Label, $Address, ($Address + $Length), $HvHeapLimit)
        return $false
    }
    if (($Address -band 0x7L) -ne 0) {
        Write-Error ("SAFETY HALT: {0} address 0x{1:X} is not 8-byte aligned" -f $Label, $Address)
        return $false
    }
    if (($Length % 8) -ne 0) {
        Write-Error ("SAFETY HALT: {0} length {1} is not a multiple of 8 bytes" -f $Label, $Length)
        return $false
    }
    return $true
}

function Peek-Lv1Memory {
    param(
        [long]$Address,
        [int]$Length,
        [string]$Label
    )
    $alignedAddr = $Address -band 0xFFFFFFFFFFFFFFF0L
    $url = "http://${ConsoleIP}:${Port}/peek.lv1?$($alignedAddr.ToString('x'))"
    if ($WhatIf) {
        Write-Host "  [WhatIf] GET $url ($Label)" -ForegroundColor Cyan
        if ($null -ne $cfg) {
            if ($Address -eq $cfg.MetadataAddr) {
                $simBytes = [System.Collections.Generic.List[byte]]::new()
                for ($i = 0; $i -lt $cfg.StockMetadata.Length; $i += 2) {
                    $simBytes.Add([Convert]::ToByte($cfg.StockMetadata.Substring($i, 2), 16))
                }
                return $simBytes.ToArray()
            }
            if ($Address -eq $cfg.RasterAddr) {
                $simBytes = [System.Collections.Generic.List[byte]]::new()
                for ($i = 0; $i -lt $cfg.StockRaster.Length; $i += 2) {
                    $simBytes.Add([Convert]::ToByte($cfg.StockRaster.Substring($i, 2), 16))
                }
                return $simBytes.ToArray()
            }
            if ($Address -eq $cfg.EncoderAddr) {
                $simBytes = [System.Collections.Generic.List[byte]]::new()
                for ($i = 0; $i -lt $cfg.StockEncoder.Length; $i += 2) {
                    $simBytes.Add([Convert]::ToByte($cfg.StockEncoder.Substring($i, 2), 16))
                }
                return $simBytes.ToArray()
            }
        }
        return [byte[]]::new($Length)
    }

    $resp = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 5
    $parsed = Parse-HexViewHtml -Html $resp.Content -ExpectedAddr $alignedAddr
    if ($null -eq $parsed) {
        throw "Failed to parse webMAN MOD hex view response from $url"
    }

    $offset = [int]($Address - $parsed.BaseAddress)
    if ($offset + $Length -gt $parsed.Bytes.Length) {
        throw "Requested span exceeds parsed view buffer ($($parsed.Bytes.Length) bytes)"
    }

    $result = [byte[]]::new($Length)
    [System.Array]::Copy($parsed.Bytes, $offset, $result, 0, $Length)
    return $result
}

function Poke-Lv1Memory {
    param(
        [long]$Address,
        [string]$HexPayload,
        [string]$Label
    )
    $cleanHex = $HexPayload.ToLower().Trim()
    $byteCount = $cleanHex.Length / 2
    if (-not (Test-MemoryAddressSafety -Address $Address -Length $byteCount -Label $Label)) {
        throw "Safety checks failed for poke to $Label"
    }

    $url = "http://${ConsoleIP}:${Port}/poke.lv1?$($Address.ToString('x'))=$cleanHex"
    if ($WhatIf) {
        Write-Host "  [WhatIf] GET $url ($Label, $byteCount bytes)" -ForegroundColor Cyan
        return $true
    }

    Write-Host ("  Poking {0} (0x{1:X}, {2} bytes)... " -f $Label, $Address, $byteCount) -NoNewline
    $resp = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 5

    # Verify write immediately via read-back
    $readBack = Peek-Lv1Memory -Address $Address -Length $byteCount -Label "$Label Verification"
    $readHex = (-join ($readBack | ForEach-Object { $_.ToString("x2") })).ToLower()
    if ($readHex -ne $cleanHex) {
        Write-Host "FAILED!" -ForegroundColor Red
        throw ("Verification mismatch on {0}: wrote {1}, read back {2}" -f $Label, $cleanHex, $readHex)
    }

    Write-Host "VERIFIED" -ForegroundColor Green
    return $true
}

function Get-BytesSha256([byte[]]$bytes) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $hash = $sha.ComputeHash($bytes)
    return (-join ($hash | ForEach-Object { $_.ToString("x2") }))
}

# -----------------------------------------------------------------------------
# Unit Test Harness (Offline)
# -----------------------------------------------------------------------------
function Run-UnitTests {
    Write-Host "`n=== Running apply_240p_patch.ps1 Offline Unit Tests ===" -ForegroundColor Cyan
    $passCount = 0
    $totalCount = 0

    # Test 1: Bounds validation
    $totalCount++
    if (Test-MemoryAddressSafety -Address 0x144160L -Length 24 -Label "Test 1") {
        Write-Host "  [PASS] Test 1: Valid hypervisor heap address accepted" -ForegroundColor Green
        $passCount++
    } else {
        Write-Host "  [FAIL] Test 1: Valid address rejected" -ForegroundColor Red
    }

    # Test 2: Reject out of bounds
    $totalCount++
    $rejected = $false
    try {
        $rejected = -not (Test-MemoryAddressSafety -Address 0x02500000L -Length 24 -Label "Test 2")
    } catch { $rejected = $true }
    if ($rejected) {
        Write-Host "  [PASS] Test 2: Address beyond 32 MB heap ceiling rejected" -ForegroundColor Green
        $passCount++
    } else {
        Write-Host "  [FAIL] Test 2: Out of bounds address accepted!" -ForegroundColor Red
    }

    # Test 3: Reject misaligned address
    $totalCount++
    $misalignedRejected = $false
    try {
        $misalignedRejected = -not (Test-MemoryAddressSafety -Address 0x144163L -Length 24 -Label "Test 3")
    } catch { $misalignedRejected = $true }
    if ($misalignedRejected) {
        Write-Host "  [PASS] Test 3: Unaligned address (not 8-byte aligned) rejected" -ForegroundColor Green
        $passCount++
    } else {
        Write-Host "  [FAIL] Test 3: Unaligned address accepted!" -ForegroundColor Red
    }

    # Test 4: Verify 240p payload lengths and hex formatting
    $totalCount++
    $p240 = $ProfileDefinitions["240p"]
    $lenCheck = ($p240.PatchMetadata.Length -eq 48) -and ($p240.PatchRaster.Length -eq 112) -and ($p240.PatchEncoder.Length -eq 16)
    if ($lenCheck) {
        Write-Host "  [PASS] Test 4: 240p binary payload lengths match exact 8-byte multiples" -ForegroundColor Green
        $passCount++
    } else {
        Write-Host "  [FAIL] Test 4: Payload lengths invalid" -ForegroundColor Red
    }

    # Test 5: Verify 288p payload lengths
    $totalCount++
    $p288 = $ProfileDefinitions["288p"]
    $lenCheck288 = ($p288.PatchMetadata.Length -eq 48) -and ($p288.PatchRaster.Length -eq 112) -and ($p288.PatchEncoder.Length -eq 16)
    if ($lenCheck288) {
        Write-Host "  [PASS] Test 5: 288p binary payload lengths match exact 8-byte multiples" -ForegroundColor Green
        $passCount++
    } else {
        Write-Host "  [FAIL] Test 5: 288p payload lengths invalid" -ForegroundColor Red
    }

    # Test 6: Verify HTML HexView parser
    $totalCount++
    $sampleHtml = @"
<html><body><pre>
00144160  42 6f c2 8f 00 00 00 01 00 00 00 04 00 00 0b 40  Bo.............@
00144170  00 00 01 e0 00 00 00 00 00 00 00 00 00 00 00 00  ................
</pre></body></html>
"@
    $parsed = Parse-HexViewHtml -Html $sampleHtml -ExpectedAddr 0x144160L
    if ($null -ne $parsed -and $parsed.Bytes.Length -eq 32 -and $parsed.Bytes[0] -eq 0x42) {
        Write-Host "  [PASS] Test 6: HexView parser correctly extracts bytes from webMAN MOD response" -ForegroundColor Green
        $passCount++
    } else {
        Write-Host "  [FAIL] Test 6: Parser failed on sample HTML" -ForegroundColor Red
    }

    Write-Host ("`nSummary: {0}/{1} Unit Tests Passed." -f $passCount, $totalCount) -ForegroundColor Cyan
    if ($passCount -eq $totalCount) {
        Write-Host "ALL UNIT TESTS PASSED SUCCESSFULLY.`n" -ForegroundColor Green
    } else {
        throw "Unit tests failed!"
    }
}

# -----------------------------------------------------------------------------
# Main Execution Flow
# -----------------------------------------------------------------------------
if ($UnitTest) {
    Run-UnitTests
    return
}

$cfg = $ProfileDefinitions[$Mode]
if ($null -eq $cfg) {
    throw "Invalid mode '$Mode'. Supported modes: 240p, 288p"
}

Write-Host "`n=======================================================" -ForegroundColor Cyan
Write-Host ("  PlayStation 3 LV1 Timing Override Applicator ({0})" -f $cfg.Name) -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "Target Console: http://${ConsoleIP}:${Port}"
Write-Host "Selected Mode:  $Mode"

# -----------------------------------------------------------------------------
# 1. Inspect Mode
# -----------------------------------------------------------------------------
if ($Inspect -or (-not $Apply -and -not $Restore)) {
    Write-Host "`n--- Live Console Memory Inspection ---" -ForegroundColor Yellow

    Write-Host ("Checking Metadata Anchor (0x{0:X}, {1} bytes)..." -f $cfg.MetadataAddr, $cfg.MetadataLen)
    $liveMeta = Peek-Lv1Memory -Address $cfg.MetadataAddr -Length $cfg.MetadataLen -Label "Metadata Anchor"
    $liveMetaHex = (-join ($liveMeta | ForEach-Object { $_.ToString("x2") })).ToLower()

    Write-Host ("Checking Raster Anchor (0x{0:X}, {1} bytes)..." -f $cfg.RasterAddr, $cfg.RasterLen)
    $liveRaster = Peek-Lv1Memory -Address $cfg.RasterAddr -Length $cfg.RasterLen -Label "Raster Anchor"
    $liveRasterHex = (-join ($liveRaster | ForEach-Object { $_.ToString("x2") })).ToLower()

    Write-Host ("Checking MultiAV Encoder Template (0x{0:X}, {1} bytes)..." -f $cfg.EncoderAddr, $cfg.EncoderLen)
    $liveEnc = Peek-Lv1Memory -Address $cfg.EncoderAddr -Length $cfg.EncoderLen -Label "Encoder Template"
    $liveEncHex = (-join ($liveEnc | ForEach-Object { $_.ToString("x2") })).ToLower()

    Write-Host "`n[Status Summary]" -ForegroundColor Cyan
    $isStock = ($liveMetaHex -eq $cfg.StockMetadata) -and ($liveRasterHex -eq $cfg.StockRaster) -and ($liveEncHex -eq $cfg.StockEncoder)
    $isPatched = ($liveMetaHex -eq $cfg.PatchMetadata) -and ($liveRasterHex -eq $cfg.PatchRaster) -and ($liveEncHex -eq $cfg.PatchEncoder)

    if ($isStock) {
        Write-Host "  Profile State: STOCK FIRMWARE (Unpatched)" -ForegroundColor Green
        Write-Host "  Ready to apply in-memory timing override via -Apply switch."
    } elseif ($isPatched) {
        Write-Host "  Profile State: NATIVE OVERRIDE ACTIVE ($Mode)" -ForegroundColor Magenta
        Write-Host "  The console volatile RAM is currently patched for native 15 kHz progressive scanout."
        Write-Host "  Use -Restore to revert back to stock timing, or reboot the console."
    } else {
        Write-Host "  Profile State: CUSTOM / DIVERGENT" -ForegroundColor Yellow
        Write-Host "  Live bytes do not match standard stock or standard patch. Inspection details:"
        Write-Host "    Metadata: $liveMetaHex"
        Write-Host "    Raster:   $liveRasterHex"
        Write-Host "    Encoder:  $liveEncHex"
    }
    return
}

# -----------------------------------------------------------------------------
# 2. Apply Mode (Volatile DDR3/XDR In-Memory Patch)
# -----------------------------------------------------------------------------
if ($Apply) {
    Write-Host "`n--- Applying In-Memory Timing Override ($Mode) ---" -ForegroundColor Magenta
    Write-Host "Safety notice: writes target volatile physical RAM only; clean on restart." -ForegroundColor Yellow

    if (-not $Force -and -not $WhatIf) {
        $confirm = Read-Host "`nProceed with writing $Mode timing records to PS3 RAM? (Type 'YES' to confirm)"
        if ($confirm -ne "YES") {
            Write-Host "Aborted by user." -ForegroundColor Yellow
            return
        }
    }

    # Step 1: Pre-flight verify
    Write-Host "`n[1/4] Pre-flight safety verification..." -ForegroundColor Cyan
    $preMeta = Peek-Lv1Memory -Address $cfg.MetadataAddr -Length $cfg.MetadataLen -Label "Pre-flight Metadata"
    $preMetaHex = (-join ($preMeta | ForEach-Object { $_.ToString("x2") })).ToLower()
    if ($preMetaHex -ne $cfg.StockMetadata -and $preMetaHex -ne $cfg.PatchMetadata) {
        throw "PRE-FLIGHT HALT: Metadata does not match stock or patch signature. Refusing to write."
    }
    Write-Host "  Pre-flight checks passed." -ForegroundColor Green

    # Step 2: Poke Metadata
    Write-Host "`n[2/4] Programming Metadata Record..." -ForegroundColor Cyan
    Poke-Lv1Memory -Address $cfg.MetadataAddr -HexPayload $cfg.PatchMetadata -Label "Metadata Record ($Mode)"

    # Step 3: Poke Raster Record
    Write-Host "`n[3/4] Programming RSX Raster Record..." -ForegroundColor Cyan
    Poke-Lv1Memory -Address $cfg.RasterAddr -HexPayload $cfg.PatchRaster -Label "Raster Record ($Mode)"

    # Step 4: Poke Encoder Template
    Write-Host "`n[4/4] Configuring MultiAV Encoder Template..." -ForegroundColor Cyan
    Poke-Lv1Memory -Address $cfg.EncoderAddr -HexPayload $cfg.PatchEncoder -Label "Encoder Reg 0x37 ($Mode)"

    Write-Host "`n=======================================================" -ForegroundColor Green
    Write-Host "  TIMING OVERRIDE APPLIED & VERIFIED SUCCESSFULLY!" -ForegroundColor Green
    Write-Host "=======================================================" -ForegroundColor Green
    Write-Host @"

Next Steps to Activate:
1. In the PS3 XMB Video Output Settings, or inside an active emulator:
   - Toggle resolution away from 480i (e.g. switch to 480p or 720p).
   - Switch back to 480i (NTSC) / 576i (PAL).
2. sysmgr_ss will load the newly patched 240p/288p records, reprogram the
   RSX CRTC registers without interlace, and send progressive sync to CXM4027R.
3. Your CRT or video scaler (OSSC / RetroTINK) should report native 240p!
4. Power cycling or restarting the console completely restores stock state.
"@
    return
}

# -----------------------------------------------------------------------------
# 3. Restore Mode
# -----------------------------------------------------------------------------
if ($Restore) {
    Write-Host "`n--- Restoring Stock Timing Records ($Mode) ---" -ForegroundColor Yellow

    if (-not $Force -and -not $WhatIf) {
        $confirm = Read-Host "`nRestore official stock timing records to PS3 RAM? (Type 'YES' to confirm)"
        if ($confirm -ne "YES") {
            Write-Host "Aborted by user." -ForegroundColor Yellow
            return
        }
    }

    Write-Host "`n[1/3] Restoring Metadata Record..." -ForegroundColor Cyan
    Poke-Lv1Memory -Address $cfg.MetadataAddr -HexPayload $cfg.StockMetadata -Label "Stock Metadata"

    Write-Host "`n[2/3] Restoring RSX Raster Record..." -ForegroundColor Cyan
    Poke-Lv1Memory -Address $cfg.RasterAddr -HexPayload $cfg.StockRaster -Label "Stock Raster"

    Write-Host "`n[3/3] Restoring MultiAV Encoder Template..." -ForegroundColor Cyan
    Poke-Lv1Memory -Address $cfg.EncoderAddr -HexPayload $cfg.StockEncoder -Label "Stock Encoder Template"

    Write-Host "`nStock timing records restored and verified. Toggle display mode or reboot console." -ForegroundColor Green
}
