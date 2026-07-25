param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$preset = if ($Configuration -eq "Release") { "x64-release" } else { "x64-debug" }

if (-not $env:VCPKG_ROOT) {
    $env:VCPKG_ROOT = "C:\vcpkg"
}

$vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    throw "No se encontro vswhere.exe. Repara o instala Visual Studio Installer."
}

$vsInstall = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) {
    throw "No se encontro una instalacion de Visual Studio con las herramientas MSVC x64/x86."
}

$devShell = Join-Path $vsInstall "Common7\Tools\Launch-VsDevShell.ps1"
if (-not (Test-Path $devShell)) {
    throw "No se encontro Launch-VsDevShell.ps1 en: $devShell"
}

& $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation

Set-Location $repoRoot

$buildDir = Join-Path $repoRoot "out\build\$preset"
Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue

cmake --preset $preset
cmake --build --preset $preset

Write-Host ""
Write-Host "Compilacion completada: $Configuration x64" -ForegroundColor Green
