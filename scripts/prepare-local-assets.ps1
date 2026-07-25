$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$directories = @(
    "LocalAssets\OriginalRom",
    "LocalAssets\ReferenceSheets\Characters",
    "LocalAssets\ReferenceSheets\Enemies",
    "LocalAssets\ReferenceSheets\Stages",
    "LocalAssets\ReferenceSheets\UI",
    "LocalAssets\Extracted\Characters",
    "LocalAssets\Extracted\Enemies",
    "LocalAssets\Extracted\Stages",
    "LocalAssets\Extracted\UI",
    "LocalAssets\Extracted\Audio",
    "LocalAssets\Checksums"
)

foreach ($relativeDirectory in $directories) {
    $fullPath = Join-Path $repoRoot $relativeDirectory
    New-Item -ItemType Directory -Path $fullPath -Force | Out-Null
}

$round1Path = Join-Path $repoRoot "LocalAssets\ReferenceSheets\Stages\Round1.png"

Write-Host "Carpetas locales preparadas." -ForegroundColor Green
Write-Host "Coloca el mapa PNG del Round 1 en:" -ForegroundColor Yellow
Write-Host $round1Path
Write-Host "LocalAssets esta ignorada por Git y no se subira al repositorio."
