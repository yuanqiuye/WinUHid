param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$Filter = "SteamController.*",
    [switch]$NoElevate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptRoot

if (-not (Test-IsAdministrator)) {
    if ($NoElevate) {
        throw "Steam Controller WinUHid tests require an elevated PowerShell because the installed WinUHid driver ACL rejects non-admin ReadWrite access."
    }

    $args = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", "`"$PSCommandPath`"",
        "-Configuration", $Configuration,
        "-Platform", $Platform,
        "-Filter", $Filter,
        "-NoElevate"
    )

    $process = Start-Process -FilePath powershell.exe -Verb RunAs -WindowStyle Hidden -Wait -PassThru -ArgumentList $args
    exit $process.ExitCode
}

$logDir = Join-Path $scriptRoot "build\test_logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$log = Join-Path $logDir "winuhid_steam_controller_elevated_$timestamp.log"

$dllPaths = @(
    Join-Path $repoRoot "WinUHid\build\$Configuration\$Platform"
    Join-Path $repoRoot "WinUHidDevs\build\$Configuration\$Platform"
    Join-Path $repoRoot "WinUHidUnitTests\build\$Configuration\$Platform"
)

$env:PATH = ($dllPaths -join ";") + ";" + $env:PATH
$testExe = Join-Path $repoRoot "WinUHidUnitTests\build\$Configuration\$Platform\WinUHidUnitTests.exe"

if (-not (Test-Path -LiteralPath $testExe)) {
    throw "WinUHidUnitTests.exe not found: $testExe"
}

& $testExe "--gtest_filter=$Filter" *> $log
$exitCode = $LASTEXITCODE

Write-Host "WinUHid Steam Controller test log: $log"
Write-Host "Exit code: $exitCode"
Get-Content -LiteralPath $log

exit $exitCode
