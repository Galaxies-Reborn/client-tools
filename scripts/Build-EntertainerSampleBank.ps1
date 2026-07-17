[CmdletBinding()]
param(
    [string]$ClientRoot = "E:\SWG\SWGSource\SWGSource Client v3.0",

    [string]$OutputDirectory,

    [switch]$CatalogOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$resolvedClientRoot = (Resolve-Path -LiteralPath $ClientRoot).Path
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repoRoot "generated\entertainer-sample-bank\midi\instruments"
}
$python = (Get-Command python -ErrorAction Stop).Source
& $python -c "import numpy"
if ($LASTEXITCODE -ne 0) {
    throw "Python NumPy is required. Install it with: python -m pip install -r scripts\requirements-entertainer-sample-bank.txt"
}
$arguments = @(
    (Join-Path $PSScriptRoot "entertainer_sample_bank.py"),
    "--client-root", $resolvedClientRoot,
    "--output", $OutputDirectory
)
if ($CatalogOnly) {
    $arguments += "--catalog-only"
}

& $python @arguments
if ($LASTEXITCODE -ne 0) {
    throw "The entertainer sample-bank builder failed with exit code $LASTEXITCODE."
}

if (-not $CatalogOnly) {
    Write-Host "Generated audio remains local and is excluded from Git."
    Write-Host "Bank: $OutputDirectory"
}
