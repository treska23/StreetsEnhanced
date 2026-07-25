param(
    [string]$AssetsRoot = "LocalAssets",
    [string]$OutputPath = "docs/LOCAL_ASSET_INVENTORY.json"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$assetsPath = Join-Path $repoRoot $AssetsRoot
$outputFile = Join-Path $repoRoot $OutputPath

if (-not (Test-Path $assetsPath)) {
    throw "No existe la carpeta de recursos: $assetsPath"
}

Add-Type -AssemblyName System.Drawing

$files = Get-ChildItem -Path $assetsPath -Recurse -File |
    Where-Object { $_.Extension -match '^\.(png|bmp|gif|jpg|jpeg)$' } |
    Sort-Object FullName

if ($files.Count -eq 0) {
    throw "No se encontraron imágenes dentro de: $assetsPath"
}

$inventory = foreach ($file in $files) {
    $relativePath = [System.IO.Path]::GetRelativePath($repoRoot, $file.FullName).Replace('\\', '/')
    $image = $null

    try {
        $image = [System.Drawing.Image]::FromFile($file.FullName)
        [PSCustomObject]@{
            fileName      = $file.Name
            relativePath  = $relativePath
            extension     = $file.Extension.ToLowerInvariant()
            width         = $image.Width
            height        = $image.Height
            sizeBytes     = $file.Length
            sha256        = (Get-FileHash -Algorithm SHA256 -Path $file.FullName).Hash.ToLowerInvariant()
        }
    }
    finally {
        if ($null -ne $image) {
            $image.Dispose()
        }
    }
}

$outputDirectory = Split-Path -Parent $outputFile
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$document = [PSCustomObject]@{
    generatedAtUtc = [DateTime]::UtcNow.ToString('o')
    assetRoot      = $AssetsRoot.Replace('\\', '/')
    imageCount     = $inventory.Count
    images         = $inventory
}

$document | ConvertTo-Json -Depth 5 | Set-Content -Path $outputFile -Encoding UTF8

Write-Host ""
Write-Host "Inventario generado correctamente." -ForegroundColor Green
Write-Host "Imágenes encontradas: $($inventory.Count)"
Write-Host "Archivo: $outputFile"
Write-Host ""

$inventory |
    Select-Object fileName, width, height, relativePath |
    Format-Table -AutoSize
