<#
.SYNOPSIS
Builds and stages a clean Release x64/DX11 Steam Deck client package.

.EXAMPLE
.\scripts\Build-SteamDeckClient.ps1 -ClientAssetRoot "E:\SWG\SWGSource\SWGSource Client v3.0" -OutputRoot "E:\SWG\SWGSource\SteamDeck"
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$ClientAssetRoot,

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$OutputRoot,

    [ValidateSet("v143", "v145")]
    [string]$PlatformToolset = "v145",

    [string]$VisualStudioRoot,

    [ValidateRange(0, 128)]
    [int]$MaxCpuCount = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-NormalizedPath {
    param([Parameter(Mandatory)][string]$Path)

    return [IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Test-PathsOverlap {
    param(
        [Parameter(Mandatory)][string]$Left,
        [Parameter(Mandatory)][string]$Right
    )

    $leftPath = Get-NormalizedPath -Path $Left
    $rightPath = Get-NormalizedPath -Path $Right
    return (
        [string]::Equals($leftPath, $rightPath, [StringComparison]::OrdinalIgnoreCase) -or
        $leftPath.StartsWith($rightPath + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $rightPath.StartsWith($leftPath + '\', [StringComparison]::OrdinalIgnoreCase)
    )
}

$repoRoot = Get-NormalizedPath -Path (Join-Path $PSScriptRoot "..")
if (-not (Test-Path -LiteralPath $ClientAssetRoot -PathType Container)) {
    throw "ClientAssetRoot does not exist or is not a directory: $ClientAssetRoot"
}
$fullAssetPath = (Resolve-Path -LiteralPath $ClientAssetRoot).ProviderPath
$assetFilesystemRoot = [IO.Path]::GetPathRoot($fullAssetPath)
if ([string]::Equals(
    $fullAssetPath.TrimEnd('\', '/'),
    $assetFilesystemRoot.TrimEnd('\', '/'),
    [StringComparison]::OrdinalIgnoreCase)) {
    throw "ClientAssetRoot must not be a filesystem root: $fullAssetPath"
}
$assetRoot = Get-NormalizedPath -Path $fullAssetPath
$fullOutputPath = [IO.Path]::GetFullPath($OutputRoot)
$filesystemRoot = [IO.Path]::GetPathRoot($fullOutputPath)
if ([string]::Equals(
    $fullOutputPath.TrimEnd('\', '/'),
    $filesystemRoot.TrimEnd('\', '/'),
    [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must not be a filesystem root: $fullOutputPath"
}
$outputPath = Get-NormalizedPath -Path $fullOutputPath

if (Test-PathsOverlap -Left $outputPath -Right $repoRoot) {
    throw "OutputRoot must be outside the source repository: $outputPath"
}
if (Test-PathsOverlap -Left $outputPath -Right $assetRoot) {
    throw "OutputRoot must not overlap the client asset source: $outputPath"
}

[void][IO.Directory]::CreateDirectory($outputPath)
$buildRoot = Join-Path $outputPath "build"
$clientRoot = Join-Path $outputPath "client"
if (Test-Path -LiteralPath $clientRoot) {
    if (-not (Test-Path -LiteralPath $clientRoot -PathType Container) -or @(Get-ChildItem -LiteralPath $clientRoot -Force).Count -ne 0) {
        throw "The runtime destination must be absent or empty: $clientRoot"
    }
}

$buildParameters = @{
    Configuration = "Release"
    Architecture = "x64"
    Renderer = "DX11"
    AudioBackend = "Juce"
    PlatformToolset = $PlatformToolset
    OutputRoot = $buildRoot
    MaxCpuCount = $MaxCpuCount
}
if ($VisualStudioRoot) {
    $buildParameters.VisualStudioRoot = $VisualStudioRoot
}

& (Join-Path $PSScriptRoot "Build-Client.ps1") @buildParameters
if (-not $?) {
    throw "The Release x64/DX11 client build failed."
}

& (Join-Path $PSScriptRoot "Stage-SteamDeckClient.ps1") `
    -ClientAssetRoot $assetRoot `
    -BuildOutputRoot $buildRoot `
    -Destination $clientRoot
if (-not $?) {
    throw "Steam Deck runtime staging failed."
}

Write-Host "Steam Deck build output: $buildRoot"
Write-Host "Steam Deck runtime: $clientRoot"
