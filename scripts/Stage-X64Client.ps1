[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string]$ClientRoot,

    [ValidateSet("Release", "Optimized", "Debug")]
    [string]$Configuration = "Release",

    [ValidateSet("None", "Precu")]
    [string]$RuntimeProfile = "None",

    [switch]$NoBackup
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-ChildRelativePath {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string]$Path
    )

    $rootPath = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $childPath = [IO.Path]::GetFullPath($Path)
    if (-not $childPath.StartsWith($rootPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path is outside the expected root: $childPath"
    }

    return $childPath.Substring($rootPath.Length)
}

function Get-PeMachine {
    param([Parameter(Mandatory)][string]$Path)

    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($reader.ReadUInt16() -ne 0x5a4d) {
            throw "Not a PE file: $Path"
        }

        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "Invalid PE signature: $Path"
        }

        return $reader.ReadUInt16()
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $ClientRoot -PathType Container)) {
    throw "ClientRoot does not exist: $ClientRoot"
}

$clientRootPath = (Resolve-Path -LiteralPath $ClientRoot).Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path

$profileFiles = @()
$shaderOverrideFiles = @()
$precuManagedLoosePaths = @()
$precuAssetArchiveFiles = @()
if ($RuntimeProfile -eq "Precu") {
    $profileDirectory = Join-Path $repoRoot "config\precu"
    $profileFiles = @(
        "client.cfg",
        "precu_login.cfg",
        "precu_live.cfg",
        "precu_preload.cfg",
        "options.cfg"
    ) | ForEach-Object {
        [pscustomobject]@{
            Source = Join-Path $profileDirectory $_
            Name   = $_
        }
    }

    foreach ($file in $profileFiles) {
        if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf)) {
            throw "Pre-CU runtime profile file is missing: $($file.Source)"
        }
    }

    $profileOptions = Get-Content -LiteralPath (Join-Path $profileDirectory "options.cfg") -Raw
    $profileClient = Get-Content -LiteralPath (Join-Path $profileDirectory "client.cfg") -Raw
    if ($profileOptions -notmatch '(?ms)^\[ClientGraphics\].*?^\s*rasterMajor\s*=\s*11\s*$') {
        throw "The Pre-CU runtime profile must select the DX11 renderer (rasterMajor=11)."
    }

    if ($profileOptions -notmatch '(?ms)^\[ClientAudio\].*?^\s*disableMiles\s*=\s*false\s*$') {
        throw "The Pre-CU runtime profile must enable the JUCE audio backend (disableMiles=false)."
    }

    $userConfigInclude = $profileClient.IndexOf('.include "user.cfg"')
    $audioOverride = $profileClient.LastIndexOf("[ClientAudio]")
    if ($userConfigInclude -lt 0 -or $audioOverride -le $userConfigInclude -or
        $profileClient.Substring($audioOverride) -notmatch '(?m)^\s*disableMiles\s*=\s*false\s*$') {
        throw "The Pre-CU client profile must lock JUCE audio on after user.cfg."
    }

    foreach ($inputSetting in @("useKeyboard", "useMouse")) {
        if ($profileOptions -notmatch "(?m)^\s*$inputSetting\s*=\s*true\s*$") {
            throw "The Pre-CU runtime profile must enable $inputSetting."
        }
    }

    $shaderOverrideDirectory = Join-Path $repoRoot "scripts\asm2hlsl\converted"
    if (-not (Test-Path -LiteralPath $shaderOverrideDirectory -PathType Container)) {
        throw "The DX11 legacy shader override directory is missing: $shaderOverrideDirectory"
    }

    $shaderOverrideFiles = @(
        Get-ChildItem -LiteralPath $shaderOverrideDirectory -Recurse -File |
            ForEach-Object {
                [pscustomobject]@{
                    Source = $_.FullName
                    Name   = Get-ChildRelativePath -Root $shaderOverrideDirectory -Path $_.FullName
                }
            }
    )
    if ($shaderOverrideFiles.Count -eq 0) {
        throw "The DX11 legacy shader override directory is empty: $shaderOverrideDirectory"
    }

    # The restored skills mediator depends on the asset repository's proven
    # Publish 14 layout. In particular, each skill-box wrapper owns an xpbar;
    # leaving this override behind falls back to the retail singleton bar.
    $precuAssetsRoot = Join-Path (Split-Path -Parent $repoRoot) "pre-cu-reborn-assets"
    foreach ($archiveName in @("precu_runtime.tre", "precu_worlds.tre")) {
        $sourcePath = Join-Path $precuAssetsRoot $archiveName
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "The generated Pre-CU asset archive is missing: $sourcePath"
        }

        $precuAssetArchiveFiles += [pscustomobject]@{
            Source = $sourcePath
            Name   = $archiveName
        }
    }

    # Every response entry is owned by precu_runtime.tre. Remove any old loose
    # copy because SearchAbsolute would otherwise mask the versioned archive.
    $runtimeResponsePath = Join-Path $precuAssetsRoot "precu_runtime.rsp"
    if (-not (Test-Path -LiteralPath $runtimeResponsePath -PathType Leaf)) {
        throw "The Pre-CU runtime response is missing: $runtimeResponsePath"
    }
    $precuManagedLoosePaths = @(
        Get-Content -LiteralPath $runtimeResponsePath |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            ForEach-Object {
                $parts = @($_ -split '\s+@\s+', 2)
                if ($parts.Count -ne 2 -or $parts[0] -cne $parts[1]) {
                    throw "Invalid Pre-CU runtime response entry: $_"
                }
                $parts[0].Replace("/", "\")
            } |
            Sort-Object -Unique
    )
    foreach ($relativePath in $precuManagedLoosePaths) {
        $sourcePath = Join-Path $precuAssetsRoot $relativePath
        Get-ChildRelativePath -Root $precuAssetsRoot -Path $sourcePath | Out-Null
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "The Pre-CU archive input is missing: $sourcePath"
        }
    }

    $skillUiText = Get-Content -LiteralPath (
        Join-Path $precuAssetsRoot "ui\ui_skill.inc"
    ) -Raw
    if ($skillUiText -notmatch "Name='xpbar'" -or
        $skillUiText -notmatch "Name='fill'") {
        throw "The Pre-CU skill asset does not contain per-box XP progress widgets."
    }
}
elseif (-not (Test-Path -LiteralPath (Join-Path $clientRootPath "client.cfg") -PathType Leaf)) {
    throw "ClientRoot does not contain client.cfg: $clientRootPath"
}

$treFiles = @(Get-ChildItem -LiteralPath $clientRootPath -Filter "*.tre" -File)
if ($treFiles.Count -eq 0) {
    throw "ClientRoot does not contain root TRE files: $clientRootPath"
}

$suffix = @{
    Release   = "r"
    Optimized = "o"
    Debug     = "d"
}[$Configuration]

$clientExecutableName = @{
    Release   = "Galaxies Reborn PRECU.exe"
    Optimized = "Galaxies Reborn PRECU_o.exe"
    Debug     = "Galaxies Reborn PRECU_d.exe"
}[$Configuration]

$runtimeFiles = @(
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\$clientExecutableName"
        Name   = $clientExecutableName
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\gl11_$suffix.dll"
        Name   = "gl11_$suffix.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\DllExport.dll"
        Name   = "DllExport.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "deps\x64\bin\SDL3.dll"
        Name   = "SDL3.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "deps\x64\bin\libxml2.dll"
        Name   = "libxml2.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "deps\x64\bin\iconv-2.dll"
        Name   = "iconv-2.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "deps\x64\bin\z.dll"
        Name   = "z.dll"
    }
)

foreach ($file in $runtimeFiles) {
    if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf)) {
        throw "Runtime source is missing. Build the client first: $($file.Source)"
    }

    $machine = Get-PeMachine -Path $file.Source
    if ($machine -ne 0x8664) {
        throw ("Refusing to stage non-x64 PE 0x{0:x4}: {1}" -f $machine, $file.Source)
    }
}

$systemRequirements = @(
    (Join-Path $env:SystemRoot "System32\vcruntime140.dll")
)
foreach ($path in $systemRequirements) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required x64 runtime is missing: $path"
    }

    if ((Get-PeMachine -Path $path) -ne 0x8664) {
        throw "Required system runtime is not x64: $path"
    }
}

$localSystemDllNames = @(
    "dbghelp.dll",
    "dbghelp_6.3.17.0.dll",
    "d3d9.dll",
    "d3dx9_43.dll",
    "ddraw.dll",
    "dinput8.dll",
    "vcruntime140.dll"
)
$incompatibleLocalPaths = @(
    $localSystemDllNames | ForEach-Object {
        $path = Join-Path $clientRootPath $_
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            if ((Get-PeMachine -Path $path) -ne 0x8664) {
                $path
            }
        }
    }
)
$obsoleteRuntimeNames = @(
    "SwgClient_d.exe", "SwgClient_o.exe", "SwgClient_r.exe",
    "Galaxies PRECU Reborn.exe", "Galaxies PRECU Reborn_o.exe", "Galaxies PRECU Reborn_d.exe",
    "mss64.dll",
    "gl00_d.dll", "gl00_o.dll", "gl00_r.dll",
    "gl05_d.dll", "gl05_o.dll", "gl05_r.dll",
    "gl06_d.dll", "gl06_o.dll", "gl06_r.dll",
    "gl07_d.dll", "gl07_o.dll", "gl07_r.dll"
)
$obsoleteRuntimePaths = @(
    $obsoleteRuntimeNames |
        ForEach-Object { Join-Path $clientRootPath $_ } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
)

if (-not $PSCmdlet.ShouldProcess($clientRootPath, "stage $Configuration x64 gameplay client")) {
    return
}

$stagedSources = @($runtimeFiles) + @($profileFiles) +
    @($shaderOverrideFiles) + @($precuAssetArchiveFiles)

$precuLooseOverridePaths = @(
    $precuManagedLoosePaths |
        ForEach-Object { Join-Path $clientRootPath $_ } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
)

$backupDirectory = $null
if (-not $NoBackup) {
    $existingTargets = @(
        $stagedSources |
            ForEach-Object { Join-Path $clientRootPath $_.Name } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
    )

    $manifestPath = Join-Path $clientRootPath "x64-runtime-manifest.json"
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        $existingTargets += $manifestPath
    }
    $existingTargets += $incompatibleLocalPaths
    $existingTargets += $obsoleteRuntimePaths
    $existingTargets += $precuLooseOverridePaths
    $existingTargets = @($existingTargets | Sort-Object -Unique)

    if ($existingTargets.Count -gt 0) {
        $backupDirectory = Join-Path $clientRootPath (".x64-backups\{0}" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
        if (Test-Path -LiteralPath $backupDirectory) {
            $backupDirectory += "-$PID"
        }

        New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
        foreach ($path in $existingTargets) {
            $relativePath = Get-ChildRelativePath -Root $clientRootPath -Path $path
            $backupPath = Join-Path $backupDirectory $relativePath
            $backupParent = Split-Path -Parent $backupPath
            New-Item -ItemType Directory -Path $backupParent -Force | Out-Null
            Copy-Item -LiteralPath $path -Destination $backupPath -Force
        }
    }
}

foreach ($path in @($incompatibleLocalPaths) + @($obsoleteRuntimePaths) + @($precuLooseOverridePaths)) {
    Get-ChildRelativePath -Root $clientRootPath -Path $path | Out-Null
    Remove-Item -LiteralPath $path -Force
}

foreach ($file in $stagedSources) {
    $destination = Join-Path $clientRootPath $file.Name
    $destinationParent = Split-Path -Parent $destination
    New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
    Copy-Item -LiteralPath $file.Source -Destination $destination -Force
}

$gitCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
$gitBranch = (& git -C $repoRoot branch --show-current).Trim()
$workingTreeDirty = @(& git -C $repoRoot status --porcelain).Count -gt 0

$stagedFiles = @(
    $stagedSources | ForEach-Object {
        $destination = Join-Path $clientRootPath $_.Name
        [ordered]@{
            name   = $_.Name
            bytes  = (Get-Item -LiteralPath $destination).Length
            sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        }
    }
)

$manifest = [ordered]@{
    formatVersion    = 1
    generatedAtUtc   = [DateTime]::UtcNow.ToString("o")
    configuration    = $Configuration
    platform         = "x64"
    runtimeProfile   = $RuntimeProfile
    sourceRepository = $repoRoot
    sourceCommit     = $gitCommit
    sourceBranch     = $gitBranch
    workingTreeDirty = $workingTreeDirty
    clientRoot       = $clientRootPath
    rootTreCount     = @(Get-ChildItem -LiteralPath $clientRootPath -Filter "*.tre" -File).Count
    rendererBackend = "Direct3d11 (gl11)"
    shaderOverrideCount = $shaderOverrideFiles.Count
    precuAssetArchiveCount = $precuAssetArchiveFiles.Count
    precuAssetOverrideCount = 0
    inputBackend     = "SDL 3.4.10 multi-device controller input"
    audioBackend     = "JUCE 8.0.14 with WASAPI and WAV/MP3/Ogg decoders"
    backupDirectory  = $backupDirectory
    removedIncompatibleLocalFiles = @($incompatibleLocalPaths | ForEach-Object { [IO.Path]::GetFileName($_) })
    removedObsoleteRuntimeFiles = @($obsoleteRuntimePaths | ForEach-Object { [IO.Path]::GetFileName($_) })
    removedPrecuLooseOverrides = @($precuLooseOverridePaths | ForEach-Object {
        Get-ChildRelativePath -Root $clientRootPath -Path $_
    })
    files            = $stagedFiles
}

$json = $manifest | ConvertTo-Json -Depth 5
$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $clientRootPath "x64-runtime-manifest.json"), $json + [Environment]::NewLine, $utf8NoBom)

Write-Host "Staged $($stagedSources.Count) x64 runtime/profile/shader/asset files to $clientRootPath"
if ($backupDirectory) {
    Write-Host "Previous runtime files were backed up to $backupDirectory"
}
Write-Host "Audio backend: JUCE 8.0.14 with WASAPI and WAV, MP3, and Ogg Vorbis decoding."
