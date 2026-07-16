[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [string]$PlatformToolset = "v145",

    [string]$VisualStudioRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-PeMachine {
    param([Parameter(Mandatory)][string]$Path)

    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset + 4
        return $reader.ReadUInt16()
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$solution = Join-Path $repoRoot "src\build\win32\swg.sln"
$project = Join-Path $repoRoot "src\game\client\application\EntertainerAudioTest\build\win32\EntertainerAudioTest.vcxproj"
$prerequisites = & (Join-Path $PSScriptRoot "Test-X64BuildPrerequisites.ps1") `
    -PlatformToolset $PlatformToolset `
    -VisualStudioRoot $VisualStudioRoot `
    -Quiet `
    -PassThru
$msbuild = $prerequisites.VisualStudio.MSBuildPath
$env:DXSDK_DIR = $prerequisites.DirectXSdk.Root.TrimEnd("\") + "\"

Write-Host "Building the shared clientAudio backend..."
& $msbuild $solution `
    "/t:clientAudio" `
    "/p:Configuration=$Configuration" `
    "/p:Platform=x64" `
    "/p:PlatformToolset=$PlatformToolset" `
    "/m" `
    "/nr:false" `
    "/v:minimal"
if ($LASTEXITCODE -ne 0) {
    throw "The clientAudio build failed with exit code $LASTEXITCODE."
}

Write-Host "Building the standalone Entertainer audio test..."
& $msbuild $project `
    "/p:Configuration=$Configuration" `
    "/p:Platform=x64" `
    "/p:PlatformToolset=$PlatformToolset" `
    "/m" `
    "/nr:false" `
    "/v:minimal"
if ($LASTEXITCODE -ne 0) {
    throw "The Entertainer audio test build failed with exit code $LASTEXITCODE."
}

$artifact = Join-Path $repoRoot "src\build\win32\x64\$Configuration\EntertainerAudioTest.exe"
if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) {
    throw "Expected build artifact is missing: $artifact"
}
if ((Get-PeMachine -Path $artifact) -ne 0x8664) {
    throw "The Entertainer audio test is not an x64 executable: $artifact"
}

$item = Get-Item -LiteralPath $artifact
$hash = (Get-FileHash -LiteralPath $artifact -Algorithm SHA256).Hash
Write-Host ("x64  {0,10:N0} bytes  {1}  {2}" -f $item.Length, $hash, $artifact)
