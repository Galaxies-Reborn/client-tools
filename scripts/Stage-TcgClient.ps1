[CmdletBinding()]
param(
    [ValidateSet("Release", "Optimized", "Debug")]
    [string]$Configuration = "Release",

    [ValidateSet("x86", "x64")]
    [string]$Architecture = "x86",

    [ValidateSet("Juce", "Miles")]
    [string]$AudioBackend = "Juce",

    [string]$ReferenceClient,

    [string]$OutputRoot,

    [string]$BuildRoot,

    [switch]$LocalDevKit,

    [string]$ExpectedTcgDllSha256 = "139399A654D1FA8780A03F31249920F75A31C9C8D1E4199B12EF204F6458ABB9"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-NormalizedPath {
    param([Parameter(Mandatory)][string]$Path)

    return [IO.Path]::GetFullPath($Path).TrimEnd([char[]]@(92, 47))
}

function Test-PathWithin {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Parent
    )

    $prefix = $Parent.TrimEnd([char[]]@(92, 47)) + [IO.Path]::DirectorySeparatorChar
    return $Path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparsePoints {
    param([Parameter(Mandatory)][string]$Path)

    $fullPath = [IO.Path]::GetFullPath($Path)
    $current = [IO.Path]::GetPathRoot($fullPath)
    $relative = $fullPath.Substring($current.Length)
    foreach ($component in $relative.Split([char[]]@(92, 47), [StringSplitOptions]::RemoveEmptyEntries)) {
        $current = Join-Path $current $component
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Staging path must not traverse a reparse point: $current"
            }
        }
    }
}

function Assert-NoReparsePointsBelow {
    param([Parameter(Mandatory)][string]$Root)

    if (-not (Test-Path -LiteralPath $Root -PathType Container)) {
        return
    }
    $reparsePoint = Get-ChildItem -LiteralPath $Root -Recurse -Force |
        Where-Object { ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 } |
        Select-Object -First 1
    if ($reparsePoint) {
        throw "Staging tree contains a reparse point: $($reparsePoint.FullName)"
    }
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

function Get-PeMachineIfPresent {
    param([Parameter(Mandatory)][string]$Path)

    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5a4d) {
            return $null
        }

        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or ($peOffset + 6) -gt $stream.Length) {
            return $null
        }

        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            return $null
        }

        return $reader.ReadUInt16()
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

function Copy-X64ReferenceAssets {
    param(
        [Parameter(Mandatory)][string]$SourceRoot,
        [Parameter(Mandatory)][string]$DestinationRoot
    )

    $sourcePrefix = $SourceRoot.TrimEnd([char[]]@(92, 47)) + [IO.Path]::DirectorySeparatorChar
    $copied = [Collections.Generic.List[object]]::new()
    $excludedPe = [Collections.Generic.List[object]]::new()
    $selectedFiles = [Collections.Generic.List[IO.FileInfo]]::new()
    $selectedPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)

    $rootTreFiles = @(Get-ChildItem -LiteralPath $SourceRoot -File -Filter *.tre | Sort-Object Name)
    if ($rootTreFiles.Count -ne 198) {
        throw "Expected exactly 198 root-level TRE archives in the final-live reference; found $($rootTreFiles.Count): $SourceRoot"
    }
    foreach ($sourceFile in $rootTreFiles) {
        if ($selectedPaths.Add($sourceFile.FullName)) {
            $selectedFiles.Add($sourceFile)
        }
    }

    $requiredRootFiles = @(
        "sku0_client.toc",
        "sku1_client.toc",
        "sku2_client.toc",
        "sku3_client.toc",
        "client.cfg",
        "live.cfg",
        "login.cfg",
        "preload.cfg",
        "options.cfg",
        "user.cfg"
    )
    foreach ($name in $requiredRootFiles) {
        $path = Join-Path $SourceRoot $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required x64 reference asset is missing: $path"
        }
        $sourceFile = Get-Item -LiteralPath $path
        if ($selectedPaths.Add($sourceFile.FullName)) {
            $selectedFiles.Add($sourceFile)
        }
    }

    $requiredNestedFiles = @(
        "forcefeedback\armor_hit.ffe",
        "forcefeedback\chassis_hit.ffe",
        "forcefeedback\component_hit.ffe",
        "forcefeedback\death_hit.ffe",
        "forcefeedback\guns_fire.ffe",
        "forcefeedback\hyperspace.ffe",
        "forcefeedback\nebula_rumble.ffe",
        "forcefeedback\shield_hit.ffe",
        "forcefeedback\wpn_fire_hvy.ffe",
        "forcefeedback\wpn_fire_lit.ffe",
        "forcefeedback\wpn_fire_med.ffe",
        "string\en\live_motd.stf"
    )
    foreach ($relativePath in $requiredNestedFiles) {
        $path = Join-Path $SourceRoot $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required x64 reference asset is missing: $path"
        }
        $sourceFile = Get-Item -LiteralPath $path
        if ($selectedPaths.Add($sourceFile.FullName)) {
            $selectedFiles.Add($sourceFile)
        }
    }

    foreach ($sourceFile in $selectedFiles | Sort-Object FullName) {
        $relativePath = $sourceFile.FullName.Substring($sourcePrefix.Length)
        $machine = Get-PeMachineIfPresent -Path $sourceFile.FullName
        if ($null -ne $machine) {
            throw ("Selected reference asset is unexpectedly a PE image (0x{0:x4}): {1}" -f $machine, $sourceFile.FullName)
        }

        $destinationFile = Join-Path $DestinationRoot $relativePath
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destinationFile)) | Out-Null
        [IO.File]::Copy($sourceFile.FullName, $destinationFile, $false)
        [IO.File]::SetLastWriteTimeUtc($destinationFile, $sourceFile.LastWriteTimeUtc)

        $copied.Add([pscustomobject]@{
            Path = $relativePath
            Length = $sourceFile.Length
            SHA256 = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash
        })
    }

    foreach ($sourceFile in Get-ChildItem -LiteralPath $SourceRoot -Recurse -Force -File | Sort-Object FullName) {
        $machine = Get-PeMachineIfPresent -Path $sourceFile.FullName
        if ($null -eq $machine) {
            continue
        }
        $excludedPe.Add([pscustomobject]@{
            Path = $sourceFile.FullName.Substring($sourcePrefix.Length)
            Machine = ("0x{0:x4}" -f $machine)
            Length = $sourceFile.Length
            SHA256 = (Get-FileHash -LiteralPath $sourceFile.FullName -Algorithm SHA256).Hash
        })
    }

    return [pscustomobject]@{
        Copied = @($copied)
        ExcludedPe = @($excludedPe)
    }
}

function Get-TreeManifest {
    param([Parameter(Mandatory)][string]$Root)

    $prefix = $Root.TrimEnd([char[]]@(92, 47)) + [IO.Path]::DirectorySeparatorChar
    return @(
        Get-ChildItem -LiteralPath $Root -Recurse -Force -File |
            Sort-Object FullName |
            ForEach-Object {
                [pscustomobject]@{
                    Path   = $_.FullName.Substring($prefix.Length)
                    Length = $_.Length
                    SHA256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
                }
            }
    )
}

function Assert-ManifestsEqual {
    param(
        [Parameter(Mandatory)][object[]]$Expected,
        [Parameter(Mandatory)][object[]]$Actual,
        [Parameter(Mandatory)][string]$Description
    )

    $difference = @(Compare-Object $Expected $Actual -Property Path, Length, SHA256)
    if ($difference.Count -ne 0) {
        $difference | Select-Object -First 20 | Format-Table | Out-Host
        throw "$Description failed with $($difference.Count) manifest difference(s)."
    }
}

$repoRoot = Get-NormalizedPath (Join-Path $PSScriptRoot "..")
$workspaceRoot = Get-NormalizedPath (Join-Path $repoRoot "..")

if (-not $ReferenceClient) {
    $ReferenceClient = Join-Path $workspaceRoot "_client"
}
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $workspaceRoot "_whitengold_client"
}

$referenceRoot = Get-NormalizedPath $ReferenceClient
$outputRootPath = Get-NormalizedPath $OutputRoot
$allowedReferenceRoot = Get-NormalizedPath (Join-Path $workspaceRoot "_client")
$allowedOutputRoot = Get-NormalizedPath (Join-Path $workspaceRoot "_whitengold_client")

if (-not (Test-Path -LiteralPath $referenceRoot -PathType Container)) {
    throw "Reference client does not exist: $referenceRoot"
}
if ($referenceRoot.Equals($outputRootPath, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-PathWithin -Path $outputRootPath -Parent $referenceRoot) -or
    (Test-PathWithin -Path $referenceRoot -Parent $outputRootPath)) {
    throw "ReferenceClient and OutputRoot must be separate, non-nested directories."
}
if (-not $outputRootPath.Equals($allowedOutputRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputRoot must be the designated writable client root: $allowedOutputRoot"
}
if (-not $referenceRoot.Equals($allowedReferenceRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ReferenceClient must be the designated read-only live client: $allowedReferenceRoot"
}
Assert-NoReparsePoints -Path $referenceRoot
Assert-NoReparsePoints -Path $outputRootPath
Assert-NoReparsePointsBelow -Root $outputRootPath

if ($Architecture -eq "x64" -and $Configuration -eq "Optimized") {
    throw "The x64 compatibility host supports Release and Debug configurations; Optimized is not available."
}

$platform = if ($Architecture -eq "x64") { "x64" } else { "Win32" }
$dependencyArchitecture = if ($Architecture -eq "x64") { "x64" } else { "win32" }
$runtimeArchitecture = if ($Architecture -eq "x64") { "x64" } else { "win32" }
$runtimeName = "{0}-dx11-{1}-{2}" -f $runtimeArchitecture, $Configuration.ToLowerInvariant(), $AudioBackend.ToLowerInvariant()
$runtimeRoot = Get-NormalizedPath (Join-Path $outputRootPath "runtime\$runtimeName")

if (-not (Test-PathWithin -Path $runtimeRoot -Parent $outputRootPath)) {
    throw "Resolved runtime directory escaped OutputRoot: $runtimeRoot"
}
if (Test-Path -LiteralPath $runtimeRoot) {
    $existing = Get-ChildItem -LiteralPath $runtimeRoot -Force | Select-Object -First 1
    if ($existing) {
        throw "Runtime directory is not empty; choose a fresh OutputRoot or remove it deliberately: $runtimeRoot"
    }
}

if (-not $BuildRoot) {
    $BuildRoot = Join-Path $outputRootPath "build\tcg-reborn"
}
$buildRootPath = Get-NormalizedPath $BuildRoot
if (-not ($buildRootPath.Equals($outputRootPath, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-PathWithin -Path $buildRootPath -Parent $outputRootPath))) {
    throw "BuildRoot must be inside the designated writable client root: $outputRootPath"
}
Assert-NoReparsePoints -Path $buildRootPath
$artifactRoot = Join-Path $buildRootPath "artifacts\$platform\$Configuration\$AudioBackend"
Assert-NoReparsePoints -Path $artifactRoot

$suffix = @{
    Release   = "r"
    Optimized = "o"
    Debug     = "d"
}[$Configuration]

$clientArtifact = Join-Path $artifactRoot "SwgClient_$suffix.exe"
$rendererArtifact = Join-Path $artifactRoot "gl11_$suffix.dll"
$hostArtifact = if ($Architecture -eq "x64") {
    Join-Path $buildRootPath "artifacts\Win32\$Configuration\$AudioBackend\TcgCompatibilityHost.exe"
}
else {
    $null
}
$browserHostArtifact = if ($Architecture -eq "x64") {
    Join-Path $buildRootPath "artifacts\Win32\$Configuration\$AudioBackend\BrowserCompatibilityHost.exe"
}
else {
    $null
}
$mozillaRuntimeSource = if ($Architecture -eq "x64") {
    Join-Path $repoRoot "src\external\3rd\library\libMozilla\include\private\bin\release"
}
else {
    $null
}
$mozillaCrtSource = if ($Architecture -eq "x64") {
    Join-Path $repoRoot "deps\win32\bin\msvcr71.dll"
}
else {
    $null
}
$dependencyBin = Join-Path $repoRoot "deps\$dependencyArchitecture\bin"
if ($Architecture -eq "x64") {
    $runtimeDependencies = @(
        "SDL3.dll",
        "libxml2.dll",
        "iconv-2.dll",
        "z.dll"
    ) | ForEach-Object { Get-Item -LiteralPath (Join-Path $dependencyBin $_) }
}
else {
    $runtimeDependencies = @(Get-ChildItem -LiteralPath $dependencyBin -Filter *.dll -File)
}
if ($Architecture -eq "x64" -and $AudioBackend -eq "Miles") {
    $runtimeDependencies += Get-Item -LiteralPath (Join-Path $repoRoot "mss64-stub\mss64.dll")
}
$requiredArtifacts = @($clientArtifact, $rendererArtifact) + @($runtimeDependencies.FullName)
foreach ($artifact in $requiredArtifacts) {
    if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) {
        throw "Required build artifact is missing: $artifact"
    }
}

$expectedMachine = if ($Architecture -eq "x64") { 0x8664 } else { 0x014c }
foreach ($artifact in $requiredArtifacts) {
    $machine = Get-PeMachine -Path $artifact
    if ($machine -ne $expectedMachine) {
        throw ("Expected PE machine 0x{0:x4}, found 0x{1:x4}: {2}" -f $expectedMachine, $machine, $artifact)
    }
}

$hostArtifactHash = $null
$hostArtifactLength = 0L
$browserHostArtifactHash = $null
$browserHostArtifactLength = 0L
$mozillaCrtHash = $null
$mozillaCrtLength = 0L
$mozillaRuntimeManifest = @()
$mozillaBrokerExpectedManifest = @()
$mozillaBrokerPeMachineByPath = $null
if ($Architecture -eq "x64") {
    Assert-NoReparsePoints -Path ([IO.Path]::GetDirectoryName($hostArtifact))
    if (-not (Test-Path -LiteralPath $hostArtifact -PathType Leaf)) {
        throw "Required x86 TCG compatibility-host build artifact is missing: $hostArtifact"
    }
    $hostMachine = Get-PeMachine -Path $hostArtifact
    if ($hostMachine -ne 0x014c) {
        throw ("Expected compatibility-host PE machine 0x014c, found 0x{0:x4}: {1}" -f $hostMachine, $hostArtifact)
    }
    $hostArtifactItem = Get-Item -LiteralPath $hostArtifact
    $hostArtifactLength = $hostArtifactItem.Length
    $hostArtifactHash = (Get-FileHash -LiteralPath $hostArtifact -Algorithm SHA256).Hash

    Assert-NoReparsePoints -Path ([IO.Path]::GetDirectoryName($browserHostArtifact))
    if (-not (Test-Path -LiteralPath $browserHostArtifact -PathType Leaf)) {
        throw "Required x86 browser compatibility-host build artifact is missing: $browserHostArtifact"
    }
    $browserHostMachine = Get-PeMachine -Path $browserHostArtifact
    if ($browserHostMachine -ne 0x014c) {
        throw ("Expected browser compatibility-host PE machine 0x014c, found 0x{0:x4}: {1}" -f $browserHostMachine, $browserHostArtifact)
    }
    $browserHostArtifactItem = Get-Item -LiteralPath $browserHostArtifact
    $browserHostArtifactLength = $browserHostArtifactItem.Length
    $browserHostArtifactHash = (Get-FileHash -LiteralPath $browserHostArtifact -Algorithm SHA256).Hash

    if (-not (Test-Path -LiteralPath $mozillaRuntimeSource -PathType Container)) {
        throw "Required legacy Mozilla runtime payload is missing: $mozillaRuntimeSource"
    }
    if (-not (Test-Path -LiteralPath $mozillaCrtSource -PathType Leaf)) {
        throw "Required legacy Mozilla CRT dependency is missing: $mozillaCrtSource"
    }
    Assert-NoReparsePoints -Path $mozillaRuntimeSource
    Assert-NoReparsePointsBelow -Root $mozillaRuntimeSource
    Assert-NoReparsePoints -Path $mozillaCrtSource

    $mozillaCrtMachine = Get-PeMachine -Path $mozillaCrtSource
    if ($mozillaCrtMachine -ne 0x014c) {
        throw ("Expected legacy Mozilla CRT PE machine 0x014c, found 0x{0:x4}: {1}" -f $mozillaCrtMachine, $mozillaCrtSource)
    }
    $mozillaCrtItem = Get-Item -LiteralPath $mozillaCrtSource
    $mozillaCrtLength = $mozillaCrtItem.Length
    $mozillaCrtHash = (Get-FileHash -LiteralPath $mozillaCrtSource -Algorithm SHA256).Hash

    Write-Host "Hashing the exact legacy Mozilla runtime payload..."
    $mozillaRuntimeManifest = Get-TreeManifest -Root $mozillaRuntimeSource
    if ($mozillaRuntimeManifest.Count -eq 0) {
        throw "The legacy Mozilla runtime payload is empty: $mozillaRuntimeSource"
    }
    $reservedBrokerPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $null = $reservedBrokerPaths.Add("BrowserCompatibilityHost.exe")
    $null = $reservedBrokerPaths.Add("msvcr71.dll")
    foreach ($entry in $mozillaRuntimeManifest) {
        if ($reservedBrokerPaths.Contains([string]$entry.Path)) {
            throw "The legacy Mozilla runtime payload collides with a broker-owned file: $($entry.Path)"
        }
    }

    $mozillaBrokerExpectedManifest = @(
        @($mozillaRuntimeManifest)
        [pscustomobject]@{
            Path = "BrowserCompatibilityHost.exe"
            Length = $browserHostArtifactLength
            SHA256 = $browserHostArtifactHash
        }
        [pscustomobject]@{
            Path = "msvcr71.dll"
            Length = $mozillaCrtLength
            SHA256 = $mozillaCrtHash
        }
    ) | Sort-Object Path

    # Every executable image in the isolated broker must be an explicitly
    # hashed Win32 image. Non-PE data is covered by the whole-tree manifest.
    $mozillaBrokerPeMachineByPath = [Collections.Generic.Dictionary[string, int]]::new([StringComparer]::OrdinalIgnoreCase)
    $mozillaRuntimeSourcePrefix = $mozillaRuntimeSource.TrimEnd([char[]]@(92, 47)) + [IO.Path]::DirectorySeparatorChar
    foreach ($mozillaRuntimeFile in Get-ChildItem -LiteralPath $mozillaRuntimeSource -Recurse -Force -File | Sort-Object FullName) {
        $mozillaRuntimeMachine = Get-PeMachineIfPresent -Path $mozillaRuntimeFile.FullName
        if ($null -eq $mozillaRuntimeMachine) {
            continue
        }
        if ($mozillaRuntimeMachine -ne 0x014c) {
            throw ("Legacy Mozilla runtime contains a non-x86 PE image (0x{0:x4}): {1}" -f $mozillaRuntimeMachine, $mozillaRuntimeFile.FullName)
        }
        $mozillaRuntimeRelativePath = $mozillaRuntimeFile.FullName.Substring($mozillaRuntimeSourcePrefix.Length)
        $mozillaBrokerPeMachineByPath.Add($mozillaRuntimeRelativePath, [int]$mozillaRuntimeMachine)
    }
    $mozillaBrokerPeMachineByPath.Add("BrowserCompatibilityHost.exe", 0x014c)
    $mozillaBrokerPeMachineByPath.Add("msvcr71.dll", 0x014c)
}

$referenceTcgRoot = Join-Path $referenceRoot "TradingCardGame"
$referenceTcgDll = Join-Path $referenceTcgRoot "SWGTCG.dll"
if (-not (Test-Path -LiteralPath $referenceTcgDll -PathType Leaf)) {
    throw "Reference TCG payload is missing SWGTCG.dll: $referenceTcgDll"
}
Assert-NoReparsePointsBelow -Root $referenceTcgRoot

$tcgDllHash = (Get-FileHash -LiteralPath $referenceTcgDll -Algorithm SHA256).Hash
if ($ExpectedTcgDllSha256 -and $tcgDllHash -ne $ExpectedTcgDllSha256) {
    throw "Reference SWGTCG.dll hash mismatch. Expected $ExpectedTcgDllSha256, found $tcgDllHash."
}

Write-Host "Hashing the read-only reference TCG payload..."
$referenceManifest = Get-TreeManifest -Root $referenceTcgRoot

New-Item -ItemType Directory -Force -Path $runtimeRoot | Out-Null
Assert-NoReparsePoints -Path $runtimeRoot
Assert-NoReparsePointsBelow -Root $runtimeRoot
$x64AssetCopy = $null
$mozillaBrokerRoot = $null
$mozillaBrokerManifestPath = $null
if ($Architecture -eq "x64") {
    Write-Host "Copying the explicit x64 reference-asset allowlist to $runtimeRoot"
    $x64AssetCopy = Copy-X64ReferenceAssets `
        -SourceRoot $referenceRoot `
        -DestinationRoot $runtimeRoot
    $stagedAssetManifest = Get-TreeManifest -Root $runtimeRoot
    Assert-ManifestsEqual `
        -Expected @($x64AssetCopy.Copied) `
        -Actual $stagedAssetManifest `
        -Description "Staged x64 reference-asset verification"

    # The compatibility host loads the original x86 TCG payload out of the
    # writable runtime. Copy the protected final-live tree byte-for-byte and
    # prove both the staged tree and its source manifest after the copy.
    $stagedTcgRoot = Join-Path $runtimeRoot "TradingCardGame"
    Write-Host "Copying the exact reference TCG payload to $stagedTcgRoot"
    & robocopy.exe $referenceTcgRoot $stagedTcgRoot /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /XJ /NFL /NDL /NP
    $tcgRobocopyExitCode = $LASTEXITCODE
    if ($tcgRobocopyExitCode -gt 7) {
        throw "TCG payload copy failed with robocopy exit code $tcgRobocopyExitCode. The reference directory was not modified."
    }
    Assert-NoReparsePointsBelow -Root $stagedTcgRoot
    $stagedManifest = Get-TreeManifest -Root $stagedTcgRoot
    Assert-ManifestsEqual -Expected $referenceManifest -Actual $stagedManifest -Description "Staged x64 TCG payload verification"

    # ClientMain resolves this path relative to SwgClient. Keep the legacy
    # browser and its dependencies isolated from the x64 DLL search path.
    $mozillaBrokerRoot = Join-Path $runtimeRoot "runtime\mozilla-broker"
    if (-not (Test-PathWithin -Path $mozillaBrokerRoot -Parent $runtimeRoot)) {
        throw "Resolved Mozilla broker directory escaped the writable runtime: $mozillaBrokerRoot"
    }
    if (Test-Path -LiteralPath $mozillaBrokerRoot) {
        throw "Mozilla broker destination must not pre-exist: $mozillaBrokerRoot"
    }

    Write-Host "Copying the exact legacy Mozilla runtime payload to $mozillaBrokerRoot"
    & robocopy.exe $mozillaRuntimeSource $mozillaBrokerRoot /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /XJ /NFL /NDL /NP
    $mozillaRobocopyExitCode = $LASTEXITCODE
    if ($mozillaRobocopyExitCode -gt 7) {
        throw "Legacy Mozilla runtime copy failed with robocopy exit code $mozillaRobocopyExitCode."
    }
    Assert-NoReparsePoints -Path $mozillaBrokerRoot
    Assert-NoReparsePointsBelow -Root $mozillaBrokerRoot
    $stagedMozillaRuntimeManifest = Get-TreeManifest -Root $mozillaBrokerRoot
    Assert-ManifestsEqual `
        -Expected $mozillaRuntimeManifest `
        -Actual $stagedMozillaRuntimeManifest `
        -Description "Staged legacy Mozilla runtime verification"

    $mozillaRuntimeManifestAfterCopy = Get-TreeManifest -Root $mozillaRuntimeSource
    Assert-ManifestsEqual `
        -Expected $mozillaRuntimeManifest `
        -Actual $mozillaRuntimeManifestAfterCopy `
        -Description "Legacy Mozilla source immutability verification"

    $stagedBrowserHost = Join-Path $mozillaBrokerRoot "BrowserCompatibilityHost.exe"
    $stagedMozillaCrt = Join-Path $mozillaBrokerRoot "msvcr71.dll"
    [IO.File]::Copy($browserHostArtifact, $stagedBrowserHost, $false)
    [IO.File]::Copy($mozillaCrtSource, $stagedMozillaCrt, $false)

    $stagedBrowserHostItem = Get-Item -LiteralPath $stagedBrowserHost
    $stagedBrowserHostHash = (Get-FileHash -LiteralPath $stagedBrowserHost -Algorithm SHA256).Hash
    if ($stagedBrowserHostItem.Length -ne $browserHostArtifactLength -or $stagedBrowserHostHash -ne $browserHostArtifactHash) {
        throw "Staged browser compatibility host does not exactly match its build artifact: $stagedBrowserHost"
    }
    if ((Get-PeMachine -Path $stagedBrowserHost) -ne 0x014c) {
        throw "Staged browser compatibility host is not an x86 PE image: $stagedBrowserHost"
    }

    $stagedMozillaCrtItem = Get-Item -LiteralPath $stagedMozillaCrt
    $stagedMozillaCrtHash = (Get-FileHash -LiteralPath $stagedMozillaCrt -Algorithm SHA256).Hash
    if ($stagedMozillaCrtItem.Length -ne $mozillaCrtLength -or $stagedMozillaCrtHash -ne $mozillaCrtHash) {
        throw "Staged legacy Mozilla CRT does not exactly match its validated dependency: $stagedMozillaCrt"
    }
    if ((Get-PeMachine -Path $stagedMozillaCrt) -ne 0x014c) {
        throw "Staged legacy Mozilla CRT is not an x86 PE image: $stagedMozillaCrt"
    }

    $stagedMozillaBrokerManifest = Get-TreeManifest -Root $mozillaBrokerRoot
    Assert-ManifestsEqual `
        -Expected $mozillaBrokerExpectedManifest `
        -Actual $stagedMozillaBrokerManifest `
        -Description "Staged Mozilla broker composite verification"
}
else {
    Write-Host "Copying the reference client to $runtimeRoot"
    & robocopy.exe $referenceRoot $runtimeRoot /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /XJ /NFL /NDL /NP
    $robocopyExitCode = $LASTEXITCODE
    if ($robocopyExitCode -gt 7) {
        throw "Robocopy failed with exit code $robocopyExitCode. The reference directory was not modified."
    }
    Assert-NoReparsePointsBelow -Root $runtimeRoot

    $stagedTcgRoot = Join-Path $runtimeRoot "TradingCardGame"
    $stagedManifest = Get-TreeManifest -Root $stagedTcgRoot
    Assert-ManifestsEqual -Expected $referenceManifest -Actual $stagedManifest -Description "Staged TCG payload verification"
}

$referenceManifestAfterCopy = Get-TreeManifest -Root $referenceTcgRoot
Assert-ManifestsEqual -Expected $referenceManifest -Actual $referenceManifestAfterCopy -Description "Reference TCG immutability verification"

[IO.File]::Copy($clientArtifact, (Join-Path $runtimeRoot "SwgClient_$suffix.exe"), $true)
[IO.File]::Copy($rendererArtifact, (Join-Path $runtimeRoot "gl11_$suffix.dll"), $true)
foreach ($dependency in $runtimeDependencies) {
    [IO.File]::Copy($dependency.FullName, (Join-Path $runtimeRoot $dependency.Name), $true)
}
if ($Architecture -eq "x64") {
    $stagedHost = Join-Path $runtimeRoot "TcgCompatibilityHost.exe"
    [IO.File]::Copy($hostArtifact, $stagedHost, $true)
    $stagedHostItem = Get-Item -LiteralPath $stagedHost
    $stagedHostHash = (Get-FileHash -LiteralPath $stagedHost -Algorithm SHA256).Hash
    if ($stagedHostItem.Length -ne $hostArtifactLength -or $stagedHostHash -ne $hostArtifactHash) {
        throw "Staged TCG compatibility host does not exactly match its build artifact: $stagedHost"
    }
    $stagedHostMachine = Get-PeMachine -Path $stagedHost
    if ($stagedHostMachine -ne 0x014c) {
        throw ("Expected staged compatibility-host PE machine 0x014c, found 0x{0:x4}: {1}" -f $stagedHostMachine, $stagedHost)
    }
}

# The live reference's client.cfg does not carry Station feature bits. Without
# the Base/expansion SKU mask, SetupSharedFile mounts no client TOCs and the
# embedded TCG path cannot reach the login/UI install path.
$embeddedClientConfig = @"
[ClientGame]
tcgDirectory=TradingCardGame

[Station]
gameFeatures=33297
subscriptionFeatures=1
"@
[IO.File]::WriteAllText(
    (Join-Path $runtimeRoot "tcg-client.cfg"),
    $embeddedClientConfig + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false)
)

$localDevKitManifestPath = $null
if ($LocalDevKit) {
    $hostConfigPath = Join-Path $stagedTcgRoot "host.svr"
    if (Test-Path -LiteralPath $hostConfigPath) {
        throw "LocalDevKit refuses to overwrite a host.svr copied from the protected reference: $hostConfigPath"
    }
    $localTcgHostConfig = @"
ghost 127.0.0.1
gport 16782
"@
    [IO.File]::WriteAllText(
        $hostConfigPath,
        $localTcgHostConfig + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false)
    )

    $loginPath = Join-Path $runtimeRoot "login.cfg"
    $localLoginConfig = @"
[ClientGame]
loginServerAddress0=127.0.0.1
loginServerPort0=44453
"@
    [IO.File]::WriteAllText(
        $loginPath,
        $localLoginConfig + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false)
    )

    $localDevKitManifestPath = Join-Path $runtimeRoot "local-devkit-runtime.json"
    $localDevKitManifest = [ordered]@{
        schema = 1
        mode = "loopback-development-only"
        tcg_gateway = "127.0.0.1:16782"
        swg_login = "127.0.0.1:44453"
        host_svr = [ordered]@{
            path = "TradingCardGame\host.svr"
            sha256 = (Get-FileHash -LiteralPath $hostConfigPath -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        login_cfg = [ordered]@{
            path = "login.cfg"
            sha256 = (Get-FileHash -LiteralPath $loginPath -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    [IO.File]::WriteAllText(
        $localDevKitManifestPath,
        ($localDevKitManifest | ConvertTo-Json -Depth 4) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false)
    )
}

$clientConfigPath = Join-Path $runtimeRoot "client.cfg"
$clientConfig = [IO.File]::ReadAllText($clientConfigPath)
if ($clientConfig -notmatch '(?im)^\s*\.include\s+"tcg-client\.cfg"\s*$') {
    if (-not $clientConfig.EndsWith([Environment]::NewLine)) {
        $clientConfig += [Environment]::NewLine
    }
    $clientConfig += '.include "tcg-client.cfg"' + [Environment]::NewLine
    [IO.File]::WriteAllText($clientConfigPath, $clientConfig, [Text.UTF8Encoding]::new($false))
}

$optionsPath = Join-Path $runtimeRoot "options.cfg"
if (-not (Test-Path -LiteralPath $optionsPath -PathType Leaf)) {
    throw "Staged client is missing options.cfg: $optionsPath"
}

$optionsReader = [IO.StreamReader]::new($optionsPath, [Text.Encoding]::Default, $true)
try {
    $options = $optionsReader.ReadToEnd()
    $optionsEncoding = $optionsReader.CurrentEncoding
}
finally {
    $optionsReader.Dispose()
}
$rasterPattern = [Text.RegularExpressions.Regex]::new('(?m)^([ \t]*rasterMajor[ \t]*=[ \t]*)\d+([ \t]*\r?)$')
if ($rasterPattern.Matches($options).Count -ne 1) {
    throw "Expected exactly one rasterMajor setting in $optionsPath"
}
$options = $rasterPattern.Replace($options, '${1}11${2}', 1)
[IO.File]::WriteAllText($optionsPath, $options, $optionsEncoding)

$manifestPath = Join-Path $runtimeRoot "tcg-reference-manifest.csv"
$referenceManifest | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding ASCII

if ($Architecture -eq "x64") {
    $x64AssetCopy.Copied |
        Export-Csv -LiteralPath (Join-Path $runtimeRoot "x64-reference-assets.csv") -NoTypeInformation -Encoding ASCII
    $x64AssetCopy.ExcludedPe |
        Export-Csv -LiteralPath (Join-Path $runtimeRoot "x64-excluded-reference-pe.csv") -NoTypeInformation -Encoding ASCII

    $sourceAssetsAfter = @(
        foreach ($entry in $x64AssetCopy.Copied) {
            $sourcePath = Join-Path $referenceRoot $entry.Path
            $sourceFile = Get-Item -LiteralPath $sourcePath
            [pscustomobject]@{
                Path = $entry.Path
                Length = $sourceFile.Length
                SHA256 = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
            }
        }
    )
    Assert-ManifestsEqual `
        -Expected @($x64AssetCopy.Copied) `
        -Actual $sourceAssetsAfter `
        -Description "Reference x64 asset immutability verification"

    $stagedMozillaBrokerManifestForAudit = Get-TreeManifest -Root $mozillaBrokerRoot
    Assert-ManifestsEqual `
        -Expected $mozillaBrokerExpectedManifest `
        -Actual $stagedMozillaBrokerManifestForAudit `
        -Description "Final Mozilla broker whole-tree verification"
    $mozillaRuntimeManifestForAudit = Get-TreeManifest -Root $mozillaRuntimeSource
    Assert-ManifestsEqual `
        -Expected $mozillaRuntimeManifest `
        -Actual $mozillaRuntimeManifestForAudit `
        -Description "Final legacy Mozilla source immutability verification"
    $mozillaBrokerManifestPath = Join-Path $runtimeRoot "mozilla-broker-manifest.csv"
    $mozillaBrokerExpectedManifest |
        Export-Csv -LiteralPath $mozillaBrokerManifestPath -NoTypeInformation -Encoding ASCII

    $dllExport = Get-ChildItem -LiteralPath $runtimeRoot -Recurse -Force -File -Filter "DllExport.dll" |
        Select-Object -First 1
    if ($dllExport) {
        throw "DllExport.dll must not be staged; the DX11 delay-load hook resolves those exports from SwgClient: $($dllExport.FullName)"
    }

    $runtimePrefix = $runtimeRoot.TrimEnd([char[]]@(92, 47)) + [IO.Path]::DirectorySeparatorChar
    $tcgRuntimePrefix = "TradingCardGame" + [IO.Path]::DirectorySeparatorChar
    $mozillaBrokerRuntimePrefix = "runtime" + [IO.Path]::DirectorySeparatorChar + "mozilla-broker" + [IO.Path]::DirectorySeparatorChar

    # Root client images are native x64. The TCG compatibility host is the
    # single root x86 exception. PE files below TradingCardGame and the isolated
    # browser broker are accepted only at exact allowlisted paths and hashes.
    $requiredRootPePaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $null = $requiredRootPePaths.Add("SwgClient_$suffix.exe")
    $null = $requiredRootPePaths.Add("gl11_$suffix.dll")
    foreach ($dependency in $runtimeDependencies) {
        $null = $requiredRootPePaths.Add($dependency.Name)
    }

    $referenceTcgManifestByPath = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $referenceManifest) {
        $referenceTcgManifestByPath.Add([string]$entry.Path, $entry)
    }
    $mozillaBrokerManifestByPath = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $mozillaBrokerExpectedManifest) {
        $mozillaBrokerManifestByPath.Add([string]$entry.Path, $entry)
    }
    $referenceTcgPeMachineByPath = [Collections.Generic.Dictionary[string, int]]::new([StringComparer]::OrdinalIgnoreCase)
    $referenceTcgPrefix = $referenceTcgRoot.TrimEnd([char[]]@(92, 47)) + [IO.Path]::DirectorySeparatorChar
    foreach ($referenceTcgFile in Get-ChildItem -LiteralPath $referenceTcgRoot -Recurse -Force -File | Sort-Object FullName) {
        $referenceMachine = Get-PeMachineIfPresent -Path $referenceTcgFile.FullName
        if ($null -ne $referenceMachine) {
            $referenceRelativePath = $referenceTcgFile.FullName.Substring($referenceTcgPrefix.Length)
            $referenceTcgPeMachineByPath.Add($referenceRelativePath, [int]$referenceMachine)
        }
    }

    $foundRootPePaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $foundReferenceTcgPePaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $foundMozillaBrokerPePaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $foundCompatibilityHost = $false
    foreach ($runtimeFile in Get-ChildItem -LiteralPath $runtimeRoot -Recurse -Force -File | Sort-Object FullName) {
        $machine = Get-PeMachineIfPresent -Path $runtimeFile.FullName
        if ($null -eq $machine) {
            continue
        }

        $relativePath = $runtimeFile.FullName.Substring($runtimePrefix.Length)
        if ($relativePath.Equals("TcgCompatibilityHost.exe", [StringComparison]::OrdinalIgnoreCase)) {
            if ($machine -ne 0x014c) {
                throw ("The compatibility-host exception must be x86 (0x014c), found 0x{0:x4}: {1}" -f $machine, $relativePath)
            }
            $actualHostHash = (Get-FileHash -LiteralPath $runtimeFile.FullName -Algorithm SHA256).Hash
            if ($runtimeFile.Length -ne $hostArtifactLength -or $actualHostHash -ne $hostArtifactHash) {
                throw "The staged compatibility host is not the exact validated build artifact: $relativePath"
            }
            $foundCompatibilityHost = $true
            continue
        }

        if ($relativePath.StartsWith($mozillaBrokerRuntimePrefix, [StringComparison]::OrdinalIgnoreCase)) {
            $mozillaBrokerRelativePath = $relativePath.Substring($mozillaBrokerRuntimePrefix.Length)
            if (-not $mozillaBrokerManifestByPath.ContainsKey($mozillaBrokerRelativePath) -or
                -not $mozillaBrokerPeMachineByPath.ContainsKey($mozillaBrokerRelativePath)) {
                throw ("Unallowlisted PE image found below runtime\mozilla-broker (0x{0:x4}): {1}" -f $machine, $relativePath)
            }

            $mozillaBrokerEntry = $mozillaBrokerManifestByPath[$mozillaBrokerRelativePath]
            $actualMozillaBrokerHash = (Get-FileHash -LiteralPath $runtimeFile.FullName -Algorithm SHA256).Hash
            if ($runtimeFile.Length -ne [long]$mozillaBrokerEntry.Length -or
                $actualMozillaBrokerHash -ne [string]$mozillaBrokerEntry.SHA256) {
                throw "Staged Mozilla broker PE does not exactly match its allowlisted source: $relativePath"
            }
            $mozillaBrokerMachine = $mozillaBrokerPeMachineByPath[$mozillaBrokerRelativePath]
            if ($machine -ne $mozillaBrokerMachine -or $machine -ne 0x014c) {
                throw ("Mozilla broker PE must match its validated x86 machine (0x{0:x4} vs 0x{1:x4}): {2}" -f $machine, $mozillaBrokerMachine, $relativePath)
            }
            $null = $foundMozillaBrokerPePaths.Add($mozillaBrokerRelativePath)
            continue
        }

        if ($relativePath.StartsWith($tcgRuntimePrefix, [StringComparison]::OrdinalIgnoreCase)) {
            $tcgRelativePath = $relativePath.Substring($tcgRuntimePrefix.Length)
            if (-not $referenceTcgManifestByPath.ContainsKey($tcgRelativePath) -or
                -not $referenceTcgPeMachineByPath.ContainsKey($tcgRelativePath)) {
                throw ("Unallowlisted PE image found below TradingCardGame (0x{0:x4}): {1}" -f $machine, $relativePath)
            }

            $referenceEntry = $referenceTcgManifestByPath[$tcgRelativePath]
            $actualTcgHash = (Get-FileHash -LiteralPath $runtimeFile.FullName -Algorithm SHA256).Hash
            if ($runtimeFile.Length -ne [long]$referenceEntry.Length -or $actualTcgHash -ne [string]$referenceEntry.SHA256) {
                throw "Staged TradingCardGame PE does not exactly match the protected reference: $relativePath"
            }
            $referenceMachine = $referenceTcgPeMachineByPath[$tcgRelativePath]
            if ($machine -ne $referenceMachine) {
                throw ("Staged TradingCardGame PE machine differs from its protected reference (0x{0:x4} vs 0x{1:x4}): {2}" -f $machine, $referenceMachine, $relativePath)
            }
            $null = $foundReferenceTcgPePaths.Add($tcgRelativePath)
            continue
        }

        if (-not $requiredRootPePaths.Contains($relativePath)) {
            throw ("Unallowlisted PE image found in the x64 runtime (0x{0:x4}): {1}" -f $machine, $relativePath)
        }
        if ($machine -ne 0x8664) {
            throw ("Non-x64 client PE image found at the runtime root (0x{0:x4}): {1}" -f $machine, $relativePath)
        }
        $null = $foundRootPePaths.Add($relativePath)
    }

    foreach ($requiredRootPePath in $requiredRootPePaths) {
        if (-not $foundRootPePaths.Contains($requiredRootPePath)) {
            throw "Required x64 runtime PE image was not found: $requiredRootPePath"
        }
    }
    if (-not $foundCompatibilityHost) {
        throw "Required x86 compatibility host was not found at the x64 runtime root."
    }
    foreach ($referenceTcgPePath in $referenceTcgPeMachineByPath.Keys) {
        if (-not $foundReferenceTcgPePaths.Contains($referenceTcgPePath)) {
            throw "Required protected-reference TradingCardGame PE was not found: $referenceTcgPePath"
        }
    }
    foreach ($mozillaBrokerPePath in $mozillaBrokerPeMachineByPath.Keys) {
        if (-not $foundMozillaBrokerPePaths.Contains($mozillaBrokerPePath)) {
            throw "Required allowlisted Mozilla broker PE was not found: $mozillaBrokerPePath"
        }
    }
}

Write-Host "Staged client: $runtimeRoot"
Write-Host "Reference TCG files verified: $($referenceManifest.Count)"
Write-Host "Reference SWGTCG.dll SHA-256: $tcgDllHash"
if ($Architecture -eq "x64") {
    Write-Host "TCG compatibility host verified: Win32 0x014c; SHA-256 $hostArtifactHash"
    Write-Host "Browser compatibility host verified: Win32 0x014c; SHA-256 $browserHostArtifactHash"
    Write-Host "Mozilla broker files verified: $($mozillaBrokerExpectedManifest.Count); manifest: $mozillaBrokerManifestPath"
}
if ($LocalDevKit) {
    Write-Host "Local dev-kit runtime configured and hashed: $localDevKitManifestPath"
}
Write-Host "Renderer configured: Direct3D 11 (rasterMajor=11)"
