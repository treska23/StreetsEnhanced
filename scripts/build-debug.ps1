param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$VcpkgRoot = "C:\vcpkg"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$preset = if ($Configuration -eq "Release") { "x64-release" } else { "x64-debug" }

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

# Launch-VsDevShell puede apuntar VCPKG_ROOT al vcpkg integrado de Visual Studio.
# Lo fijamos despues para usar siempre la copia independiente de C:\vcpkg.
$env:VCPKG_ROOT = $VcpkgRoot

$vcpkgExe = Join-Path $env:VCPKG_ROOT "vcpkg.exe"
$vcpkgToolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"

if (-not (Test-Path $vcpkgExe)) {
    throw "No se encontro vcpkg.exe en: $vcpkgExe"
}

if (-not (Test-Path $vcpkgToolchain)) {
    throw "No se encontro el toolchain de vcpkg en: $vcpkgToolchain"
}

Set-Location $repoRoot

$buildDir = Join-Path $repoRoot "out\build\$preset"
Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue

& cmake --preset $preset
if ($LASTEXITCODE -ne 0) {
    throw "La configuracion de CMake ha fallado con codigo $LASTEXITCODE."
}

& cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) {
    throw "La compilacion ha fallado con codigo $LASTEXITCODE."
}

Write-Host ""
Write-Host "Compilacion completada: $Configuration x64" -ForegroundColor Green
