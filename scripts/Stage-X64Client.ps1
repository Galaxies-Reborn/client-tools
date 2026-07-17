[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string]$ClientRoot,

    [ValidateSet("Release", "Optimized", "Debug")]
    [string]$Configuration = "Release",

    [switch]$NoBackup
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

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

if (-not (Test-Path -LiteralPath (Join-Path $clientRootPath "client.cfg") -PathType Leaf)) {
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

$runtimeFiles = @(
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\SwgClient_$suffix.exe"
        Name   = "SwgClient_$suffix.exe"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\gl05_$suffix.dll"
        Name   = "gl05_$suffix.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\gl06_$suffix.dll"
        Name   = "gl06_$suffix.dll"
    },
    [pscustomobject]@{
        Source = Join-Path $repoRoot "src\build\win32\x64\$Configuration\gl07_$suffix.dll"
        Name   = "gl07_$suffix.dll"
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
    (Join-Path $env:SystemRoot "System32\d3dx9_43.dll"),
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
$obsoleteRuntimePaths = @(
    Join-Path $clientRootPath "mss64.dll" |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
)

if (-not $PSCmdlet.ShouldProcess($clientRootPath, "stage $Configuration x64 gameplay client")) {
    return
}

$backupDirectory = $null
if (-not $NoBackup) {
    $existingTargets = @(
        $runtimeFiles |
            ForEach-Object { Join-Path $clientRootPath $_.Name } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
    )

    $manifestPath = Join-Path $clientRootPath "x64-runtime-manifest.json"
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        $existingTargets += $manifestPath
    }
    $existingTargets += $incompatibleLocalPaths
    $existingTargets += $obsoleteRuntimePaths
    $existingTargets = @($existingTargets | Sort-Object -Unique)

    if ($existingTargets.Count -gt 0) {
        $backupDirectory = Join-Path $clientRootPath (".x64-backups\{0}" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
        if (Test-Path -LiteralPath $backupDirectory) {
            $backupDirectory += "-$PID"
        }

        New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
        foreach ($path in $existingTargets) {
            Copy-Item -LiteralPath $path -Destination (Join-Path $backupDirectory ([IO.Path]::GetFileName($path))) -Force
        }
    }
}

foreach ($path in @($incompatibleLocalPaths) + @($obsoleteRuntimePaths)) {
    Remove-Item -LiteralPath $path -Force
}

foreach ($file in $runtimeFiles) {
    Copy-Item -LiteralPath $file.Source -Destination (Join-Path $clientRootPath $file.Name) -Force
}

# Stage the repo-owned Entertainer UI and wire it into the client's loose UI
# root. Loose files intentionally override archive content for this source build.
$uiDirectory = Join-Path $clientRootPath "ui"
$uiAssetDirectory = Join-Path $repoRoot "assets\ui"
New-Item -ItemType Directory -Path $uiDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $uiAssetDirectory "ui_entertainer_reborn.inc") -Destination $uiDirectory -Force
Copy-Item -LiteralPath (Join-Path $uiAssetDirectory "ui_options_entertainer.inc") -Destination $uiDirectory -Force

$utf8NoBom = [Text.UTF8Encoding]::new($false)
$uiRootPath = Join-Path $uiDirectory "ui_root.ui"
if (-not (Test-Path -LiteralPath $uiRootPath -PathType Leaf)) {
    throw "Client UI root is missing: $uiRootPath"
}
$uiRoot = [IO.File]::ReadAllText($uiRootPath)
if ($uiRoot -notmatch '(?i)<include>ui_entertainer_reborn\.inc</include>') {
    $rootMarker = "`t<include>ui_styles.inc</include>"
    if (-not $uiRoot.Contains($rootMarker)) {
        throw "Could not locate the UI root insertion marker in $uiRootPath"
    }
    $uiRoot = $uiRoot.Replace($rootMarker, "`t<include>ui_entertainer_reborn.inc</include>`r`n$rootMarker")
    [IO.File]::WriteAllText($uiRootPath, $uiRoot, $utf8NoBom)
}

$optionsUiPath = Join-Path $uiDirectory "ui_options.inc"
if (-not (Test-Path -LiteralPath $optionsUiPath -PathType Leaf)) {
    throw "Client options UI is missing: $optionsUiPath"
}
$optionsUi = [IO.File]::ReadAllText($optionsUiPath)
if ($optionsUi -notmatch "pagePerformance='comp\.target\.performance'") {
    $codeMarker = "pageKeymap='comp.target.keymap'"
    if (-not $optionsUi.Contains($codeMarker)) {
        throw "Could not locate the options CodeData marker in $optionsUiPath"
    }
    $optionsUi = $optionsUi.Replace($codeMarker, "$codeMarker`r`n`t`t`t`tpagePerformance='comp.target.performance'")
}
if ($optionsUi -notmatch "Target='target\.performance'") {
    $tabMarker = "Target='target.keymap'"
    $tabTarget = $optionsUi.IndexOf($tabMarker, [StringComparison]::Ordinal)
    $tabEnd = if ($tabTarget -ge 0) { $optionsUi.IndexOf("/>", $tabTarget, [StringComparison]::Ordinal) } else { -1 }
    if ($tabEnd -lt 0) {
        throw "Could not locate the keymap tab entry in $optionsUiPath"
    }
    $tabEnd += 2
    $performanceTab = "`r`n`t`t`t`t`t<Data localName='Performance Input' Name='Performance Input' Size='128,64' Target='target.performance'/>"
    $optionsUi = $optionsUi.Insert($tabEnd, $performanceTab)
}
if ($optionsUi -notmatch "Name='buttonPerformance'") {
    $buttonMarker = ">@ui_opt:b_keymap</Button>"
    $buttonEnd = $optionsUi.IndexOf($buttonMarker, [StringComparison]::Ordinal)
    if ($buttonEnd -lt 0) {
        throw "Could not locate the keymap button in $optionsUiPath"
    }
    $buttonEnd += $buttonMarker.Length
    $performanceButton = "`r`n`t`t`t`t`t<Button LocalText='Performance Input' Location='4,395' MaximumSize='16384,22' MinimumSize='0,19' Name='buttonPerformance' OnPress='parent.parent.tabs.activetab=13' PackSize='a' RStyleDefault='rs_default' ScrollExtent='103,22' Size='103,22' Style='/Styles.New.buttons.hud.style'>Performance Input</Button>"
    $optionsUi = $optionsUi.Insert($buttonEnd, $performanceButton)
}
if ($optionsUi -notmatch '(?i)<include>ui_options_entertainer\.inc</include>') {
    $optionsIncludeMarker = "<include>ui_options_keymap.inc</include>"
    if (-not $optionsUi.Contains($optionsIncludeMarker)) {
        throw "Could not locate the options include marker in $optionsUiPath"
    }
    $optionsUi = $optionsUi.Replace($optionsIncludeMarker, "<include>ui_options_entertainer.inc</include>`r`n`t`t`t`t`t$optionsIncludeMarker")
}
[IO.File]::WriteAllText($optionsUiPath, $optionsUi, $utf8NoBom)

$midiDirectory = Join-Path $clientRootPath "midi"
New-Item -ItemType Directory -Path $midiDirectory -Force | Out-Null
$generatedSampleBank = Join-Path $repoRoot "generated\entertainer-sample-bank\midi\instruments"
$sampleBankDirectory = Join-Path $midiDirectory "instruments"
$sampleBankCount = 0
if (Test-Path -LiteralPath (Join-Path $generatedSampleBank "bank.json") -PathType Leaf) {
    New-Item -ItemType Directory -Path $sampleBankDirectory -Force | Out-Null
    Get-ChildItem -LiteralPath $generatedSampleBank -File | Copy-Item -Destination $sampleBankDirectory -Force
    $sampleBankCount = @(Get-ChildItem -LiteralPath $sampleBankDirectory -Filter "instrument_*.wav" -File).Count
}

$gitCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
$gitBranch = (& git -C $repoRoot branch --show-current).Trim()
$workingTreeDirty = @(& git -C $repoRoot status --porcelain).Count -gt 0

$stagedFiles = @(
    $runtimeFiles | ForEach-Object {
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
    sourceRepository = $repoRoot
    sourceCommit     = $gitCommit
    sourceBranch     = $gitBranch
    workingTreeDirty = $workingTreeDirty
    clientRoot       = $clientRootPath
    rootTreCount     = $treFiles.Count
    inputBackend     = "SDL 3.4.10 multi-device controller input"
    audioBackend     = "JUCE 8.0.14 with WASAPI and WAV/MP3/Ogg decoders"
    midiDirectory    = $midiDirectory
    sampleBankDirectory = if ($sampleBankCount -gt 0) { $sampleBankDirectory } else { $null }
    sampleBankCount  = $sampleBankCount
    backupDirectory  = $backupDirectory
    removedIncompatibleLocalFiles = @($incompatibleLocalPaths | ForEach-Object { [IO.Path]::GetFileName($_) })
    removedObsoleteRuntimeFiles = @($obsoleteRuntimePaths | ForEach-Object { [IO.Path]::GetFileName($_) })
    files            = $stagedFiles
}

$json = $manifest | ConvertTo-Json -Depth 5
[IO.File]::WriteAllText((Join-Path $clientRootPath "x64-runtime-manifest.json"), $json + [Environment]::NewLine, $utf8NoBom)

Write-Host "Staged $($runtimeFiles.Count) x64 runtime files to $clientRootPath"
if ($backupDirectory) {
    Write-Host "Previous runtime files were backed up to $backupDirectory"
}
Write-Host "Audio backend: JUCE 8.0.14 with WASAPI and WAV, MP3, and Ogg Vorbis decoding."
