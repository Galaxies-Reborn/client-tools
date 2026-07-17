[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [string]$MidiFile,

    [switch]$NoBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot "Build-EntertainerAudioTest.ps1") -Configuration $Configuration
}

$executable = Join-Path $repoRoot "src\build\win32\x64\$Configuration\EntertainerAudioTest.exe"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Build the Entertainer audio test first: $executable"
}

$generatedBank = Join-Path $repoRoot "generated\entertainer-sample-bank\midi\instruments"
if (Test-Path -LiteralPath (Join-Path $generatedBank "bank.json") -PathType Leaf) {
    $runtimeBank = Join-Path (Split-Path -Parent $executable) "midi\instruments"
    New-Item -ItemType Directory -Path $runtimeBank -Force | Out-Null
    Get-ChildItem -LiteralPath $generatedBank -File | Copy-Item -Destination $runtimeBank -Force
}

if ($MidiFile) {
    $midiPath = (Resolve-Path -LiteralPath $MidiFile).Path
    $process = Start-Process -FilePath $executable -ArgumentList $midiPath -WorkingDirectory (Split-Path -Parent $executable) -WindowStyle Normal -PassThru
}
else {
    $process = Start-Process -FilePath $executable -WorkingDirectory (Split-Path -Parent $executable) -WindowStyle Normal -PassThru
}
Write-Host "Entertainer audio test started in a separate console (PID $($process.Id))."
