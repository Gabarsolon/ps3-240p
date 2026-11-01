<#
.SYNOPSIS
    Read-only verification of six LV1 embedded-image anchor words via webMAN MOD HTTP GET.

.DESCRIPTION
    Sends read-only HTTP GET requests to webMAN MOD's /peek.lv1 endpoint on a PlayStation 3
    running CFW (e.g. Evilnat 4.93 + Cobra) to verify three 16-byte embedded ordinary-RAM
    identity anchors (six aligned 64-bit words) from the 4.93 LV1 image.
    
    This script contains NO write/poke/AV-mode commands and does NOT execute any state-modifying
    operations. It reads six 64-bit words, compares them against expected patterns, and checks
    the SHA-256 hash of each 16-byte anchor window.

.PARAMETER ConsoleIP
    IPv4 address of the target PlayStation 3 console (default: 192.168.0.169).

.PARAMETER Port
    HTTP port where webMAN MOD web server listens (default: 80).

.PARAMETER WhatIf
    Dry-run mode: prints the exact URLs and query parameters without sending any network requests.

.EXAMPLE
    .\lv1_anchor_peek.ps1 -WhatIf
    Prints the URLs and query parameters for dry-run verification without contacting the console.

.EXAMPLE
    .\lv1_anchor_peek.ps1 -ConsoleIP "192.168.0.169"
    Executes the six read-only GET requests against the specified console.
#>

[CmdletBinding()]
param(
    [string]$ConsoleIP = "192.168.0.169",
    [int]$Port = 80,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"

# Definition of the three 16-byte embedded-image anchors in 4.93 LV1 ordinary RAM:
# Each anchor spans 16 bytes (two aligned 64-bit words).
$Anchors = @(
    @{
        Name           = "Metadata identity anchor"
        BaseAddress    = 0x46AEA0
        Word0Address   = 0x46AEA0
        Word1Address   = 0x46AEA8
        ExpectedWord0  = "426fc28f00000001" # 59.94 Hz refresh + interlace flag
        ExpectedWord1  = "0000000400000b40" # multiplier 4 + width 2880
        ExpectedSha256 = "d5bfc18bf3510558595e88fe25ededc4306284f90331adaf2fabc68dd753d63f"
    },
    @{
        Name           = "Raster identity anchor"
        BaseAddress    = 0x46A8A0
        Word0Address   = 0x46A8A0
        Word1Address   = 0x46A8A8
        ExpectedWord0  = "00000b3f00000d67" # H active end 2879 + H total 3431
        ExpectedWord1  = "00000b7f00000c77" # H sync edges 2943 / 3191
        ExpectedSha256 = "381678f339e524cd406d4e8545bbdf36c6b8aad426acc497f71f92f7268ec57e"
    },
    @{
        Name           = "Encoder-template identity anchor"
        BaseAddress    = 0x471D40
        Word0Address   = 0x471D40
        Word1Address   = 0x471D48
        ExpectedWord0  = "91003000313f3201" # reg 0x91=0x00, 0x30=0x00, 0x31=0x3f, 0x32=0x01
        ExpectedWord1  = "3302340035003600" # reg 0x33=0x02, 0x34=0x00, 0x35=0x00, 0x36=0x00
        ExpectedSha256 = "8a13e9ec965d86a1bc81cd706dfa2bfd9f0fbcf0234e3b7b25425e2ff7f3c455"
    }
)

Write-Output "================================================================="
Write-Output "LV1 Embedded-Image Anchor Verification (Read-Only)"
Write-Output "Target Console: http://${ConsoleIP}:${Port}"
Write-Output "Mode: $(if ($WhatIf) { 'DRY RUN (-WhatIf)' } else { 'LIVE READ' })"
Write-Output "=================================================================`n"

if ($WhatIf) {
    Write-Output "[DRY RUN] The following 6 GET requests would be issued to read the anchors:"
    Write-Output "          (webMAN MOD /peek.lv1 returns 512-byte hex view aligned to 16 bytes)`n"

    foreach ($anchor in $Anchors) {
        $url0 = "http://${ConsoleIP}:${Port}/peek.lv1?$($anchor.Word0Address.ToString('x'))"
        $url1 = "http://${ConsoleIP}:${Port}/peek.lv1?$($anchor.Word1Address.ToString('x'))"
        Write-Output "Anchor: $($anchor.Name)"
        Write-Output "  Word 0: $url0  [Expected: 0x$($anchor.ExpectedWord0)]"
        Write-Output "  Word 1: $url1  [Expected: 0x$($anchor.ExpectedWord1)]"
        Write-Output "  Expected 16-byte SHA-256: $($anchor.ExpectedSha256)"
        Write-Output ""
    }
    Write-Output "No network packets were transmitted. Dry run complete."
    return
}

# Live execution block (to be run only by console owner)
$sha256 = [System.Security.Cryptography.SHA256]::Create()
$overallPass = $true

function Parse-HexLine([string]$html, [long]$targetAddr) {
    $hexAddrStr = $targetAddr.ToString("X8")
    # Matches lines like: "0046AEA0  42 6F C2 8F 00 00 00 01 00 00 00 04 00 00 0B 40"
    $regex = [regex]"${hexAddrStr}\s+((?:[0-9A-Fa-f]{2}\s+){16})"
    $match = $regex.Match($html)
    if (-not $match.Success) {
        # Fallback: search for 16 hex byte pairs following address
        $regex2 = [regex]"${hexAddrStr}[^0-9A-Fa-f]*((?:[0-9A-Fa-f]{2}[^0-9A-Fa-f]+){16})"
        $match = $regex2.Match($html)
    }
    if ($match.Success) {
        $byteStrings = ($match.Groups[1].Value.Trim() -split '\s+') | Where-Object { $_ -match '^[0-9A-Fa-f]{2}$' }
        if ($byteStrings.Count -ge 16) {
            $bytes = [byte[]]@($byteStrings[0..15] | ForEach-Object { [Convert]::ToByte($_, 16) })
            return $bytes
        }
    }
    return $null
}

foreach ($anchor in $Anchors) {
    Write-Output "-----------------------------------------------------------------"
    Write-Output "Testing Anchor: $($anchor.Name)"
    Write-Output "Base Address: 0x$($anchor.BaseAddress.ToString('X6')) (16 bytes)"

    $url = "http://${ConsoleIP}:${Port}/peek.lv1?$($anchor.BaseAddress.ToString('x'))"
    Write-Output "GET $url"

    try {
        $response = Invoke-WebRequest -Uri $url -UseBasicParsing -TimeoutSec 5
        $html = $response.Content
        $bytes = Parse-HexLine $html $anchor.BaseAddress

        if ($null -eq $bytes) {
            Write-Output "  [FAIL] Unable to parse 16 hex bytes from webMAN MOD response at 0x$($anchor.BaseAddress.ToString('X6'))."
            $overallPass = $false
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

        Write-Output "  Word 0 (0x$($anchor.Word0Address.ToString('X6'))): 0x$w0Hex (Expected: 0x$($anchor.ExpectedWord0))"
        Write-Output "  Word 1 (0x$($anchor.Word1Address.ToString('X6'))): 0x$w1Hex (Expected: 0x$($anchor.ExpectedWord1))"
        Write-Output "  SHA-256: $computedHash"

        $w0Match = ($w0Hex -ieq $anchor.ExpectedWord0)
        $w1Match = ($w1Hex -ieq $anchor.ExpectedWord1)
        $hashMatch = ($computedHash -ieq $anchor.ExpectedSha256)

        if ($w0Match -and $w1Match -and $hashMatch) {
            Write-Output "  Result: PASS"
        } else {
            Write-Output "  Result: FAIL (Mismatch detected)"
            $overallPass = $false
        }
    }
    catch {
        Write-Output "  [ERROR] HTTP request failed: $($_.Exception.Message)"
        $overallPass = $false
    }
}

Write-Output "-----------------------------------------------------------------"
if ($overallPass) {
    Write-Output "OVERALL STATUS: ALL ANCHORS PASSED"
} else {
    Write-Output "OVERALL STATUS: ONE OR MORE ANCHORS FAILED"
}
