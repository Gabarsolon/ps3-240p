# Upload PS3-240P.pkg to PS3 HDD via FTP and verify read-back hash
param(
    [string]$ConsoleIP = '192.168.0.169'
)

$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$pkg = Join-Path $root 'deliverables\PS3-240P.pkg'

function Get-Sha256([string]$path) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.IO.File]::ReadAllBytes($path)
        ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $pkg)) {
    throw "Package deliverable not found: $pkg. Run python tools/build_ps3_app.py first."
}

$expectedHash = Get-Sha256 $pkg
Write-Host "Target PS3: ftp://${ConsoleIP}" -ForegroundColor Cyan
Write-Host "Local PKG : $pkg"
Write-Host "SHA-256   : $expectedHash"

# Test FTP connection to console
Write-Host "Testing FTP connectivity..." -NoNewline
$null = curl.exe --fail --silent --show-error --disable-epsv --ftp-skip-pasv-ip --max-time 10 "ftp://${ConsoleIP}/dev_hdd0/"
if ($LASTEXITCODE -ne 0) {
    throw "Cannot connect to ftp://${ConsoleIP}/. Ensure PS3 is powered on with webMAN MOD FTP enabled."
}
Write-Host " Connected!" -ForegroundColor Green

# Upload to /dev_hdd0/packages/
$remotePath = '/dev_hdd0/packages/PS3-240P.pkg'
Write-Host "Uploading PS3-240P.pkg -> ${remotePath}..." -ForegroundColor Cyan
curl.exe --fail --silent --show-error --disable-epsv --ftp-skip-pasv-ip --ftp-create-dirs --max-time 300 -T $pkg "ftp://${ConsoleIP}${remotePath}"
if ($LASTEXITCODE -ne 0) {
    throw "Upload failed to ftp://${ConsoleIP}${remotePath}"
}
Write-Host "Upload complete. Verifying integrity via read-back..." -ForegroundColor Cyan

# Download to temp and verify byte-for-byte hash
$tempFile = Join-Path $env:TEMP ('ps3_verify_' + [System.Guid]::NewGuid().ToString('N') + '.pkg')
try {
    curl.exe --fail --silent --show-error --disable-epsv --ftp-skip-pasv-ip --max-time 300 -o $tempFile "ftp://${ConsoleIP}${remotePath}"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to download uploaded package for verification."
    }
    $downloadedHash = Get-Sha256 $tempFile
    if ($downloadedHash -ne $expectedHash) {
        throw "Verification failed! Uploaded hash ($downloadedHash) did not match local hash ($expectedHash)."
    }
    Write-Host "Verification SUCCESS! Package integrity verified on console HDD." -ForegroundColor Green
}
finally {
    Remove-Item -LiteralPath $tempFile -Force -ErrorAction SilentlyContinue
}

# Optional: send webMAN notification popup on screen
try {
    $popupMsg = [System.Uri]::EscapeDataString("PS3-240P.pkg uploaded to /dev_hdd0/packages!")
    Invoke-WebRequest -Uri "http://${ConsoleIP}/popup.ps3?${popupMsg}" -UseBasicParsing -TimeoutSec 3 | Out-Null
} catch {}

Write-Host ""
Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  INSTALLATION READY ON CONSOLE" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
Write-Host "1. On your PS3 XMB, go to: Game > Package Manager > Install Package Files > Standard"
Write-Host "2. Select 'PS3 240p / 288p Switcher' (PS3-240P.pkg)"
Write-Host "3. Once installed, launch the app from the XMB Game column!"
Write-Host "=========================================================="
