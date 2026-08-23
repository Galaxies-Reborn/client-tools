[CmdletBinding()]
param(
    [ValidateSet("Release", "Optimized", "Debug")]
    [string]$Configuration = "Release",

    [ValidateSet("x86", "x64", "All")]
    [string]$Architecture = "All",

    [ValidateSet("DX9", "DX11", "All")]
    [string]$Renderer = "All",

    [ValidateSet("Juce", "Miles")]
    [string]$AudioBackend = "Juce",

    [string]$PlatformToolset = "v145",

    [string]$VisualStudioRoot,

    [string]$OutputRoot,

    [ValidateRange(0, 128)]
    [int]$MaxCpuCount = 0
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
                throw "Build output path must not traverse a reparse point: $current"
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
        throw "Build output tree contains a reparse point: $($reparsePoint.FullName)"
    }
}

function Assert-Inputs {
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][string]$Platform,
        [Parameter(Mandatory)][string]$SelectedAudioBackend
    )

    $dependencyRoot = if ($Platform -eq "x64") { "deps\x64" } else { "deps\win32" }
    $required = @(
        "$dependencyRoot\include\SDL3\SDL.h",
        "$dependencyRoot\lib\SDL3.lib",
        "$dependencyRoot\bin\SDL3.dll"
    )

    if ($Platform -eq "x64") {
        $required += @(
            "deps\x64\include\libjpeg-turbo\jpeglib.h",
            "deps\x64\lib\jpeg-static.lib",
            "deps\x64\lib\libxml2.lib",
            "deps\x64\lib\pcre.lib",
            "deps\x64\lib\dpvs.lib",
            "deps\x64\lib\libEverQuestTCG.lib",
            "deps\x64\lib\vivoxSharedWrapper.lib",
            "deps\x64\lib\swg-stubs.lib",
            "src\external\3rd\application\BrowserCompatibilityHost\BrowserCompatibilityHost.vcxproj",
            "src\external\3rd\library\libMozilla\include\private\lib\release\nspr4.lib",
            "src\external\3rd\library\libMozilla\include\private\lib\release\plc4.lib",
            "src\external\3rd\library\libMozilla\include\private\lib\release\profdirserviceprovider_s.lib",
            "src\external\3rd\library\libMozilla\include\private\lib\release\xpcom.lib",
            "src\external\3rd\library\libMozilla\include\private\lib\release\xul.lib"
        )
    }

    if ($SelectedAudioBackend -eq "Juce") {
        $required += "src\external\3rd\library\JUCE-8.0.14\modules\juce_audio_devices\juce_audio_devices.cpp"
    }
    elseif ($Platform -eq "x64") {
        $required += @(
            "mss64-stub\mss64.lib",
            "mss64-stub\mss64.dll"
        )
    }

    foreach ($relativePath in $required) {
        $path = Join-Path $RepoRoot $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required build input is missing: $path"
        }
    }
}

function Get-ExpectedArtifacts {
    param(
        [Parameter(Mandatory)][string]$Platform,
        [Parameter(Mandatory)][string]$SelectedConfiguration,
        [Parameter(Mandatory)][string]$SelectedRenderer,
        [Parameter(Mandatory)][string]$SelectedAudioBackend,
        [Parameter(Mandatory)][string]$Suffix,
        [string]$ExternalOutputRoot
    )

    if ($ExternalOutputRoot) {
        $outputRoot = Join-Path $ExternalOutputRoot "artifacts\$Platform\$SelectedConfiguration\$SelectedAudioBackend"
        $artifacts = @((Join-Path $outputRoot "SwgClient_$Suffix.exe"))
        if ($SelectedRenderer -eq "DX9" -or $SelectedRenderer -eq "All") {
            $artifacts += @(
                (Join-Path $outputRoot "gl05_$Suffix.dll"),
                (Join-Path $outputRoot "gl06_$Suffix.dll"),
                (Join-Path $outputRoot "gl07_$Suffix.dll")
            )
        }
        if ($SelectedRenderer -eq "DX11" -or $SelectedRenderer -eq "All") {
            $artifacts += (Join-Path $outputRoot "gl11_$Suffix.dll")
        }
        return $artifacts
    }

    if ($Platform -eq "x64") {
        $outputRoot = "src\build\win32\x64\$SelectedConfiguration"
        $artifacts = @("$outputRoot\SwgClient_$Suffix.exe")
        if ($SelectedRenderer -eq "DX9" -or $SelectedRenderer -eq "All") {
            $artifacts += @(
                "$outputRoot\gl05_$Suffix.dll",
                "$outputRoot\gl06_$Suffix.dll",
                "$outputRoot\gl07_$Suffix.dll"
            )
        }
        if ($SelectedRenderer -eq "DX11" -or $SelectedRenderer -eq "All") {
            $artifacts += "$outputRoot\gl11_$Suffix.dll"
        }
        return $artifacts
    }

    $artifacts = @("src\compile\win32\SwgClient\$SelectedConfiguration\SwgClient_$Suffix.exe")
    if ($SelectedRenderer -eq "DX9" -or $SelectedRenderer -eq "All") {
        $artifacts += @(
            "src\compile\win32\Direct3d9\$SelectedConfiguration\gl05_$Suffix.dll",
            "src\compile\win32\Direct3d9_ffp\$SelectedConfiguration\gl06_$Suffix.dll",
            "src\compile\win32\Direct3d9_vsps\$SelectedConfiguration\gl07_$Suffix.dll"
        )
    }
    if ($SelectedRenderer -eq "DX11" -or $SelectedRenderer -eq "All") {
        $artifacts += "src\compile\win32\Direct3d11\$SelectedConfiguration\gl11_$Suffix.dll"
    }
    return $artifacts
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$solution = Join-Path $repoRoot "src\build\win32\swg.sln"
$prerequisites = & (Join-Path $PSScriptRoot "Test-ClientBuildPrerequisites.ps1") `
    -PlatformToolset $PlatformToolset `
    -VisualStudioRoot $VisualStudioRoot `
    -Quiet `
    -PassThru

$msbuild = $prerequisites.VisualStudio.MSBuildPath
$env:DXSDK_DIR = $prerequisites.DirectXSdk.Root.TrimEnd("\") + "\"

$allowedOutputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot "..\_whitengold_client")).TrimEnd([char[]]@(92, 47))
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $allowedOutputRoot "build\tcg-reborn"
}
if (-not [IO.Path]::IsPathRooted($OutputRoot) -or $OutputRoot -match '^[A-Za-z]:[^\\/]') {
    throw "OutputRoot must be a fully qualified path: $OutputRoot"
}
if ($OutputRoot.IndexOfAny([char[]]@('%', ';', ',', "'", '"', '$', '@', '(', ')')) -ge 0) {
    throw "OutputRoot contains a character that is unsafe in an MSBuild property: $OutputRoot"
}

$resolvedOutputRoot = [IO.Path]::GetFullPath($OutputRoot).TrimEnd([char[]]@(92, 47))
if (-not ($resolvedOutputRoot.Equals($allowedOutputRoot, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-PathWithin -Path $resolvedOutputRoot -Parent $allowedOutputRoot))) {
    throw "OutputRoot must be the writable client root or one of its descendants: $allowedOutputRoot"
}
Assert-NoReparsePoints -Path $allowedOutputRoot
[IO.Directory]::CreateDirectory($allowedOutputRoot) | Out-Null
[IO.Directory]::CreateDirectory($resolvedOutputRoot) | Out-Null
Assert-NoReparsePoints -Path $resolvedOutputRoot
Assert-NoReparsePointsBelow -Root $resolvedOutputRoot

if ($Configuration -eq "Optimized" -and ($Architecture -eq "x64" -or $Architecture -eq "All")) {
    throw "Optimized|x64 is not defined by the three legacy Direct3D 9 dependency projects. Select Release/Debug, or build Optimized x86 only."
}

$platforms = switch ($Architecture) {
    "x86" { @("Win32") }
    "x64" { @("x64") }
    default { @("Win32", "x64") }
}

$suffix = @{
    Release   = "r"
    Optimized = "o"
    Debug     = "d"
}[$Configuration]

foreach ($platform in $platforms) {
    Assert-Inputs -RepoRoot $repoRoot -Platform $platform -SelectedAudioBackend $AudioBackend

    $artifacts = Get-ExpectedArtifacts `
        -Platform $platform `
        -SelectedConfiguration $Configuration `
        -SelectedRenderer $Renderer `
        -SelectedAudioBackend $AudioBackend `
        -Suffix $suffix `
        -ExternalOutputRoot $resolvedOutputRoot
    foreach ($artifact in $artifacts) {
        $artifactPath = [IO.Path]::GetFullPath($artifact)
        if (-not (Test-PathWithin -Path $artifactPath -Parent $resolvedOutputRoot)) {
            throw "Expected artifact escaped OutputRoot: $artifactPath"
        }
        Assert-NoReparsePoints -Path ([IO.Path]::GetDirectoryName($artifactPath))
        if (Test-Path -LiteralPath $artifactPath -PathType Leaf) {
            [IO.File]::Delete($artifactPath)
        }
    }

    $arguments = @(
        $solution,
        "/t:SwgClient",
        "/p:Configuration=$Configuration",
        "/p:Platform=$platform",
        "/p:PlatformToolset=$PlatformToolset",
        "/p:SwgAudioBackend=$AudioBackend",
        "/nr:false",
        "/v:minimal"
    )
    if ($MaxCpuCount -gt 0) {
        $arguments += "/m:$MaxCpuCount"
    }
    else {
        $arguments += "/m"
    }

    if ($resolvedOutputRoot) {
        $arguments += "/p:SwgExternalOutputRoot=$resolvedOutputRoot"
    }

    Write-Host "MSBuild: $msbuild"
    Write-Host "Solution: $solution"
    Write-Host "Build: $Configuration|$platform; verify-renderers=$Renderer (the solution builds all declared renderer dependencies); audio=$AudioBackend; toolset=$PlatformToolset"

    $buildLogRoot = if ($resolvedOutputRoot) {
        $path = Join-Path $resolvedOutputRoot "logs"
        New-Item -ItemType Directory -Force -Path $path | Out-Null
        $path
    }
    else {
        [IO.Path]::GetTempPath()
    }
    $buildLog = Join-Path $buildLogRoot ("swg-build-{0}-{1}-{2}-{3}.log" -f $Configuration, $platform, $AudioBackend, $PID)
    & $msbuild @arguments | Tee-Object -FilePath $buildLog
    if ($LASTEXITCODE -ne 0) {
        throw "The $Configuration|$platform client build failed with exit code $LASTEXITCODE."
    }

    # SwgClient links with ForceFileOutput (/FORCE), which downgrades LNK2019/
    # LNK2001 unresolved-external errors to warnings and still emits a binary --
    # so MSBuild exit 0 is NOT a clean link. A binary linked this way boots into
    # undefined behavior at the first call through a stubbed symbol. Gate: the
    # build log must contain ZERO unresolved external references.
    $unresolved = @(Select-String -Path $buildLog -Pattern 'unresolved external symbol|LNK(?:2001|2019|1120)')
    if ($unresolved.Count -gt 0) {
        $unresolved | Select-Object -First 10 | ForEach-Object { Write-Host $_.Line }
        throw "The $Configuration|$platform link left $($unresolved.Count) unresolved external symbol reference(s); /FORCE emitted a binary anyway -- treating the build as FAILED. Full log: $buildLog"
    }

    $expectedMachine = if ($platform -eq "x64") { 0x8664 } else { 0x014c }
    foreach ($relativePath in $artifacts) {
        $path = if ([IO.Path]::IsPathRooted($relativePath)) {
            $relativePath
        }
        else {
            Join-Path $repoRoot $relativePath
        }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Expected build artifact is missing: $path"
        }

        $machine = Get-PeMachine -Path $path
        if ($machine -ne $expectedMachine) {
            throw ("Expected PE machine 0x{0:x4}, found 0x{1:x4}: {2}" -f $expectedMachine, $machine, $path)
        }

        $item = Get-Item -LiteralPath $path
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        Write-Host ("{0,-5} {1,10:N0} bytes  {2}  {3}" -f $platform, $item.Length, $hash, $path)
    }
}

if ($platforms -contains "x64") {
    # The 64-bit client talks to the original 32-bit SWGTCG.dll through this
    # child process. Build it as Win32 into the same external artifact tree so
    # staging never needs to discover or trust an in-source binary.
    $hostProject = Join-Path $repoRoot "src\external\3rd\application\TcgCompatibilityHost\TcgCompatibilityHost.vcxproj"
    if (-not (Test-Path -LiteralPath $hostProject -PathType Leaf)) {
        throw "The TCG compatibility-host project is missing: $hostProject"
    }

    $hostArtifactRoot = Join-Path $resolvedOutputRoot "artifacts\Win32\$Configuration\$AudioBackend"
    $hostArtifact = Join-Path $hostArtifactRoot "TcgCompatibilityHost.exe"
    if (-not (Test-PathWithin -Path $hostArtifact -Parent $resolvedOutputRoot)) {
        throw "Expected TCG compatibility-host artifact escaped OutputRoot: $hostArtifact"
    }
    [IO.Directory]::CreateDirectory($hostArtifactRoot) | Out-Null
    Assert-NoReparsePoints -Path $hostArtifactRoot
    if (Test-Path -LiteralPath $hostArtifact -PathType Leaf) {
        [IO.File]::Delete($hostArtifact)
    }

    $hostArguments = @(
        $hostProject,
        "/t:Build",
        "/p:Configuration=$Configuration",
        "/p:Platform=Win32",
        "/p:PlatformToolset=$PlatformToolset",
        "/p:SwgExternalOutputRoot=$resolvedOutputRoot",
        "/p:SwgAudioBackend=$AudioBackend",
        "/p:OutDir=$hostArtifactRoot\",
        "/nr:false",
        "/v:minimal"
    )
    if ($MaxCpuCount -gt 0) {
        $hostArguments += "/m:$MaxCpuCount"
    }
    else {
        $hostArguments += "/m"
    }

    $hostBuildLogRoot = Join-Path $resolvedOutputRoot "logs"
    [IO.Directory]::CreateDirectory($hostBuildLogRoot) | Out-Null
    $hostBuildLog = Join-Path $hostBuildLogRoot ("tcg-compatibility-host-build-{0}-Win32-{1}-{2}.log" -f $Configuration, $AudioBackend, $PID)
    Write-Host "Build: TcgCompatibilityHost $Configuration|Win32; audio=$AudioBackend; toolset=$PlatformToolset"
    & $msbuild @hostArguments | Tee-Object -FilePath $hostBuildLog
    if ($LASTEXITCODE -ne 0) {
        throw "The $Configuration|Win32 TCG compatibility-host build failed with exit code $LASTEXITCODE. Full log: $hostBuildLog"
    }

    $hostUnresolved = @(Select-String -Path $hostBuildLog -Pattern 'unresolved external symbol|LNK(?:2001|2019|1120)')
    if ($hostUnresolved.Count -gt 0) {
        $hostUnresolved | Select-Object -First 10 | ForEach-Object { Write-Host $_.Line }
        throw "The $Configuration|Win32 TCG compatibility-host link left $($hostUnresolved.Count) unresolved external symbol reference(s). Full log: $hostBuildLog"
    }
    if (-not (Test-Path -LiteralPath $hostArtifact -PathType Leaf)) {
        throw "Expected TCG compatibility-host build artifact is missing: $hostArtifact"
    }

    $hostMachine = Get-PeMachine -Path $hostArtifact
    if ($hostMachine -ne 0x014c) {
        throw ("Expected compatibility-host PE machine 0x014c, found 0x{0:x4}: {1}" -f $hostMachine, $hostArtifact)
    }
    $hostItem = Get-Item -LiteralPath $hostArtifact
    $hostHash = (Get-FileHash -LiteralPath $hostArtifact -Algorithm SHA256).Hash
    Write-Host ("{0,-5} {1,10:N0} bytes  {2}  {3}" -f "Win32", $hostItem.Length, $hostHash, $hostArtifact)

    # The x64 libMozilla proxy renders through the original 32-bit embedded
    # Mozilla implementation hosted out of process. Keep the broker executable
    # in the same trusted external artifact tree as the TCG compatibility host.
    $browserHostProject = Join-Path $repoRoot "src\external\3rd\application\BrowserCompatibilityHost\BrowserCompatibilityHost.vcxproj"
    if (-not (Test-Path -LiteralPath $browserHostProject -PathType Leaf)) {
        throw "The browser compatibility-host project is missing: $browserHostProject"
    }

    $browserHostArtifact = Join-Path $hostArtifactRoot "BrowserCompatibilityHost.exe"
    if (-not (Test-PathWithin -Path $browserHostArtifact -Parent $resolvedOutputRoot)) {
        throw "Expected browser compatibility-host artifact escaped OutputRoot: $browserHostArtifact"
    }
    Assert-NoReparsePoints -Path $hostArtifactRoot
    if (Test-Path -LiteralPath $browserHostArtifact -PathType Leaf) {
        [IO.File]::Delete($browserHostArtifact)
    }

    $browserHostArguments = @(
        $browserHostProject,
        "/t:Build",
        "/p:Configuration=$Configuration",
        "/p:Platform=Win32",
        "/p:PlatformToolset=$PlatformToolset",
        "/p:SwgExternalOutputRoot=$resolvedOutputRoot",
        "/p:SwgAudioBackend=$AudioBackend",
        "/p:OutDir=$hostArtifactRoot\",
        "/nr:false",
        "/v:minimal"
    )
    if ($MaxCpuCount -gt 0) {
        $browserHostArguments += "/m:$MaxCpuCount"
    }
    else {
        $browserHostArguments += "/m"
    }

    $browserHostBuildLog = Join-Path $hostBuildLogRoot ("browser-compatibility-host-build-{0}-Win32-{1}-{2}.log" -f $Configuration, $AudioBackend, $PID)
    Write-Host "Build: BrowserCompatibilityHost $Configuration|Win32; audio=$AudioBackend; toolset=$PlatformToolset"
    & $msbuild @browserHostArguments | Tee-Object -FilePath $browserHostBuildLog
    if ($LASTEXITCODE -ne 0) {
        throw "The $Configuration|Win32 browser compatibility-host build failed with exit code $LASTEXITCODE. Full log: $browserHostBuildLog"
    }

    $browserHostUnresolved = @(Select-String -Path $browserHostBuildLog -Pattern 'unresolved external symbol|LNK(?:2001|2019|1120)')
    if ($browserHostUnresolved.Count -gt 0) {
        $browserHostUnresolved | Select-Object -First 10 | ForEach-Object { Write-Host $_.Line }
        throw "The $Configuration|Win32 browser compatibility-host link left $($browserHostUnresolved.Count) unresolved external symbol reference(s). Full log: $browserHostBuildLog"
    }
    if (-not (Test-Path -LiteralPath $browserHostArtifact -PathType Leaf)) {
        throw "Expected browser compatibility-host build artifact is missing: $browserHostArtifact"
    }

    $browserHostMachine = Get-PeMachine -Path $browserHostArtifact
    if ($browserHostMachine -ne 0x014c) {
        throw ("Expected browser compatibility-host PE machine 0x014c, found 0x{0:x4}: {1}" -f $browserHostMachine, $browserHostArtifact)
    }
    $browserHostItem = Get-Item -LiteralPath $browserHostArtifact
    $browserHostHash = (Get-FileHash -LiteralPath $browserHostArtifact -Algorithm SHA256).Hash
    Write-Host ("{0,-5} {1,10:N0} bytes  {2}  {3}" -f "Win32", $browserHostItem.Length, $browserHostHash, $browserHostArtifact)
}
