param(
    [string]$NetSnmpVersion = "5.10.pre2",
    [string]$ExpectedSha256 = "7a5fc63515f0477d84fb160b6e8a2aee0196117d7e4ffe7d0fba2bdf3a8d72cc",
    [string]$Arch = "x64",
    [string]$InstallPrefix = "C:\ezsnmp-netsnmp-dist"
)

$ErrorActionPreference = "Stop"
$targetDir = Join-Path $InstallPrefix $Arch
$dllPath = Join-Path $targetDir "bin\release\netsnmp.dll"
$libPath = Join-Path $targetDir "lib\release\netsnmp.lib"
$hdrPath = Join-Path $targetDir "include\net-snmp\net-snmp-config.h"

if ((Test-Path $dllPath) -and (Test-Path $libPath) -and (Test-Path $hdrPath)) {
    Write-Host "Net-SNMP $NetSnmpVersion ($Arch) already cached at $targetDir"
    exit 0
}

Write-Host "Preparing standalone Net-SNMP $NetSnmpVersion ($Arch) at $targetDir..."
New-Item -ItemType Directory -Force -Path $targetDir | Out-Null

$workDir = Join-Path ([System.IO.Path]::GetTempPath()) ("ezsnmp-dl-win-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $workDir | Out-Null
$zipPath = Join-Path $workDir "net-snmp-$NetSnmpVersion.zip"
$url = "https://github.com/net-snmp/net-snmp/archive/refs/tags/v$NetSnmpVersion.zip"

Invoke-WebRequest -Uri $url -OutFile $zipPath
$actualHash = (Get-FileHash -Path $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualHash -ne $ExpectedSha256.ToLowerInvariant()) {
    throw "Net-SNMP SHA256 mismatch: expected $ExpectedSha256, got $actualHash"
}

Expand-Archive -Path $zipPath -DestinationPath $workDir -Force
Write-Host "Net-SNMP $NetSnmpVersion archive verified and unpacked in $workDir"
