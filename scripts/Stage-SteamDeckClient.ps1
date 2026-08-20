<#
.SYNOPSIS
Creates a new Steam Deck runtime from a client asset tree and Release x64 build.

.EXAMPLE
.\scripts\Stage-SteamDeckClient.ps1 -ClientAssetRoot "E:\SWG\SWGSource\SWGSource Client v3.0" -BuildOutputRoot ".\src\build\win32\x64\Release" -Destination "E:\SWG\SWGSource\SteamDeck\client"
#>

[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$ClientAssetRoot,

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$BuildOutputRoot,

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$Destination
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-ExistingDirectory {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "$Description does not exist or is not a directory: $Path"
    }

    $resolved = Resolve-Path -LiteralPath $Path
    if ($resolved.Provider.Name -ne "FileSystem") {
        throw "$Description is not a filesystem path: $Path"
    }

    return [IO.Path]::GetFullPath($resolved.ProviderPath)
}

function Resolve-DestinationDirectory {
    param([Parameter(Mandatory)][string]$Path)

    $candidate = [IO.Path]::GetFullPath($Path)
    if (Test-Path -LiteralPath $candidate) {
        return Resolve-ExistingDirectory -Path $candidate -Description "Destination"
    }

    $leaf = Split-Path -Leaf $candidate
    $parent = Split-Path -Parent $candidate
    if (-not $leaf -or -not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw "Destination must be below an existing directory: $candidate"
    }

    $resolvedParent = Resolve-ExistingDirectory -Path $parent -Description "Destination parent"
    return [IO.Path]::GetFullPath((Join-Path $resolvedParent $leaf))
}

function Test-PathAtOrBelow {
    param(
        [Parameter(Mandatory)][string]$Candidate,
        [Parameter(Mandatory)][string]$Parent
    )

    $candidatePath = [IO.Path]::GetFullPath($Candidate).TrimEnd('\', '/')
    $parentPath = [IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    if ([string]::Equals($candidatePath, $parentPath, [StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }

    return $candidatePath.StartsWith($parentPath + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Get-PeMachine {
    param([Parameter(Mandatory)][string]$Path)

    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5a4d) {
            throw "Not a PE file: $Path"
        }

        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or $peOffset -gt ($stream.Length - 6)) {
            throw "Invalid PE header offset: $Path"
        }

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

function Assert-X64Pe {
    param([Parameter(Mandatory)][string]$Path)

    $machine = Get-PeMachine -Path $Path
    if ($machine -ne 0x8664) {
        throw ("Refusing to ship non-x64 PE machine 0x{0:x4}: {1}" -f $machine, $Path)
    }
}

function Get-BuildArtifactDirectory {
    param([Parameter(Mandatory)][string]$Root)

    $required = @("SwgClient_r.exe", "gl11_r.dll", "DllExport.dll")
    foreach ($candidate in @($Root, (Join-Path $Root "Release")) | Select-Object -Unique) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
            continue
        }
        $missing = @($required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $candidate $_) -PathType Leaf) })
        if ($missing.Count -eq 0) {
            return Resolve-ExistingDirectory -Path $candidate -Description "Release x64 build output"
        }
    }

    throw "BuildOutputRoot must contain SwgClient_r.exe, gl11_r.dll, and DllExport.dll directly or below Release: $Root"
}

function Get-NewestVcRuntimeDirectory {
    $programFiles = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles)
    $programFilesX86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
    $redistRoots = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)

    if ($env:VCToolsRedistDir -and (Test-Path -LiteralPath $env:VCToolsRedistDir -PathType Container)) {
        [void]$redistRoots.Add([IO.Path]::GetFullPath($env:VCToolsRedistDir))
    }

    $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installations = @(& $vswhere "-all" "-products" "*" "-property" "installationPath" 2>$null)
        if ($LASTEXITCODE -eq 0) {
            foreach ($installation in $installations | Where-Object { $_ }) {
                $redistRoot = Join-Path ([string]$installation) "VC\Redist\MSVC"
                if (Test-Path -LiteralPath $redistRoot -PathType Container) {
                    [void]$redistRoots.Add([IO.Path]::GetFullPath($redistRoot))
                }
            }
        }
    }

    foreach ($pattern in @(
        (Join-Path $programFiles "Microsoft Visual Studio\*\*\VC\Redist\MSVC"),
        (Join-Path $programFilesX86 "Microsoft Visual Studio\*\*\VC\Redist\MSVC")
    )) {
        foreach ($item in @(Get-Item -Path $pattern -ErrorAction SilentlyContinue)) {
            if ($item.PSIsContainer) {
                [void]$redistRoots.Add($item.FullName)
            }
        }
    }

    $candidates = @()
    foreach ($redistRoot in $redistRoots) {
        $versionRoots = @()
        if ((Split-Path -Leaf $redistRoot) -match '^\d+\.\d+\.\d+(?:\.\d+)?$') {
            $versionRoots += Get-Item -LiteralPath $redistRoot
        }
        else {
            $versionRoots += @(Get-ChildItem -LiteralPath $redistRoot -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+(?:\.\d+)?$' })
        }

        foreach ($versionRoot in $versionRoots) {
            $version = [version]"0.0"
            [void][version]::TryParse($versionRoot.Name, [ref]$version)
            $x64Root = Join-Path $versionRoot.FullName "x64"
            if (-not (Test-Path -LiteralPath $x64Root -PathType Container)) {
                continue
            }

            foreach ($runtimeDirectory in @(Get-ChildItem -LiteralPath $x64Root -Directory -Filter "Microsoft.VC*.CRT")) {
                $family = 0
                if ($runtimeDirectory.Name -match '^Microsoft\.VC(?<family>\d+)\.CRT$') {
                    $family = [int]$Matches.family
                }
                if (Test-Path -LiteralPath (Join-Path $runtimeDirectory.FullName "vcruntime140.dll") -PathType Leaf) {
                    $candidates += [pscustomobject]@{
                        Path = $runtimeDirectory.FullName
                        Version = $version
                        Family = $family
                        Name = $runtimeDirectory.Name
                    }
                }
            }
        }
    }

    if ($candidates.Count -eq 0) {
        throw "Could not locate an x64 Microsoft.VC*.CRT directory. Install the Visual Studio x64 C++ runtime tools."
    }

    return $candidates |
        Sort-Object @{ Expression = { $_.Version }; Descending = $true },
                    @{ Expression = { $_.Family }; Descending = $true },
                    @{ Expression = { $_.Path }; Descending = $true } |
        Select-Object -First 1
}

function Get-CfgAssignmentKeys {
    param([Parameter(Mandatory)][string]$Path)

    $results = [Collections.Generic.List[object]]::new()
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $Path) {
        ++$lineNumber
        $trimmedLine = $line.TrimStart()
        if (-not $trimmedLine -or
            $trimmedLine[0] -eq '#' -or
            $trimmedLine[0] -eq ';' -or
            $trimmedLine[0] -eq '[' -or
            $trimmedLine.StartsWith(".include ", [StringComparison]::Ordinal)) {
            continue
        }

        $position = $line.Length - $trimmedLine.Length
        while ($position -lt $line.Length) {
            while ($position -lt $line.Length -and [char]::IsWhiteSpace($line[$position])) {
                ++$position
            }
            if ($position -ge $line.Length -or $line[$position] -eq '#' -or $line[$position] -eq ';') {
                break
            }

            $equalsIndex = $line.IndexOf('=', $position)
            if ($equalsIndex -lt 0) {
                break
            }
            $results.Add([pscustomobject]@{
                Key = $line.Substring($position, $equalsIndex - $position).Trim()
                Line = $lineNumber
            })
            $position = $equalsIndex + 1

            $foundNextKey = $false
            while ($position -lt $line.Length) {
                while ($position -lt $line.Length -and [char]::IsWhiteSpace($line[$position])) {
                    ++$position
                }
                if ($position -ge $line.Length -or $line[$position] -eq '#' -or $line[$position] -eq ';') {
                    $position = $line.Length
                    break
                }

                if ($line[$position] -eq [char]34) {
                    ++$position
                    while ($position -lt $line.Length -and $line[$position] -ne [char]34) {
                        ++$position
                    }
                    if ($position -lt $line.Length) {
                        ++$position
                    }
                }
                else {
                    while ($position -lt $line.Length -and
                        -not [char]::IsWhiteSpace($line[$position]) -and
                        $line[$position] -ne '#' -and
                        $line[$position] -ne ';' -and
                        $line[$position] -ne '&') {
                        ++$position
                    }
                }

                while ($position -lt $line.Length -and [char]::IsWhiteSpace($line[$position])) {
                    ++$position
                }
                if ($position -lt $line.Length -and $line[$position] -eq '&') {
                    ++$position
                    $foundNextKey = $true
                    break
                }
                if ($position -lt $line.Length -and ($line[$position] -eq '#' -or $line[$position] -eq ';')) {
                    $position = $line.Length
                    break
                }
            }

            if (-not $foundNextKey) {
                break
            }
        }
    }

    return $results
}

function Assert-NoCredentialKeys {
    param([Parameter(Mandatory)][string]$Path)

    $sensitiveKeys = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($key in @(
        "loginClientPassword", "loginClientID", "launcherAvatarName",
        "accountName", "userName", "authToken", "password"
    )) {
        [void]$sensitiveKeys.Add($key)
    }

    foreach ($assignment in @(Get-CfgAssignmentKeys -Path $Path)) {
        if ($sensitiveKeys.Contains($assignment.Key)) {
            throw "A whitelisted configuration contains credential or account key '$($assignment.Key)' at ${Path}:$($assignment.Line) and will not be copied."
        }
    }
}

function Get-CfgRootReferences {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][ValidateSet("Include", "Search")][string]$ReferenceType
    )

    $references = [Collections.Generic.List[object]]::new()
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $Path) {
        ++$lineNumber
        if ($line -match '^\s*(?:#|;|$)') {
            continue
        }

        $match = $null
        # Runtime ConfigFile recognizes exactly `.include "file.cfg"` after
        # trimming leading whitespace. Mirror that syntax so a form ignored by
        # the client cannot satisfy staging's required-include checks.
        if ($ReferenceType -eq "Include" -and
            $line -match '^\s*\.include "(?<reference>[^"]+)"') {
            $match = [pscustomobject]@{ Kind = "include"; Value = $Matches.reference }
        }
        elseif ($ReferenceType -eq "Search" -and
            $line -match '^\s*(?<kind>searchTree|searchTOC)(?:_[^=\s]*)?\s*=\s*(?<reference>"[^"]+"|''[^'']+''|[^#;\s]+)') {
            $match = [pscustomobject]@{ Kind = $Matches.kind; Value = $Matches.reference }
        }

        if ($null -eq $match) {
            continue
        }

        $value = $match.Value.Trim()
        if ($value.Length -ge 2 -and
            (($value[0] -eq [char]34 -and $value[$value.Length - 1] -eq [char]34) -or
             ($value[0] -eq [char]39 -and $value[$value.Length - 1] -eq [char]39))) {
            $value = $value.Substring(1, $value.Length - 2)
        }

        $references.Add([pscustomobject]@{
            Kind = $match.Kind
            Value = $value
            Source = $Path
            Line = $lineNumber
        })
    }

    return $references
}

function Assert-RootFileName {
    param(
        [Parameter(Mandatory)][string]$Reference,
        [Parameter(Mandatory)][string]$Description
    )

    if ([string]::IsNullOrWhiteSpace($Reference) -or
        [IO.Path]::IsPathRooted($Reference) -or
        $Reference -match '[\\/:]' -or
        $Reference -in @(".", "..")) {
        throw "$Description must reference a file directly in the runtime root: $Reference"
    }
}

function Get-Sha256Text {
    param([Parameter(Mandatory)][string]$Text)

    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes($Text)
        return [BitConverter]::ToString($sha256.ComputeHash($bytes)).Replace("-", "")
    }
    finally {
        $sha256.Dispose()
    }
}

$repoRoot = Resolve-ExistingDirectory -Path (Join-Path $PSScriptRoot "..") -Description "Repository root"
$assetRoot = Resolve-ExistingDirectory -Path $ClientAssetRoot -Description "ClientAssetRoot"
$requestedBuildRoot = Resolve-ExistingDirectory -Path $BuildOutputRoot -Description "BuildOutputRoot"
$buildArtifactRoot = Get-BuildArtifactDirectory -Root $requestedBuildRoot
$destinationPath = Resolve-DestinationDirectory -Path $Destination

foreach ($source in @(
    [pscustomobject]@{ Name = "repository"; Path = $repoRoot },
    [pscustomobject]@{ Name = "client asset root"; Path = $assetRoot },
    [pscustomobject]@{ Name = "build output root"; Path = $requestedBuildRoot }
)) {
    if (Test-PathAtOrBelow -Candidate $destinationPath -Parent $source.Path) {
        throw "Destination must not be the $($source.Name) or a directory below it: $destinationPath"
    }
    if (Test-PathAtOrBelow -Candidate $source.Path -Parent $destinationPath) {
        throw "Destination must not contain the $($source.Name): $destinationPath"
    }
}

if (Test-Path -LiteralPath $destinationPath -PathType Container) {
    if (@(Get-ChildItem -LiteralPath $destinationPath -Force).Count -ne 0) {
        throw "Destination must be empty. Existing content is never overwritten or deleted: $destinationPath"
    }
}

$assetDirectories = @(
    "appearance", "clientdata", "customization", "datatables", "misc",
    "object", "shader", "string", "texture", "ui"
)
foreach ($name in $assetDirectories) {
    if (-not (Test-Path -LiteralPath (Join-Path $assetRoot $name) -PathType Container)) {
        throw "Required runtime asset directory is missing: $(Join-Path $assetRoot $name)"
    }
}

$rootFiles = @("client.cfg", "defaultLights.iff", "live.cfg", "login.cfg", "preload.cfg", "version.txt")
foreach ($name in $rootFiles) {
    $path = Join-Path $assetRoot $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required root client file is missing: $path"
    }
    if ($name.EndsWith(".cfg", [StringComparison]::OrdinalIgnoreCase)) {
        Assert-NoCredentialKeys -Path $path
    }
}

$rootTreFiles = @(Get-ChildItem -LiteralPath $assetRoot -File -Filter "*.tre")
if ($rootTreFiles.Count -eq 0) {
    throw "ClientAssetRoot must contain at least one root TRE file: $assetRoot"
}
$rootTocFiles = @(Get-ChildItem -LiteralPath $assetRoot -File -Filter "*.toc")

$overlayRoot = Join-Path $repoRoot "deploy\steamdeck"
foreach ($name in @("options.cfg", "user.cfg", "README.txt")) {
    if (-not (Test-Path -LiteralPath (Join-Path $overlayRoot $name) -PathType Leaf)) {
        throw "Steam Deck overlay file is missing: $(Join-Path $overlayRoot $name)"
    }
}
Assert-NoCredentialKeys -Path (Join-Path $overlayRoot "user.cfg")

# Walk the active include graph before constructing the payload. ConfigFile
# supports nested includes, so validating client.cfg alone could otherwise omit
# a second-level root CFG. Deck-owned options/user files replace asset copies;
# all other active root CFGs come from the selected client data.
$configQueue = [Collections.Generic.Queue[object]]::new()
$configSources = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::OrdinalIgnoreCase)
$includeReferences = [Collections.Generic.List[object]]::new()
$includedCfgNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$clientCfgPath = Join-Path $assetRoot "client.cfg"
$configSources.Add("client.cfg", $clientCfgPath)
$configQueue.Enqueue([pscustomobject]@{
    Name = "client.cfg"
    Path = $clientCfgPath
    Ancestry = @("client.cfg")
})

while ($configQueue.Count -gt 0) {
    $configEntry = $configQueue.Dequeue()
    foreach ($reference in @(Get-CfgRootReferences -Path $configEntry.Path -ReferenceType Include)) {
        Assert-RootFileName -Reference $reference.Value -Description "$($reference.Source):$($reference.Line) .include"
        if ([IO.Path]::GetExtension($reference.Value) -ine ".cfg") {
            throw "$($reference.Source):$($reference.Line) .include must reference a root CFG file: $($reference.Value)"
        }

        $includeReferences.Add($reference)
        [void]$includedCfgNames.Add($reference.Value)
        if ($configEntry.Ancestry -icontains $reference.Value) {
            $cycle = (@($configEntry.Ancestry) + $reference.Value) -join " -> "
            throw "$($reference.Source):$($reference.Line) creates a recursive CFG include cycle: $cycle"
        }

        if (-not $configSources.ContainsKey($reference.Value)) {
            $configSource = if ($reference.Value -ieq "options.cfg" -or $reference.Value -ieq "user.cfg") {
                Join-Path $overlayRoot $reference.Value
            }
            else {
                Join-Path $assetRoot $reference.Value
            }
            if (-not (Test-Path -LiteralPath $configSource -PathType Leaf)) {
                throw "$($reference.Source):$($reference.Line) references a missing root CFG file: $configSource"
            }
            Assert-NoCredentialKeys -Path $configSource

            $configSources.Add($reference.Value, $configSource)
            if ($reference.Value -ine "options.cfg" -and
                $reference.Value -ine "user.cfg" -and
                $reference.Value -notin $rootFiles) {
                $rootFiles += $reference.Value
            }
        }

        $configQueue.Enqueue([pscustomobject]@{
            Name = $reference.Value
            Path = $configSources[$reference.Value]
            Ancestry = @($configEntry.Ancestry) + $reference.Value
        })
    }
}

foreach ($requiredInclude in @("options.cfg", "user.cfg")) {
    if (-not $includedCfgNames.Contains($requiredInclude)) {
        throw "The client.cfg include graph must contain an active .include for $requiredInclude"
    }
}

$licenseNames = @(
    "THIRD-PARTY-NOTICES.txt", "SDL3.txt", "libxml2.txt",
    "libiconv.txt", "zlib.txt", "JUCE.md"
)
$licenseRoot = Join-Path $overlayRoot "licenses"
foreach ($name in $licenseNames) {
    if (-not (Test-Path -LiteralPath (Join-Path $licenseRoot $name) -PathType Leaf)) {
        throw "Required third-party notice is missing: $(Join-Path $licenseRoot $name)"
    }
}

$vcRuntime = Get-NewestVcRuntimeDirectory
$vcRuntimeFiles = @(Get-ChildItem -LiteralPath $vcRuntime.Path -File -Filter "*.dll")
if ($vcRuntimeFiles.Count -eq 0) {
    throw "Selected Visual C++ runtime contains no DLLs: $($vcRuntime.Path)"
}

$copyPlan = [Collections.Generic.List[object]]::new()
$targetIndex = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
function Add-CopyPlanItem {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$RelativePath,
        [Parameter(Mandatory)][string]$Category
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Copy source is missing: $Source"
    }
    if ([IO.Path]::IsPathRooted($RelativePath) -or @($RelativePath -split '[\\/]' | Where-Object { $_ -eq ".." }).Count -gt 0) {
        throw "Copy plan path must stay below the destination: $RelativePath"
    }
    if (-not $targetIndex.Add($RelativePath)) {
        throw "Two source files map to the same destination path: $RelativePath"
    }

    $copyPlan.Add([pscustomobject]@{
        Source = [IO.Path]::GetFullPath($Source)
        RelativePath = $RelativePath
        Category = $Category
    })
}

$excludedDirectoryPattern = '(?i)(^|[\\/])(?:\.git|PortableGit|profiles|logs|screenshots|backups|\.codex-backups|\.x64-backups)([\\/]|$)'
$excludedAssetFileCount = 0
foreach ($name in $assetDirectories) {
    $sourceDirectory = Join-Path $assetRoot $name
    foreach ($file in Get-ChildItem -LiteralPath $sourceDirectory -Recurse -File -Force) {
        $relativeInDirectory = $file.FullName.Substring($sourceDirectory.Length).TrimStart('\', '/')
        $relativePath = Join-Path $name $relativeInDirectory
        if ($relativePath -match $excludedDirectoryPattern -or $file.Extension -ieq ".exe" -or $file.Extension -ieq ".dll") {
            ++$excludedAssetFileCount
            continue
        }
        Add-CopyPlanItem -Source $file.FullName -RelativePath $relativePath -Category "asset-loose"
    }
}

foreach ($file in $rootTreFiles) {
    Add-CopyPlanItem -Source $file.FullName -RelativePath $file.Name -Category "asset-root-tre"
}
foreach ($file in $rootTocFiles) {
    Add-CopyPlanItem -Source $file.FullName -RelativePath $file.Name -Category "asset-root-toc"
}
foreach ($name in $rootFiles) {
    Add-CopyPlanItem -Source (Join-Path $assetRoot $name) -RelativePath $name -Category "asset-root-config"
}

Add-CopyPlanItem -Source (Join-Path $buildArtifactRoot "SwgClient_r.exe") -RelativePath "SwgClient_r.exe" -Category "client-binary"
Add-CopyPlanItem -Source (Join-Path $buildArtifactRoot "gl11_r.dll") -RelativePath "gl11_r.dll" -Category "dx11-renderer"
Add-CopyPlanItem -Source (Join-Path $buildArtifactRoot "DllExport.dll") -RelativePath "DllExport.dll" -Category "client-runtime"
foreach ($name in @("SDL3.dll", "libxml2.dll", "iconv-2.dll", "z.dll")) {
    Add-CopyPlanItem -Source (Join-Path $repoRoot "deps\x64\bin\$name") -RelativePath $name -Category "client-runtime"
}
foreach ($file in $vcRuntimeFiles) {
    Add-CopyPlanItem -Source $file.FullName -RelativePath $file.Name -Category "vc-runtime"
}
Add-CopyPlanItem -Source (Join-Path $overlayRoot "options.cfg") -RelativePath "options.cfg" -Category "steamdeck-config"
Add-CopyPlanItem -Source (Join-Path $overlayRoot "user.cfg") -RelativePath "user.cfg" -Category "steamdeck-config"
Add-CopyPlanItem -Source (Join-Path $overlayRoot "README.txt") -RelativePath "README.txt" -Category "steamdeck-readme"
foreach ($name in $licenseNames) {
    Add-CopyPlanItem -Source (Join-Path $licenseRoot $name) -RelativePath (Join-Path "licenses" $name) -Category "third-party-license"
}

foreach ($reference in $includeReferences) {
    if (-not $targetIndex.Contains($reference.Value)) {
        throw "$($reference.Source):$($reference.Line) references a CFG that is not included in the payload: $($reference.Value)"
    }
    $includedCfgEntry = $copyPlan | Where-Object { $_.RelativePath -ieq $reference.Value } | Select-Object -First 1
    if ($null -eq $includedCfgEntry -or -not (Test-Path -LiteralPath $includedCfgEntry.Source -PathType Leaf)) {
        throw "$($reference.Source):$($reference.Line) references a missing root CFG file: $($reference.Value)"
    }
}

foreach ($cfgEntry in @($copyPlan | Where-Object { [IO.Path]::GetExtension($_.RelativePath) -ieq ".cfg" })) {
    foreach ($reference in @(Get-CfgRootReferences -Path $cfgEntry.Source -ReferenceType Search)) {
        Assert-RootFileName -Reference $reference.Value -Description "$($reference.Source):$($reference.Line) $($reference.Kind)"
        $expectedExtension = if ($reference.Kind -ieq "searchTree") { ".tre" } else { ".toc" }
        if ([IO.Path]::GetExtension($reference.Value) -ine $expectedExtension) {
            throw "$($reference.Source):$($reference.Line) $($reference.Kind) must reference a root $expectedExtension file: $($reference.Value)"
        }

        $assetReference = Join-Path $assetRoot $reference.Value
        if (-not (Test-Path -LiteralPath $assetReference -PathType Leaf)) {
            throw "$($reference.Source):$($reference.Line) references a missing root asset: $assetReference"
        }
        if (-not $targetIndex.Contains($reference.Value)) {
            throw "$($reference.Source):$($reference.Line) references an asset that is not included in the payload: $($reference.Value)"
        }
    }
}

$plannedPeFiles = @($copyPlan | Where-Object { $_.RelativePath -match '(?i)\.(?:exe|dll)$' })
foreach ($entry in $plannedPeFiles) {
    Assert-X64Pe -Path $entry.Source
}

$gitCommit = (& git -C $repoRoot rev-parse HEAD 2>$null) -join ""
if ($LASTEXITCODE -ne 0 -or -not $gitCommit) {
    throw "Could not determine source Git commit."
}
$gitBranch = (& git -C $repoRoot branch --show-current 2>$null) -join ""
if ($LASTEXITCODE -ne 0) {
    throw "Could not determine source Git branch."
}
$workingTreeDirty = @(& git -C $repoRoot status --porcelain --untracked-files=all).Count -gt 0
$plannedBytes = [int64](($copyPlan | ForEach-Object { (Get-Item -LiteralPath $_.Source).Length } | Measure-Object -Sum).Sum)

Write-Host ("Steam Deck payload: {0:N0} files, {1:N2} GiB" -f $copyPlan.Count, ($plannedBytes / 1GB))
Write-Host "Asset source: $assetRoot"
Write-Host "Build output: $buildArtifactRoot"
Write-Host "VC runtime: $($vcRuntime.Path)"
Write-Host "Destination: $destinationPath"
if (-not $PSCmdlet.ShouldProcess($destinationPath, "create a new Steam Deck client runtime")) {
    return
}

if (-not (Test-Path -LiteralPath $destinationPath)) {
    New-Item -ItemType Directory -Path $destinationPath | Out-Null
}
elseif (@(Get-ChildItem -LiteralPath $destinationPath -Force).Count -ne 0) {
    throw "Destination became nonempty after validation: $destinationPath"
}

$copyIndex = 0
foreach ($entry in $copyPlan) {
    ++$copyIndex
    Write-Progress -Activity "Creating Steam Deck runtime" -Status $entry.RelativePath -PercentComplete ([int](($copyIndex * 100) / $copyPlan.Count))
    $target = Join-Path $destinationPath $entry.RelativePath
    $targetDirectory = Split-Path -Parent $target
    if (-not (Test-Path -LiteralPath $targetDirectory -PathType Container)) {
        New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
    }
    Copy-Item -LiteralPath $entry.Source -Destination $target
}
Write-Progress -Activity "Creating Steam Deck runtime" -Completed

$shippedPeFiles = @(Get-ChildItem -LiteralPath $destinationPath -Recurse -File | Where-Object { $_.Extension -ieq ".exe" -or $_.Extension -ieq ".dll" })
foreach ($file in $shippedPeFiles) {
    Assert-X64Pe -Path $file.FullName
}
if ($shippedPeFiles.Count -ne $plannedPeFiles.Count) {
    throw "Destination PE count differs from approved plan ($($shippedPeFiles.Count) != $($plannedPeFiles.Count))."
}

$manifestFiles = [Collections.Generic.List[object]]::new()
$hashIndex = 0
foreach ($entry in $copyPlan | Sort-Object RelativePath) {
    ++$hashIndex
    Write-Progress -Activity "Hashing Steam Deck runtime" -Status $entry.RelativePath -PercentComplete ([int](($hashIndex * 100) / $copyPlan.Count))
    $path = Join-Path $destinationPath $entry.RelativePath
    $item = Get-Item -LiteralPath $path
    # Windows PowerShell 5's Measure-Object does not expose OrderedDictionary
    # keys as properties. Wrap each ordered entry as a PSCustomObject so the
    # later byte aggregation and JSON serialization use real properties.
    $manifestFiles.Add([pscustomobject][ordered]@{
        path = $entry.RelativePath.Replace('\', '/')
        category = $entry.Category
        bytes = [int64]$item.Length
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    })
}
Write-Progress -Activity "Hashing Steam Deck runtime" -Completed

$payloadDigestInput = @($manifestFiles | ForEach-Object { "$($_.sha256)  $($_.path)" }) -join [Environment]::NewLine
$payloadSha256 = Get-Sha256Text -Text ($payloadDigestInput + [Environment]::NewLine)
$categoryCounts = [ordered]@{}
foreach ($group in $copyPlan | Group-Object Category | Sort-Object Name) {
    $categoryCounts[$group.Name] = $group.Count
}
$payloadBytes = [int64](($manifestFiles | Measure-Object -Property bytes -Sum).Sum)

$manifest = [ordered]@{
    formatVersion = 1
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    profile = "Galaxies Reborn Steam Deck"
    sourceGit = [ordered]@{
        repository = $repoRoot
        commit = $gitCommit
        branch = $gitBranch
        workingTreeDirty = $workingTreeDirty
    }
    sources = [ordered]@{
        clientAssets = $assetRoot
        buildOutput = $buildArtifactRoot
        vcRuntime = $vcRuntime.Path
        vcRuntimeVersion = $vcRuntime.Version.ToString()
        vcRuntimeFamily = $vcRuntime.Name
    }
    destination = $destinationPath
    runtime = [ordered]@{
        configuration = "Release"
        architecture = "x64"
        executable = "SwgClient_r.exe"
        renderer = "Direct3D 11 (rasterMajor 11; Proton/DXVK)"
        display = [ordered]@{
            width = 1280
            height = 800
            mode = "borderless windowed"
            frameRateLimit = 60
        }
        input = "SDL 3.4.10 mapped-gamepad and multi-controller backend"
        audio = "JUCE 8.0.14 / Windows Audio (WASAPI)"
    }
    counts = [ordered]@{
        files = $copyPlan.Count
        directories = @(Get-ChildItem -LiteralPath $destinationPath -Recurse -Directory).Count
        bytes = $payloadBytes
        rootTreFiles = $rootTreFiles.Count
        rootTocFiles = $rootTocFiles.Count
        looseAssetFiles = @($copyPlan | Where-Object { $_.Category -eq "asset-loose" }).Count
        shippedPeFiles = $shippedPeFiles.Count
        excludedAssetFiles = $excludedAssetFileCount
        categories = $categoryCounts
    }
    payloadSha256 = $payloadSha256
    files = $manifestFiles
}

$manifestPath = Join-Path $destinationPath "steamdeck-runtime-manifest.json"
$json = $manifest | ConvertTo-Json -Depth 8
$utf8NoBom = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText($manifestPath, $json + [Environment]::NewLine, $utf8NoBom)
$null = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json

Write-Host ("Created Steam Deck runtime with {0:N0} payload files ({1:N2} GiB)." -f $copyPlan.Count, ($payloadBytes / 1GB))
Write-Host "Manifest: $manifestPath"
Write-Host "Payload SHA-256: $payloadSha256"
