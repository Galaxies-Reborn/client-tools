#requires -Version 5.1

[CmdletBinding()]
param(
    [string]$RuntimeRoot,

    [string]$DevKitRoot,

    [ValidateSet("Win32", "x64")]
    [string]$Architecture = "Win32",

    [ValidateSet("SplashDirect", "GameAction")]
    [string]$Trigger = "SplashDirect",

    [ValidateRange(30, 300)]
    [int]$TimeoutSeconds = 90,

    [switch]$KeepProcess,

    [switch]$GameActionBrowserProbe,

    [switch]$AutoLoginLocalServer,

    [ValidateRange(1, 2147483647)]
    [int]$LocalStationId
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

function Get-PeMachine {
    param([Parameter(Mandatory)][string]$Path)

    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    $reader = $null
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5a4d) {
            throw "File is not a PE image: $Path"
        }
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0x40 -or $peOffset -gt ($stream.Length - 6)) {
            throw "File has an invalid PE header offset: $Path"
        }
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "File has an invalid PE signature: $Path"
        }
        return $reader.ReadUInt16()
    }
    finally {
        if ($reader) {
            $reader.Dispose()
        }
        else {
            $stream.Dispose()
        }
    }
}

function Get-ReferenceManifest {
    param([Parameter(Mandatory)][string]$Root)

    $items = [Collections.Generic.List[object]]::new()
    $canonical = [Collections.Generic.List[string]]::new()
    [long]$totalBytes = 0
    $fileCount = 0
    $directoryCount = 0

    foreach ($item in @(Get-ChildItem -LiteralPath $Root -Recurse -Force | Sort-Object FullName)) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "The protected reference contains a reparse point, which this manifest intentionally refuses: $($item.FullName)"
        }

        $relativePath = $item.FullName.Substring($Root.Length).TrimStart([char[]]@(92, 47)).Replace("\", "/")
        if ($item.PSIsContainer) {
            ++$directoryCount
            $canonical.Add("D|$relativePath")
            $items.Add([pscustomobject]@{
                kind = "directory"
                path = $relativePath
                length = 0
                sha256 = $null
            })
        }
        else {
            ++$fileCount
            $totalBytes += $item.Length
            $hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $canonical.Add("F|$relativePath|$($item.Length)|$hash")
            $items.Add([pscustomobject]@{
                kind = "file"
                path = $relativePath
                length = [long]$item.Length
                sha256 = $hash
            })
        }
    }

    $canonicalText = [string]::Join([Environment]::NewLine, $canonical.ToArray())
    $bytes = [Text.Encoding]::UTF8.GetBytes($canonicalText)
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        $digest = [BitConverter]::ToString($sha256.ComputeHash($bytes)).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
        [Array]::Clear($bytes, 0, $bytes.Length)
    }

    return [pscustomobject]@{
        root = $Root
        digest = $digest
        file_count = $fileCount
        directory_count = $directoryCount
        total_bytes = $totalBytes
        items = $items.ToArray()
    }
}

function Assert-ReferenceManifestEqual {
    param(
        [Parameter(Mandatory)]$Before,
        [Parameter(Mandatory)]$After
    )

    if (-not $Before.digest.Equals($After.digest, [StringComparison]::Ordinal) -or
        $Before.file_count -ne $After.file_count -or
        $Before.directory_count -ne $After.directory_count -or
        $Before.total_bytes -ne $After.total_bytes) {
        throw "The protected _client\TradingCardGame reference changed during the test. The harness never writes that tree; investigate before continuing."
    }
}

function Get-LoadedWin32ProcessModule {
    param(
        [Parameter(Mandatory)][int]$ProcessId,
        [Parameter(Mandatory)][string]$ModuleName
    )

    if ($ModuleName -notmatch '^[A-Za-z0-9_.-]+$') {
        throw "Refusing an unsafe Win32 module name: $ModuleName"
    }

    # Windows PowerShell 5.1 running as a 64-bit process exposes only the main
    # image for a WOW64 child through Process.Modules. Ask the OS's 32-bit
    # PowerShell to enumerate that exact PID, and return only base64-encoded
    # module metadata so paths cannot be confused with protocol delimiters.
    $win32PowerShell = Join-Path $env:WINDIR "SysWOW64\WindowsPowerShell\v1.0\powershell.exe"
    if (-not (Test-Path -LiteralPath $win32PowerShell -PathType Leaf)) {
        throw "The Win32 PowerShell module-enumeration helper is missing: $win32PowerShell"
    }

    $helperScript = @'
$ErrorActionPreference = 'Stop'
$target = Get-Process -Id __PROCESS_ID__ -ErrorAction Stop
$matches = @($target.Modules | Where-Object { $_.ModuleName -ieq '__MODULE_NAME__' })
if ($matches.Count -eq 0) { exit 2 }
if ($matches.Count -ne 1) { exit 3 }
$encoding = [Text.Encoding]::UTF8
$name = [Convert]::ToBase64String($encoding.GetBytes([string]$matches[0].ModuleName))
$path = [Convert]::ToBase64String($encoding.GetBytes([string]$matches[0].FileName))
Write-Output ("{0}|{1}|{2}" -f $name, $path, [long]$matches[0].ModuleMemorySize)
'@
    $helperScript = $helperScript.Replace('__PROCESS_ID__', [string]$ProcessId).Replace('__MODULE_NAME__', $ModuleName)
    $helperBytes = [Text.Encoding]::Unicode.GetBytes($helperScript)
    try {
        $encodedCommand = [Convert]::ToBase64String($helperBytes)
    }
    finally {
        [Array]::Clear($helperBytes, 0, $helperBytes.Length)
        $helperScript = $null
    }

    $helperOutput = @(& $win32PowerShell -NoProfile -NonInteractive -OutputFormat Text -EncodedCommand $encodedCommand 2>$null)
    $helperExitCode = $LASTEXITCODE
    $encodedCommand = $null
    if ($helperExitCode -eq 2) {
        return $null
    }
    if ($helperExitCode -ne 0) {
        throw "Win32 module enumeration failed for PID $ProcessId (exit $helperExitCode)."
    }

    $record = @($helperOutput | Where-Object { $_ -match '^[A-Za-z0-9+/=]+\|[A-Za-z0-9+/=]+\|[0-9]+$' }) | Select-Object -Last 1
    if (-not $record) {
        throw "Win32 module enumeration returned no parseable record for PID $ProcessId."
    }
    $fields = $record -split '\|', 3
    return [pscustomobject]@{
        name = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($fields[0]))
        path = Get-NormalizedPath ([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($fields[1])))
        module_memory_size = [long]$fields[2]
    }
}

function Get-LoadedTcgModule {
    param(
        [Parameter(Mandatory)][int]$ProcessId,
        [Parameter(Mandatory)][string]$ExpectedPath
    )

    $module = Get-LoadedWin32ProcessModule -ProcessId $ProcessId -ModuleName "SWGTCG.dll"
    if (-not $module) {
        return $null
    }

    $actualPath = $module.path
    if (-not $actualPath.Equals($ExpectedPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Process PID $ProcessId loaded SWGTCG.dll from an unexpected path: $actualPath"
    }

    return [pscustomobject]@{
        name = $module.name
        path = $actualPath
        module_memory_size = [long]$module.module_memory_size
    }
}

function Get-ChildCompatibilityHost {
    param(
        [Parameter(Mandatory)][int]$ParentProcessId,
        [Parameter(Mandatory)][string]$ExpectedPath
    )

    $candidates = @(
        Get-CimInstance Win32_Process -Filter "ParentProcessId=$ParentProcessId" -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ieq "TcgCompatibilityHost.exe" }
    )
    if ($candidates.Count -eq 0) {
        return $null
    }
    if ($candidates.Count -ne 1) {
        throw "SwgClient PID $ParentProcessId owns more than one TcgCompatibilityHost.exe child."
    }
    if (-not $candidates[0].ExecutablePath) {
        throw "Windows did not expose the compatibility host executable path."
    }

    $actualPath = Get-NormalizedPath $candidates[0].ExecutablePath
    if (-not $actualPath.Equals($ExpectedPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "SwgClient launched a compatibility host from an unexpected path: $actualPath"
    }

    return [pscustomobject]@{
        pid = [int]$candidates[0].ProcessId
        parent_pid = [int]$candidates[0].ParentProcessId
        path = $actualPath
        command_line = [string]$candidates[0].CommandLine
    }
}

function Get-ChildBrowserCompatibilityHost {
    param(
        [Parameter(Mandatory)][int]$ParentProcessId,
        [Parameter(Mandatory)][string]$ExpectedPath
    )

    $candidates = @(
        Get-CimInstance Win32_Process -Filter "ParentProcessId=$ParentProcessId" -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ieq "BrowserCompatibilityHost.exe" }
    )
    if ($candidates.Count -eq 0) {
        return $null
    }
    if ($candidates.Count -ne 1) {
        throw "SwgClient PID $ParentProcessId owns more than one BrowserCompatibilityHost.exe child."
    }
    if (-not $candidates[0].ExecutablePath) {
        throw "Windows did not expose the browser compatibility host executable path."
    }

    $actualPath = Get-NormalizedPath $candidates[0].ExecutablePath
    if (-not $actualPath.Equals($ExpectedPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "SwgClient launched a browser compatibility host from an unexpected path: $actualPath"
    }

    return [pscustomobject]@{
        pid = [int]$candidates[0].ProcessId
        parent_pid = [int]$candidates[0].ParentProcessId
        path = $actualPath
        command_line = [string]$candidates[0].CommandLine
    }
}

function Get-LoadedBrowserXulModule {
    param(
        [Parameter(Mandatory)][int]$ProcessId,
        [Parameter(Mandatory)][string]$ExpectedPath
    )

    $module = Get-LoadedWin32ProcessModule -ProcessId $ProcessId -ModuleName "xul.dll"
    if (-not $module) {
        return $null
    }

    $actualPath = $module.path
    if (-not $actualPath.Equals($ExpectedPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Browser compatibility host loaded xul.dll from an unexpected path: $actualPath"
    }

    return [pscustomobject]@{
        name = $module.name
        path = $actualPath
        module_memory_size = [long]$module.module_memory_size
    }
}

function New-LoopbackBrowserProbeListener {
    for ($attempt = 0; $attempt -lt 16; ++$attempt) {
        $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, 0)
        try {
            $listener.Start(8)
            $port = [int]$listener.LocalEndpoint.Port
            if ($port -ge 49152 -and $port -le 65535) {
                return [pscustomobject]@{
                    listener = $listener
                    port = $port
                }
            }
        }
        catch {
            $listener.Stop()
            throw
        }
        $listener.Stop()
    }

    throw "Windows did not allocate a loopback listener in the required dynamic high-port range 49152-65535."
}

function Get-TextSha256 {
    param([Parameter(Mandatory)][string]$Text)

    $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString($sha256.ComputeHash($bytes)).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
        [Array]::Clear($bytes, 0, $bytes.Length)
    }
}

function Receive-LoopbackBrowserProbeRequest {
    param(
        [Parameter(Mandatory)][Net.Sockets.TcpListener]$Listener,
        [Parameter(Mandatory)][string]$ExpectedPath
    )

    if (-not $Listener.Pending()) {
        return $null
    }

    $client = $Listener.AcceptTcpClient()
    $stream = $null
    $requestBytes = [IO.MemoryStream]::new()
    try {
        $client.ReceiveTimeout = 2000
        $client.SendTimeout = 2000
        $stream = $client.GetStream()
        $buffer = [byte[]]::new(1024)
        try {
            while ($requestBytes.Length -lt 8192) {
                $remaining = [Math]::Min($buffer.Length, 8192 - [int]$requestBytes.Length)
                $read = $stream.Read($buffer, 0, $remaining)
                if ($read -le 0) {
                    break
                }
                $requestBytes.Write($buffer, 0, $read)
                $requestText = [Text.Encoding]::ASCII.GetString($requestBytes.GetBuffer(), 0, [int]$requestBytes.Length)
                if ($requestText.IndexOf("`r`n`r`n", [StringComparison]::Ordinal) -ge 0) {
                    break
                }
            }
        }
        finally {
            [Array]::Clear($buffer, 0, $buffer.Length)
        }

        $requestText = [Text.Encoding]::ASCII.GetString($requestBytes.GetBuffer(), 0, [int]$requestBytes.Length)
        $lineEnd = $requestText.IndexOf("`r`n", [StringComparison]::Ordinal)
        $requestLine = if ($lineEnd -ge 0) { $requestText.Substring(0, $lineEnd) } else { "" }
        $method = ""
        $requestTarget = ""
        $wellFormed = $requestLine -match '^(?<method>[A-Z]+) (?<target>[^ ]+) HTTP/1\.[01]$'
        if ($wellFormed) {
            $method = [string]$Matches.method
            $requestTarget = [string]$Matches.target
        }
        $exact = $wellFormed -and $method -ceq "GET" -and $requestTarget -ceq $ExpectedPath

        $body = if ($exact) {
            '<!doctype html><html><head><title>SWG TCG Browser Probe</title></head><body style="background:#17324d;color:#ffffff"><h1>SWG TCG Browser Probe</h1></body></html>'
        }
        else {
            '<!doctype html><html><body>Not Found</body></html>'
        }
        $bodyBytes = [Text.Encoding]::UTF8.GetBytes($body)
        $status = if ($exact) { "200 OK" } else { "404 Not Found" }
        $responseHeader = "HTTP/1.1 $status`r`nContent-Type: text/html; charset=utf-8`r`nContent-Length: $($bodyBytes.Length)`r`nConnection: close`r`nCache-Control: no-store`r`n`r`n"
        $responseHeaderBytes = [Text.Encoding]::ASCII.GetBytes($responseHeader)
        try {
            $stream.Write($responseHeaderBytes, 0, $responseHeaderBytes.Length)
            $stream.Write($bodyBytes, 0, $bodyBytes.Length)
            $stream.Flush()
        }
        finally {
            [Array]::Clear($responseHeaderBytes, 0, $responseHeaderBytes.Length)
            [Array]::Clear($bodyBytes, 0, $bodyBytes.Length)
        }

        return [pscustomobject]@{
            method = $method
            exact_path = [bool]$exact
            request_target_sha256 = if ($wellFormed) { Get-TextSha256 -Text $requestTarget } else { $null }
            header_bytes = [int]$requestBytes.Length
        }
    }
    finally {
        $requestBytes.Dispose()
        if ($stream) {
            $stream.Dispose()
        }
        $client.Close()
    }
}

function Get-NewDisallowedProcesses {
    param(
        [Parameter(Mandatory)][string[]]$Names,
        [Parameter(Mandatory)][AllowEmptyCollection()][Collections.Generic.HashSet[int]]$BaselineProcessIds,
        [Parameter(Mandatory)][int[]]$RootProcessIds
    )

    $rootIds = [Collections.Generic.HashSet[int]]::new()
    foreach ($rootProcessId in $RootProcessIds) {
        $null = $rootIds.Add($rootProcessId)
    }

    $snapshot = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue)
    $processById = @{}
    foreach ($processInfo in $snapshot) {
        $processById[[int]$processInfo.ProcessId] = $processInfo
    }

    foreach ($candidate in @($snapshot | Where-Object {
        $_.Name -in $Names -and -not $BaselineProcessIds.Contains([int]$_.ProcessId)
    })) {
        $visited = [Collections.Generic.HashSet[int]]::new()
        $ancestor = $candidate
        while ($ancestor) {
            $ancestorId = [int]$ancestor.ProcessId
            if (-not $visited.Add($ancestorId)) {
                break
            }

            $parentId = [int]$ancestor.ParentProcessId
            if ($rootIds.Contains($parentId)) {
                $candidate | Select-Object Name, ProcessId, ParentProcessId, ExecutablePath
                break
            }
            if ($parentId -eq 0 -or -not $processById.ContainsKey($parentId)) {
                break
            }
            $ancestor = $processById[$parentId]
        }
    }
}

function Get-LocalDevKitConnections {
    param([Parameter(Mandatory)][int]$ProcessId)

    return @(
        Get-NetTCPConnection -OwningProcess $ProcessId -State Established -ErrorAction SilentlyContinue |
            Where-Object {
                $_.RemoteAddress -in @("127.0.0.1", "::ffff:127.0.0.1") -and
                $_.RemotePort -in @(16782, 16783)
            } |
            ForEach-Object {
                [pscustomobject]@{
                    owning_process = [int]$_.OwningProcess
                    local_address = [string]$_.LocalAddress
                    local_port = [int]$_.LocalPort
                    remote_address = [string]$_.RemoteAddress
                    remote_port = [int]$_.RemotePort
                    state = [string]$_.State
                }
            }
    )
}

function Stop-AllowlistedProcess {
    param(
        [Parameter(Mandatory)][int]$ProcessId,
        [Parameter(Mandatory)][string]$AllowedExecutablePath
    )

    $processInfo = Get-CimInstance Win32_Process -Filter "ProcessId=$ProcessId" -ErrorAction SilentlyContinue
    if (-not $processInfo) {
        return
    }
    if (-not $processInfo.ExecutablePath) {
        throw "Refusing to stop PID $ProcessId because Windows did not expose its executable path."
    }

    $actualPath = Get-NormalizedPath $processInfo.ExecutablePath
    if (-not $actualPath.Equals($AllowedExecutablePath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to stop PID $ProcessId because its executable is outside the exact test allowlist: $actualPath"
    }

    $process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if (-not $process) {
        return
    }
    $null = $process.CloseMainWindow()
    if (-not $process.WaitForExit(3000)) {
        Stop-Process -Id $ProcessId -Force
        $null = $process.WaitForExit(5000)
    }
}

function Protect-EvidenceText {
    param(
        [AllowEmptyString()][string]$Text,
        [object[]]$Secrets
    )

    if ($null -eq $Text) {
        return $null
    }

    $protected = $Text
    foreach ($secret in $Secrets) {
        $secretText = [string]$secret
        if (-not [string]::IsNullOrEmpty($secretText)) {
            $protected = $protected.Replace($secretText, "[redacted]")
        }
    }
    $protected = [regex]::Replace($protected, '(?i)(session|session_id|challenge)=(''[^'']*''|"[^"]*"|[^\s,;]+)', '$1=[redacted]')
    return $protected
}

function New-RandomHex {
    param([ValidateRange(8, 128)][int]$ByteCount)

    $bytes = [byte[]]::new($ByteCount)
    $random = [Security.Cryptography.RandomNumberGenerator]::Create()
    try {
        $random.GetBytes($bytes)
        return [BitConverter]::ToString($bytes).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $random.Dispose()
        [Array]::Clear($bytes, 0, $bytes.Length)
    }
}

function Restore-TestEnvironment {
    param([hashtable]$Snapshot)

    if (-not $Snapshot) {
        return
    }
    foreach ($name in $Snapshot.Keys) {
        [Environment]::SetEnvironmentVariable($name, $Snapshot[$name], [EnvironmentVariableTarget]::Process)
    }
}

$repoRoot = Get-NormalizedPath (Join-Path $PSScriptRoot "..")
$workspaceRoot = Get-NormalizedPath (Join-Path $repoRoot "..")
$outputRoot = Get-NormalizedPath (Join-Path $workspaceRoot "_whitengold_client")
$protectedClientRoot = Get-NormalizedPath (Join-Path $workspaceRoot "_client")
$protectedTcgRoot = Get-NormalizedPath (Join-Path $protectedClientRoot "TradingCardGame")

if ($GameActionBrowserProbe -and ($Architecture -ne "x64" -or $Trigger -ne "GameAction")) {
    throw "GameActionBrowserProbe requires -Architecture x64 -Trigger GameAction."
}
if ($GameActionBrowserProbe -and $KeepProcess) {
    throw "GameActionBrowserProbe cannot be combined with KeepProcess because both compatibility hosts must be proven to exit with SwgClient."
}
if ($AutoLoginLocalServer -and $Trigger -ne "GameAction") {
    throw "AutoLoginLocalServer is available only with -Trigger GameAction."
}
$localStationIdWasBound = $PSBoundParameters.ContainsKey("LocalStationId")
if ($AutoLoginLocalServer -and -not $localStationIdWasBound) {
    throw "AutoLoginLocalServer requires an explicit -LocalStationId; no shared default account is permitted."
}
if (-not $AutoLoginLocalServer -and $localStationIdWasBound) {
    throw "LocalStationId is valid only with -AutoLoginLocalServer."
}

if (-not $RuntimeRoot) {
    $runtimeFolder = if ($Architecture -eq "x64") { "x64-dx11-release-juce" } else { "win32-dx11-release-juce" }
    $RuntimeRoot = Join-Path $outputRoot "runtime\$runtimeFolder"
}
if (-not $DevKitRoot) {
    $DevKitRoot = Join-Path $outputRoot "tcg-devkit\source"
}

$runtimeRootPath = Get-NormalizedPath $RuntimeRoot
$devKitRootPath = Get-NormalizedPath $DevKitRoot
if (-not (Test-PathWithin -Path $runtimeRootPath -Parent $outputRoot)) {
    throw "RuntimeRoot must stay below the designated writable output root: $outputRoot"
}
if (-not (Test-PathWithin -Path $devKitRootPath -Parent $outputRoot)) {
    throw "DevKitRoot must stay below the designated writable output root: $outputRoot"
}
if (-not (Test-PathWithin -Path $protectedTcgRoot -Parent $protectedClientRoot)) {
    throw "The protected TCG reference did not resolve beneath _client."
}

$gameExecutable = Get-NormalizedPath (Join-Path $runtimeRootPath "SwgClient_r.exe")
$tcgDll = Get-NormalizedPath (Join-Path $runtimeRootPath "TradingCardGame\SWGTCG.dll")
$compatibilityHost = Get-NormalizedPath (Join-Path $runtimeRootPath "TcgCompatibilityHost.exe")
$browserBrokerRoot = Get-NormalizedPath (Join-Path $runtimeRootPath "runtime\mozilla-broker")
$browserCompatibilityHost = Get-NormalizedPath (Join-Path $browserBrokerRoot "BrowserCompatibilityHost.exe")
$browserXul = Get-NormalizedPath (Join-Path $browserBrokerRoot "xul.dll")
$hostConfig = Get-NormalizedPath (Join-Path $runtimeRootPath "TradingCardGame\host.svr")
$loginConfig = Get-NormalizedPath (Join-Path $runtimeRootPath "login.cfg")
$localDevKitRuntimeManifest = Get-NormalizedPath (Join-Path $runtimeRootPath "local-devkit-runtime.json")
$clientLog = Get-NormalizedPath (Join-Path $runtimeRootPath "logs\warning.log")
$serverLog = Get-NormalizedPath (Join-Path $devKitRootPath ".local\logs\server.stdout.log")
$serverErrorLog = Get-NormalizedPath (Join-Path $devKitRootPath ".local\logs\server.stderr.log")

foreach ($requiredDirectory in @($protectedTcgRoot, $runtimeRootPath, $devKitRootPath)) {
    if (-not (Test-Path -LiteralPath $requiredDirectory -PathType Container)) {
        throw "Required embedded-client test directory is missing: $requiredDirectory"
    }
}
$requiredFiles = @($gameExecutable, $tcgDll, $hostConfig, $localDevKitRuntimeManifest, $serverLog)
if ($Architecture -eq "x64") {
    $requiredFiles += $compatibilityHost
}
if ($GameActionBrowserProbe) {
    if (-not (Test-PathWithin -Path $browserBrokerRoot -Parent $runtimeRootPath)) {
        throw "The browser broker resolved outside the writable runtime."
    }
    $requiredFiles += @($browserCompatibilityHost, $browserXul)
}
if ($AutoLoginLocalServer) {
    $requiredFiles += $loginConfig
}
foreach ($requiredFile in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required embedded-client test file is missing: $requiredFile"
    }
}

$expectedGameMachine = if ($Architecture -eq "x64") { 0x8664 } else { 0x014c }
if ((Get-PeMachine -Path $gameExecutable) -ne $expectedGameMachine) {
    throw "The embedded harness requires the $Architecture SwgClient build (PE machine 0x$($expectedGameMachine.ToString('x4')))."
}
if ((Get-PeMachine -Path $tcgDll) -ne 0x014c) {
    throw "The staged SWGTCG.dll is not the required Win32 DLL (PE machine 0x014c)."
}
if ($Architecture -eq "x64" -and (Get-PeMachine -Path $compatibilityHost) -ne 0x014c) {
    throw "The staged compatibility host must be Win32 (PE machine 0x014c)."
}
if ($GameActionBrowserProbe -and (Get-PeMachine -Path $browserCompatibilityHost) -ne 0x014c) {
    throw "The staged browser compatibility host must be Win32 (PE machine 0x014c)."
}
if ($GameActionBrowserProbe -and (Get-PeMachine -Path $browserXul) -ne 0x014c) {
    throw "The staged browser broker xul.dll must be Win32 (PE machine 0x014c)."
}

if ($AutoLoginLocalServer) {
    $loginConfigText = [IO.File]::ReadAllText($loginConfig)
    if ($loginConfigText -notmatch '(?im)^\s*loginServerAddress0\s*=\s*(127\.0\.0\.1|localhost|::1)\s*$') {
        throw "AutoLoginLocalServer requires login.cfg loginServerAddress0 to be an exact loopback address."
    }
    $loginConfigText = $null
}

$hostLines = @(Get-Content -LiteralPath $hostConfig | ForEach-Object { $_.Trim() } | Where-Object { $_ })
if ($hostLines.Count -ne 2 -or
    @($hostLines | Where-Object { $_ -match '^ghost\s+127\.0\.0\.1$' }).Count -ne 1 -or
    @($hostLines | Where-Object { $_ -match '^gport\s+16782$' }).Count -ne 1) {
    throw "The writable runtime host.svr must target the local dev-kit gateway at 127.0.0.1:16782. The harness will not rewrite it."
}
$hostConfigHash = (Get-FileHash -LiteralPath $hostConfig -Algorithm SHA256).Hash.ToLowerInvariant()
$loginConfigHash = (Get-FileHash -LiteralPath $loginConfig -Algorithm SHA256).Hash.ToLowerInvariant()
try {
    $localDevKitRuntime = [IO.File]::ReadAllText($localDevKitRuntimeManifest) | ConvertFrom-Json
}
catch {
    throw "The staged local-devkit runtime manifest is not valid JSON: $localDevKitRuntimeManifest"
}
if ($localDevKitRuntime.schema -ne 1 -or
    $localDevKitRuntime.mode -ne "loopback-development-only" -or
    $localDevKitRuntime.tcg_gateway -ne "127.0.0.1:16782" -or
    $localDevKitRuntime.swg_login -ne "127.0.0.1:44453" -or
    $localDevKitRuntime.host_svr.path -ne "TradingCardGame\host.svr" -or
    $localDevKitRuntime.login_cfg.path -ne "login.cfg" -or
    -not ([string]$localDevKitRuntime.host_svr.sha256).Equals($hostConfigHash, [StringComparison]::OrdinalIgnoreCase) -or
    -not ([string]$localDevKitRuntime.login_cfg.sha256).Equals($loginConfigHash, [StringComparison]::OrdinalIgnoreCase)) {
    throw "The staged local-devkit configuration does not match its recorded loopback manifest. Restage with -LocalDevKit."
}
$localDevKitRuntime = $null

$conflictingNames = @("SwgClient_r.exe", "TcgCompatibilityHost.exe", "BrowserCompatibilityHost.exe")
$conflictingExactPaths = @($gameExecutable)
if ($Architecture -eq "x64") {
    $conflictingExactPaths += $compatibilityHost
}
if ($GameActionBrowserProbe) {
    $conflictingExactPaths += $browserCompatibilityHost
}
$conflictingProcesses = @(
    Get-CimInstance Win32_Process |
        Where-Object {
            if ($_.Name -notin $conflictingNames -or -not $_.ExecutablePath) {
                return $false
            }
            $candidatePath = Get-NormalizedPath $_.ExecutablePath
            return $conflictingExactPaths -contains $candidatePath
        } |
        Select-Object Name, ProcessId, ExecutablePath
)
if ($conflictingProcesses.Count -ne 0) {
    $conflictingProcesses | Format-Table -AutoSize | Out-Host
    throw "Close existing SWG/TCG processes from this exact staged runtime before running the isolated embedded integration test."
}

$disallowedExternalProcessNames = @(
    "TCGRebornLauncher.exe",
    "SWGTCGGame.exe",
    "chrome.exe",
    "msedge.exe",
    "firefox.exe",
    "iexplore.exe",
    "brave.exe",
    "opera.exe",
    "vivaldi.exe",
    "waterfox.exe"
)
$baselineDisallowedProcessIds = [Collections.Generic.HashSet[int]]::new()
foreach ($processInfo in @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -in $disallowedExternalProcessNames })) {
    $null = $baselineDisallowedProcessIds.Add([int]$processInfo.ProcessId)
}

try {
    $health = Invoke-RestMethod -Uri "http://127.0.0.1:16780/ping" -TimeoutSec 5
}
catch {
    throw "The local TCG dev-kit auth service is not reachable at 127.0.0.1:16780."
}
if (-not $health.ok -or -not $health.open_registration) {
    throw "The local TCG dev-kit auth service must be healthy with open registration."
}
$health = $null

$referenceManifestBefore = Get-ReferenceManifest -Root $protectedTcgRoot
$referenceManifestAfter = $null
$randomHex = New-RandomHex -ByteCount 24
$username = "codex_embedded_{0}_{1}" -f [DateTime]::UtcNow.ToString("yyyyMMddHHmmss"), $randomHex.Substring(0, 8)
$password = $randomHex.Substring(8)
$nonce = "tcg_embedded_{0}_{1}" -f [DateTime]::UtcNow.ToString("yyyyMMddHHmmss"), $randomHex.Substring(0, 8)
$randomHex = $null

$sessionId = $null
$challenge = $null
$characterId = $null
$redactionSecrets = [Collections.Generic.List[string]]::new()
$redactionSecrets.Add($password)
# This low-entropy loopback password must never enter a command line or evidence text;
# adding it to whole-document replacement would corrupt keys such as local_station_id.
$localSwgLoginPassword = if ($AutoLoginLocalServer) { "local" } else { $null }
$environmentSnapshot = @{}
$environmentApplied = $false
$gameProcess = $null
$gameProcessId = $null
$hostProcess = $null
$browserHostProcess = $null
$browserXulModule = $null
$browserProbeListenerState = $null
$browserProbeRequest = $null
$browserProbeRequestCount = 0
$expectedBrowserProbePath = "/tcg-browser-probe/$nonce"
$tcgHostExitedWithGame = $false
$browserHostExitedWithGame = $false
$pendingEvidence = $null
$evidencePath = $null
$primaryError = $null
$integrityError = $null

try {
    $registrationBody = @{
        username = $username
        password = $password
        character_id = 1
    } | ConvertTo-Json -Compress
    try {
        $registration = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:16780/register" -ContentType "application/json" -Body $registrationBody -TimeoutSec 15
    }
    catch {
        throw "The dev-kit rejected disposable-account registration. No credential values were logged."
    }
    finally {
        $registrationBody = $null
        $password = $null
    }

    if (-not $registration.ok -or -not $registration.session_id -or -not $registration.challenge -or
        -not $registration.username -or -not $registration.character_id) {
        throw "Disposable-account registration returned an incomplete credential envelope."
    }
    $sessionId = [string]$registration.session_id
    $challenge = [string]$registration.challenge
    $characterId = [string]$registration.character_id
    if ($registration.username -ne $username -or $sessionId.Length -gt 50 -or
        $challenge.Length -gt 115 -or $characterId.Length -gt 112) {
        throw "Disposable-account registration returned values outside the embedded adapter ABI limits."
    }
    $redactionSecrets.Add($sessionId)
    $redactionSecrets.Add($challenge)
    $registration = $null

    $testEnvironment = @{
        SWGTCG_TEST_USERNAME = $username
        SWGTCG_TEST_SESSION = $sessionId
        SWGTCG_TEST_CHALLENGE = $challenge
        SWGTCG_TEST_CHARACTER_ID = $characterId
    }
    if ($AutoLoginLocalServer) {
        $testEnvironment.SWGTCG_TEST_SWG_LOGIN_PASSWORD = $localSwgLoginPassword
    }
    foreach ($name in $testEnvironment.Keys) {
        $environmentSnapshot[$name] = [Environment]::GetEnvironmentVariable($name, [EnvironmentVariableTarget]::Process)
        [Environment]::SetEnvironmentVariable($name, $testEnvironment[$name], [EnvironmentVariableTarget]::Process)
    }
    $environmentApplied = $true

    if ($GameActionBrowserProbe) {
        $browserProbeListenerState = New-LoopbackBrowserProbeListener
    }

    $triggerArguments = ""
    if ($Trigger -eq "GameAction") {
        $sceneTimeoutSeconds = $TimeoutSeconds
        $triggerArguments = " trigger=gameAction timeoutSeconds=$sceneTimeoutSeconds"
    }
    $browserProbeArguments = if ($GameActionBrowserProbe) {
        " browserProbePort=$($browserProbeListenerState.port)"
    }
    else {
        ""
    }
    $autoLoginArguments = if ($AutoLoginLocalServer) {
        " -s ClientGame loginClientID=$LocalStationId autoConnectToLoginServer=true centralServerName=swg autoConnectToCentralServer=true autoConnectToGameServer=true"
    }
    else {
        ""
    }
    $arguments = "-- -s ClientGame/TcgIntegrationTest enabled=true nonce=$nonce$triggerArguments$browserProbeArguments -s ClientGame skipIntro=true skipSplash=true$autoLoginArguments"
    try {
        $gameProcess = Start-Process -FilePath $gameExecutable -ArgumentList $arguments -WorkingDirectory $runtimeRootPath -PassThru
        $gameProcessId = [int]$gameProcess.Id
        $gameProcessInfo = Get-CimInstance Win32_Process -Filter "ProcessId=$gameProcessId" -ErrorAction Stop
        if ([string]$gameProcessInfo.CommandLine -match '(?i)(^|\s)loginClientPassword\s*=') {
            throw "The SWG auto-login password appeared on the game command line."
        }
        $gameProcessInfo = $null
    }
    finally {
        Restore-TestEnvironment -Snapshot $environmentSnapshot
        $environmentApplied = $false
        $testEnvironment.Clear()
        $testEnvironment = $null
        $arguments = $null
        $triggerArguments = $null
        $browserProbeArguments = $null
        $autoLoginArguments = $null
    }

    if ($Trigger -eq "GameAction") {
        if ($AutoLoginLocalServer) {
            Write-Host "GameAction local auto-login is enabled against the loopback login server; the proof starts only after the live HUD action is available."
        }
        else {
            Write-Host "GameAction mode is interactive: log in to SWG and enter a character within $sceneTimeoutSeconds seconds; the proof starts only after the live HUD action is available."
        }
    }
    $proofTimeoutSeconds = if ($Trigger -eq "GameAction") { $TimeoutSeconds + 60 } else { $TimeoutSeconds }
    $deadline = [DateTime]::UtcNow.AddSeconds($proofTimeoutSeconds)
    $loadedModule = $null
    $lobbyConnection = $null
    $embeddedLaunchLine = $null
    $actionDispatchLine = $null
    $buttonPressLine = $null
    $inputMapLine = $null
    $inputExecuteLine = $null
    $messageQueueLine = $null
    $uiParserLine = $null
    $actionTransportOrderReady = $false
    $hudHandlerLine = $null
    $mediatorActivationLine = $null
    $actionResultLine = $null
    $sceneTimeoutLine = $null
    $surfaceLine = $null
    $surface = $null
    $embeddedInputLine = $null
    $eulaInputLine = $null
    $viewMoreInputLine = $null
    $authenticCallbackLine = $null
    $browserPageLine = $null
    $browserProxyWindowLine = $null
    $browserFirstFrameLine = $null
    $browserFrame = $null
    $gatewayLine = $null
    $gatewayConnectionId = $null
    $lobbyLine = $null
    $lobbyConnectionId = $null
    $accountId = $null
    $homeLines = @()
    $clientLogSha256 = $null
    $noncePattern = [regex]::Escape($nonce)
    $usernamePattern = [regex]::Escape($username)
    $nextExternalProcessAudit = [DateTime]::UtcNow

    do {
        $gameProcess.Refresh()
        if ($gameProcess.HasExited) {
            throw "SwgClient exited before completing the embedded TCG proof (exit code $($gameProcess.ExitCode))."
        }

        if ($Architecture -eq "x64" -and -not $hostProcess) {
            $hostProcess = Get-ChildCompatibilityHost -ParentProcessId $gameProcess.Id -ExpectedPath $compatibilityHost
            if ($hostProcess) {
                if ($hostProcess.command_line -match '(?i)--(?:user(?:name)?|password|session(?:-?id)?|challenge|character(?:-?id)?)\b') {
                    throw "The compatibility-host command line exposed a TCG credential argument."
                }
                foreach ($secret in @($username, $sessionId, $challenge, $characterId)) {
                    $secretText = [string]$secret
                    if ($secretText.Length -ge 8 -and
                        $hostProcess.command_line.IndexOf($secretText, [StringComparison]::Ordinal) -ge 0) {
                        throw "A TCG credential appeared in the compatibility-host command line."
                    }
                }
            }
        }

        if ($GameActionBrowserProbe -and -not $browserHostProcess) {
            $browserHostProcess = Get-ChildBrowserCompatibilityHost `
                -ParentProcessId $gameProcess.Id `
                -ExpectedPath $browserCompatibilityHost
            if ($browserHostProcess) {
                if ($browserHostProcess.command_line -match '(?i)--(?:user(?:name)?|password|session(?:-?id)?|challenge|character(?:-?id)?)\b') {
                    throw "The browser compatibility-host command line exposed a TCG credential argument."
                }
                foreach ($secret in @($username, $sessionId, $challenge, $characterId)) {
                    $secretText = [string]$secret
                    if ($secretText.Length -ge 8 -and
                        $browserHostProcess.command_line.IndexOf($secretText, [StringComparison]::Ordinal) -ge 0) {
                        throw "A TCG credential appeared in the browser compatibility-host command line."
                    }
                }
                if ($browserHostProcess.command_line.IndexOf($nonce, [StringComparison]::Ordinal) -ge 0 -or
                    $browserHostProcess.command_line.IndexOf("tcg-browser-probe", [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                    throw "The browser compatibility-host command line exposed probe navigation data."
                }
            }
        }
        if ($GameActionBrowserProbe -and $browserHostProcess -and -not $browserXulModule) {
            $browserXulModule = Get-LoadedBrowserXulModule `
                -ProcessId $browserHostProcess.pid `
                -ExpectedPath $browserXul
        }

        if ($GameActionBrowserProbe) {
            while ($browserProbeListenerState.listener.Pending()) {
                $candidateRequest = Receive-LoopbackBrowserProbeRequest `
                    -Listener $browserProbeListenerState.listener `
                    -ExpectedPath $expectedBrowserProbePath
                ++$browserProbeRequestCount
                if (-not $candidateRequest -or -not $candidateRequest.exact_path -or $candidateRequest.method -cne "GET") {
                    throw "The browser probe listener received a request other than the exact allowlisted GET target; raw request data was not logged."
                }
                if ($browserProbeRequest) {
                    throw "The browser probe listener received more than one request; expected exactly one authentic navigation callback."
                }
                $browserProbeRequest = $candidateRequest
            }
        }

        if ([DateTime]::UtcNow -ge $nextExternalProcessAudit) {
            $newExternalProcesses = @(Get-NewDisallowedProcesses `
                -Names $disallowedExternalProcessNames `
                -BaselineProcessIds $baselineDisallowedProcessIds `
                -RootProcessIds @([int]$gameProcess.Id))
            if ($newExternalProcesses.Count -ne 0) {
                $newExternalProcesses | Format-Table -AutoSize | Out-Host
                throw "A disallowed external browser or TCG client process appeared during the embedded integration test."
            }
            $nextExternalProcessAudit = [DateTime]::UtcNow.AddSeconds(1)
        }

        $tcgOwnerProcessId = if ($Architecture -eq "x64") {
            if ($hostProcess) { [int]$hostProcess.pid } else { 0 }
        }
        else {
            [int]$gameProcess.Id
        }

        if (-not $loadedModule -and $tcgOwnerProcessId -ne 0) {
            $loadedModule = Get-LoadedTcgModule -ProcessId $tcgOwnerProcessId -ExpectedPath $tcgDll
        }

        if ($tcgOwnerProcessId -ne 0) {
            foreach ($connection in @(Get-LocalDevKitConnections -ProcessId $tcgOwnerProcessId)) {
                if ($connection.remote_port -eq 16783) {
                    $lobbyConnection = $connection
                }
            }
        }

        if (Test-Path -LiteralPath $clientLog -PathType Leaf) {
            $clientTail = @(Get-Content -LiteralPath $clientLog -Tail 2000 -ErrorAction SilentlyContinue)
            if (-not $embeddedLaunchLine) {
                $embeddedLaunchLine = $clientTail |
                    Where-Object { $_ -match "TCG integration: embedded-launch nonce=\[$noncePattern\] pid=$($gameProcess.Id) result=1\." } |
                    Select-Object -Last 1
            }
            if ($Trigger -eq "GameAction") {
                if (-not $actionDispatchLine) {
                    $actionDispatchLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-dispatch nonce=\[$noncePattern\] pid=$($gameProcess.Id) scene=present workspace=present handler=hud-queued\." } |
                        Select-Object -Last 1
                }
                if (-not $buttonPressLine) {
                    $buttonPressLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-button-press nonce=\[$noncePattern\] pid=$($gameProcess.Id) button=buttonTcg visible=true enabled=true command=CMD_uiTcg\." } |
                        Select-Object -Last 1
                }
                if (-not $inputMapLine) {
                    $inputMapLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-input-map pid=$($gameProcess.Id) command=CMD_uiTcg mapping=present route=ui-action-tcg\." } |
                        Select-Object -Last 1
                }
                if (-not $inputExecuteLine) {
                    $inputExecuteLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-input-execute pid=$($gameProcess.Id) command=CMD_uiTcg result=queued\." } |
                        Select-Object -Last 1
                }
                if (-not $messageQueueLine) {
                    $messageQueueLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-message-queue pid=$($gameProcess.Id) route=ui-action-tcg result=observed\." } |
                        Select-Object -Last 1
                }
                if (-not $uiParserLine) {
                    $uiParserLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-ui-parser pid=$($gameProcess.Id) action=tcg result=dispatching\." } |
                        Select-Object -Last 1
                }
                if (-not $hudHandlerLine) {
                    $hudHandlerLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-hud-handler nonce=\[$noncePattern\] pid=$($gameProcess.Id)\." } |
                        Select-Object -Last 1
                }
                if (-not $mediatorActivationLine) {
                    $mediatorActivationLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-mediator-activation nonce=\[$noncePattern\] pid=$($gameProcess.Id) activated=true\." } |
                        Select-Object -Last 1
                }
                if (-not $actionResultLine) {
                    $actionResultLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-result nonce=\[$noncePattern\] pid=$($gameProcess.Id) handled=true credentials=consumed mediator=active launched=true\." } |
                        Select-Object -Last 1
                }
                if (-not $sceneTimeoutLine) {
                    $sceneTimeoutLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: game-action-timeout nonce=\[$noncePattern\] pid=$($gameProcess.Id)" } |
                        Select-Object -Last 1
                }
                if ($sceneTimeoutLine) {
                    throw "SwgClient timed out before an in-game scene and game workspace became available for the gameAction trigger."
                }

                if (-not $actionTransportOrderReady -and $buttonPressLine -and $inputMapLine -and
                    $inputExecuteLine -and $actionDispatchLine -and
                    $messageQueueLine -and $uiParserLine -and $hudHandlerLine) {
                    $buttonPressIndex = [Array]::LastIndexOf($clientTail, $buttonPressLine)
                    $inputMapIndex = [Array]::LastIndexOf($clientTail, $inputMapLine)
                    $inputExecuteIndex = [Array]::LastIndexOf($clientTail, $inputExecuteLine)
                    $actionDispatchIndex = [Array]::LastIndexOf($clientTail, $actionDispatchLine)
                    $messageQueueIndex = [Array]::LastIndexOf($clientTail, $messageQueueLine)
                    $uiParserIndex = [Array]::LastIndexOf($clientTail, $uiParserLine)
                    $hudHandlerIndex = [Array]::LastIndexOf($clientTail, $hudHandlerLine)
                    $actionTransportOrderReady = $buttonPressIndex -ge 0 -and
                        $buttonPressIndex -lt $inputMapIndex -and
                        $inputMapIndex -lt $inputExecuteIndex -and
                        $inputExecuteIndex -lt $actionDispatchIndex -and
                        $actionDispatchIndex -lt $messageQueueIndex -and
                        $messageQueueIndex -lt $uiParserIndex -and
                        $uiParserIndex -lt $hudHandlerIndex
                }
            }
            if (-not $surfaceLine) {
                $backendPattern = if ($Architecture -eq "x64") { "compatibility-host" } else { "loaded" }
                $surfaceCandidate = $clientTail |
                    Where-Object { $_ -match "TCG integration: embedded-surface-ready nonce=\[$noncePattern\] pid=$($gameProcess.Id) module=$backendPattern windows=(?<windows>[1-9][0-9]*) width=(?<width>[1-9][0-9]*) height=(?<height>[1-9][0-9]*) stride=(?<stride>[1-9][0-9]*)\." } |
                    Select-Object -Last 1
                if ($surfaceCandidate -and $surfaceCandidate -match "embedded-surface-ready nonce=\[$noncePattern\] pid=$($gameProcess.Id) module=$backendPattern windows=(?<windows>[1-9][0-9]*) width=(?<width>[1-9][0-9]*) height=(?<height>[1-9][0-9]*) stride=(?<stride>[1-9][0-9]*)\.") {
                    $surfaceValues = @{
                        windows = [uint32]$Matches.windows
                        width = [uint32]$Matches.width
                        height = [uint32]$Matches.height
                        stride = [uint32]$Matches.stride
                    }
                    if ($surfaceValues.width -le 16384 -and $surfaceValues.height -le 16384 -and
                        $surfaceValues.stride -ge ($surfaceValues.width * 4) -and $surfaceValues.stride -le (16384 * 4)) {
                        $surfaceLine = $surfaceCandidate
                        $surface = [pscustomobject]$surfaceValues
                    }
                }
            }
            if ($GameActionBrowserProbe) {
                if (-not $embeddedInputLine) {
                    $embeddedInputLine = $clientTail |
                        Where-Object { $_ -match 'TCG integration: embedded-control-input-dispatched\.' } |
                        Select-Object -Last 1
                }
                if (-not $eulaInputLine) {
                    $eulaInputLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: browser-probe-eula-input nonce=\[$noncePattern\] pid=$($gameProcess.Id) normalized=1075/1772,904/1293\." } |
                        Select-Object -Last 1
                }
                if (-not $viewMoreInputLine) {
                    $viewMoreInputLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: browser-probe-view-more-input nonce=\[$noncePattern\] pid=$($gameProcess.Id) normalized=1288/1772,1101/1293\." } |
                        Select-Object -Last 1
                }

                $authenticCallbackCandidates = @(
                    $clientTail |
                        Where-Object { $_ -match "TCG integration: browser-probe-authentic-callback nonce=\[$noncePattern\] pid=$($gameProcess.Id)\." }
                )
                if ($authenticCallbackCandidates.Count -gt 1) {
                    throw "SwgClient reported more than one authentic TCG navigation callback for the browser probe."
                }
                if (-not $authenticCallbackLine -and $authenticCallbackCandidates.Count -eq 1) {
                    $authenticCallbackLine = $authenticCallbackCandidates[0]
                }
                if (-not $browserPageLine) {
                    $browserPageLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: browser-probe-page-created nonce=\[$noncePattern\] pid=$($gameProcess.Id)\." } |
                        Select-Object -Last 1
                }
                if (-not $browserProxyWindowLine) {
                    $browserProxyWindowLine = $clientTail |
                        Where-Object { $_ -match "TCG integration: browser-probe-proxy-window-created nonce=\[$noncePattern\] pid=$($gameProcess.Id) width=[1-9][0-9]* height=[1-9][0-9]*\." } |
                        Select-Object -Last 1
                }
                if (-not $browserFirstFrameLine) {
                    $browserFrameCandidate = $clientTail |
                        Where-Object { $_ -match "TCG integration: browser-probe-first-nonzero-frame nonce=\[$noncePattern\] pid=$($gameProcess.Id) width=(?<width>[1-9][0-9]*) height=(?<height>[1-9][0-9]*)\." } |
                        Select-Object -Last 1
                    if ($browserFrameCandidate -and
                        $browserFrameCandidate -match "browser-probe-first-nonzero-frame nonce=\[$noncePattern\] pid=$($gameProcess.Id) width=(?<width>[1-9][0-9]*) height=(?<height>[1-9][0-9]*)\.") {
                        $browserFrameValues = @{
                            width = [uint32]$Matches.width
                            height = [uint32]$Matches.height
                        }
                        if ($browserFrameValues.width -le 16384 -and $browserFrameValues.height -le 16384) {
                            $browserFirstFrameLine = $browserFrameCandidate
                            $browserFrame = [pscustomobject]$browserFrameValues
                        }
                    }
                }
            }
        }

        $serverTail = @(Get-Content -LiteralPath $serverLog -Tail 3000 -ErrorAction SilentlyContinue)
        if (-not $gatewayLine) {
            $gatewayCandidate = $serverTail |
                Where-Object { $_ -match "^\[:16782 c(?<connection>[0-9]+)\].*GetConnectionServer.*user='$usernamePattern'.*-> OK" } |
                Select-Object -Last 1
            if ($gatewayCandidate -and $gatewayCandidate -match "^\[:16782 c(?<connection>[0-9]+)\]") {
                $gatewayLine = $gatewayCandidate
                $gatewayConnectionId = $Matches.connection
            }
        }
        if (-not $lobbyLine) {
            $lobbyCandidate = $serverTail |
                Where-Object { $_ -match "^\[:16783 c(?<connection>[0-9]+)\] lobby login: account (?<account>[0-9]+) \($usernamePattern\)" } |
                Select-Object -Last 1
            if ($lobbyCandidate -and $lobbyCandidate -match "^\[:16783 c(?<connection>[0-9]+)\] lobby login: account (?<account>[0-9]+) \($usernamePattern\)") {
                $lobbyLine = $lobbyCandidate
                $lobbyConnectionId = $Matches.connection
                $accountId = [int]$Matches.account
            }
        }
        if ($lobbyConnectionId) {
            $homeLines = @(
                $serverTail |
                    Where-Object {
                        $_ -match "^\[:16783 c$lobbyConnectionId\].*(collection sync|online decks|login screen = Home|-> Home active:)"
                    }
            )
        }

        $homeReady = @($homeLines | Where-Object { $_ -match "-> Home active:" }).Count -ne 0
        $bridgeReady = $Architecture -ne "x64" -or $hostProcess
        $actionProofReady = $Trigger -ne "GameAction" -or
            ($actionDispatchLine -and $buttonPressLine -and $inputMapLine -and $inputExecuteLine -and
            $messageQueueLine -and $uiParserLine -and $actionTransportOrderReady -and $hudHandlerLine -and
            $mediatorActivationLine -and $actionResultLine)
        $browserProofReady = -not $GameActionBrowserProbe -or
            ($embeddedInputLine -and $eulaInputLine -and $viewMoreInputLine -and $authenticCallbackLine -and
            $browserPageLine -and $browserProxyWindowLine -and $browserFirstFrameLine -and
            $browserHostProcess -and $browserXulModule -and $browserProbeRequest -and
            $browserProbeRequest.exact_path -and $browserProbeRequestCount -eq 1)
        $proofComplete = $bridgeReady -and $loadedModule -and $lobbyConnection -and $embeddedLaunchLine -and
            $surfaceLine -and $gatewayLine -and $lobbyLine -and $homeReady -and $actionProofReady -and $browserProofReady
        if (-not $proofComplete) {
            Start-Sleep -Milliseconds 100
        }
    } while (-not $proofComplete -and [DateTime]::UtcNow -lt $deadline)

    if (-not $proofComplete) {
        $missing = [Collections.Generic.List[string]]::new()
        if ($Architecture -eq "x64" -and -not $hostProcess) { $missing.Add("game-owned TcgCompatibilityHost child") }
        if (-not $loadedModule) { $missing.Add("SWGTCG.dll in the expected TCG owner process") }
        if (-not $lobbyConnection) { $missing.Add("TCG-owner localhost:16783 TCP connection") }
        if (-not $embeddedLaunchLine) { $missing.Add("embedded-launch result=1 marker") }
        if ($Trigger -eq "GameAction" -and -not $actionDispatchLine) { $missing.Add("gameAction dispatch marker") }
        if ($Trigger -eq "GameAction" -and -not $buttonPressLine) { $missing.Add("visible buttonTcg/CMD_uiTcg press marker") }
        if ($Trigger -eq "GameAction" -and -not $inputMapLine) { $missing.Add("current-process CMD_uiTcg input-map route marker") }
        if ($Trigger -eq "GameAction" -and -not $inputExecuteLine) { $missing.Add("current-process CMD_uiTcg queued input marker") }
        if ($Trigger -eq "GameAction" -and -not $messageQueueLine) { $missing.Add("current-process game message-queue observation marker") }
        if ($Trigger -eq "GameAction" -and -not $uiParserLine) { $missing.Add("current-process UI parser dispatch marker") }
        if ($Trigger -eq "GameAction" -and -not $actionTransportOrderReady) { $missing.Add("ordered button/input-map/queue/parser/HUD transport chain") }
        if ($Trigger -eq "GameAction" -and -not $hudHandlerLine) { $missing.Add("actual HUD TCG action-handler marker") }
        if ($Trigger -eq "GameAction" -and -not $mediatorActivationLine) { $missing.Add("active TCG mediator marker") }
        if ($Trigger -eq "GameAction" -and -not $actionResultLine) { $missing.Add("successful gameAction result marker") }
        if (-not $surfaceLine) { $missing.Add("embedded-surface-ready marker with valid dimensions/stride") }
        if (-not $gatewayLine) { $missing.Add("dev-kit gateway acceptance") }
        if (-not $lobbyLine) { $missing.Add("dev-kit lobby login") }
        if (-not $homeReady) { $missing.Add("dev-kit Home active transition (server may need SWGTCG_PROACTIVE_LOBBY=1)") }
        if ($GameActionBrowserProbe -and -not $embeddedInputLine) { $missing.Add("embedded control input-dispatch marker") }
        if ($GameActionBrowserProbe -and -not $eulaInputLine) { $missing.Add("EULA Accept synthetic input marker") }
        if ($GameActionBrowserProbe -and -not $viewMoreInputLine) { $missing.Add("Home View More synthetic input marker") }
        if ($GameActionBrowserProbe -and -not $authenticCallbackLine) { $missing.Add("authentic SWGTCG navigate callback marker") }
        if ($GameActionBrowserProbe -and -not $browserPageLine) { $missing.Add("in-game browser page creation marker") }
        if ($GameActionBrowserProbe -and -not $browserProxyWindowLine) { $missing.Add("libMozilla proxy window creation marker") }
        if ($GameActionBrowserProbe -and -not $browserFirstFrameLine) { $missing.Add("first nonzero in-game browser frame marker") }
        if ($GameActionBrowserProbe -and -not $browserHostProcess) { $missing.Add("game-owned BrowserCompatibilityHost child") }
        if ($GameActionBrowserProbe -and -not $browserXulModule) { $missing.Add("broker-owned xul.dll module") }
        if ($GameActionBrowserProbe -and -not $browserProbeRequest) { $missing.Add("exact loopback browser-probe GET") }
        throw "Timed out waiting for embedded TCG proof: $([string]::Join(', ', $missing.ToArray()))."
    }

    if ($GameActionBrowserProbe) {
        $browserProbeQuiescenceDeadline = [DateTime]::UtcNow.AddSeconds(2)
        while ($true) {
            while ($browserProbeListenerState.listener.Pending()) {
                $candidateRequest = Receive-LoopbackBrowserProbeRequest `
                    -Listener $browserProbeListenerState.listener `
                    -ExpectedPath $expectedBrowserProbePath
                ++$browserProbeRequestCount
                if (-not $candidateRequest -or -not $candidateRequest.exact_path -or $candidateRequest.method -cne "GET") {
                    throw "The browser probe listener received a delayed request other than the exact allowlisted GET target; raw request data was not logged."
                }
                if ($browserProbeRequest) {
                    throw "The browser probe listener received a delayed duplicate GET after the complete proof; expected exactly one authentic navigation request."
                }
                $browserProbeRequest = $candidateRequest
            }

            $newExternalProcesses = @(Get-NewDisallowedProcesses `
                -Names $disallowedExternalProcessNames `
                -BaselineProcessIds $baselineDisallowedProcessIds `
                -RootProcessIds @([int]$gameProcess.Id))
            if ($newExternalProcesses.Count -ne 0) {
                throw "A disallowed external browser or TCG client descendant appeared during the browser-probe quiescence audit."
            }
            if ([DateTime]::UtcNow -ge $browserProbeQuiescenceDeadline) {
                break
            }
            Start-Sleep -Milliseconds 50
        }
        if ($browserProbeRequestCount -ne 1) {
            throw "The browser probe did not remain at exactly one GET through the final quiescence audit."
        }
    }

    if ($Architecture -eq "x64") {
        try {
            $unexpectedGameModules = @(
                (Get-Process -Id $gameProcess.Id -ErrorAction Stop).Modules |
                    Where-Object { $_.ModuleName -ieq "SWGTCG.dll" }
            )
        }
        catch {
            throw "Could not verify that the x64 game process remained free of the x86 SWGTCG.dll: $($_.Exception.Message)"
        }
        if ($unexpectedGameModules.Count -ne 0) {
            throw "The x64 game process unexpectedly loaded the x86 SWGTCG.dll instead of isolating it in the compatibility host."
        }
    }

    $newExternalProcesses = @(Get-NewDisallowedProcesses `
        -Names $disallowedExternalProcessNames `
        -BaselineProcessIds $baselineDisallowedProcessIds `
        -RootProcessIds @([int]$gameProcess.Id))
    if ($newExternalProcesses.Count -ne 0) {
        throw "A disallowed external browser or TCG client process appeared during the embedded integration test."
    }
    $emptyProcessBaseline = [Collections.Generic.HashSet[int]]::new()
    $ownedCompatibilityHosts = @(Get-NewDisallowedProcesses `
        -Names @("TcgCompatibilityHost.exe") `
        -BaselineProcessIds $emptyProcessBaseline `
        -RootProcessIds @([int]$gameProcess.Id))
    if ($Architecture -eq "Win32" -and $ownedCompatibilityHosts.Count -ne 0) {
        throw "A compatibility-host descendant appeared during the Win32 in-process baseline test."
    }
    if ($Architecture -eq "x64") {
        if ($ownedCompatibilityHosts.Count -ne 1 -or
            [int]$ownedCompatibilityHosts[0].ProcessId -ne [int]$hostProcess.pid -or
            -not $ownedCompatibilityHosts[0].ExecutablePath -or
            -not (Get-NormalizedPath $ownedCompatibilityHosts[0].ExecutablePath).Equals(
                $compatibilityHost, [StringComparison]::OrdinalIgnoreCase)) {
            throw "The x64 test did not retain exactly one allowlisted compatibility-host descendant from the staged runtime."
        }
    }
    if ($GameActionBrowserProbe) {
        $ownedBrowserCompatibilityHosts = @(Get-NewDisallowedProcesses `
            -Names @("BrowserCompatibilityHost.exe") `
            -BaselineProcessIds $emptyProcessBaseline `
            -RootProcessIds @([int]$gameProcess.Id))
        if ($ownedBrowserCompatibilityHosts.Count -ne 1 -or
            [int]$ownedBrowserCompatibilityHosts[0].ProcessId -ne [int]$browserHostProcess.pid -or
            -not $ownedBrowserCompatibilityHosts[0].ExecutablePath -or
            -not (Get-NormalizedPath $ownedBrowserCompatibilityHosts[0].ExecutablePath).Equals(
                $browserCompatibilityHost, [StringComparison]::OrdinalIgnoreCase)) {
            throw "The browser probe did not retain exactly one allowlisted browser-host descendant from the staged runtime."
        }
    }

    $safeClientLines = @(
        @(
            $buttonPressLine,
            $inputMapLine,
            $inputExecuteLine,
            $actionDispatchLine,
            $messageQueueLine,
            $uiParserLine,
            $hudHandlerLine,
            $embeddedLaunchLine,
            $mediatorActivationLine,
            $actionResultLine,
            $surfaceLine,
            $eulaInputLine,
            $embeddedInputLine,
            $viewMoreInputLine,
            $authenticCallbackLine,
            $browserPageLine,
            $browserProxyWindowLine,
            $browserFirstFrameLine
        ) |
            Where-Object { $_ } |
            ForEach-Object { Protect-EvidenceText -Text $_ -Secrets $redactionSecrets.ToArray() }
    )
    $safeServerLines = @(
        @($gatewayLine, $lobbyLine) + $homeLines |
            Where-Object { $_ } |
            ForEach-Object { Protect-EvidenceText -Text $_ -Secrets $redactionSecrets.ToArray() }
    )

    if (-not $KeepProcess) {
        Stop-AllowlistedProcess -ProcessId $gameProcess.Id -AllowedExecutablePath $gameExecutable
        $gameProcess = $null
        if ($Architecture -eq "x64") {
            $hostExitDeadline = [DateTime]::UtcNow.AddSeconds(15)
            while (Get-Process -Id $hostProcess.pid -ErrorAction SilentlyContinue) {
                if ([DateTime]::UtcNow -ge $hostExitDeadline) {
                    throw "The game-owned compatibility host did not exit after SwgClient stopped."
                }
                Start-Sleep -Milliseconds 100
            }
            $tcgHostExitedWithGame = $true
        }
        if ($GameActionBrowserProbe) {
            $browserHostExitDeadline = [DateTime]::UtcNow.AddSeconds(15)
            while (Get-Process -Id $browserHostProcess.pid -ErrorAction SilentlyContinue) {
                if ([DateTime]::UtcNow -ge $browserHostExitDeadline) {
                    throw "The game-owned browser compatibility host did not exit after SwgClient stopped."
                }
                Start-Sleep -Milliseconds 100
            }
            $browserHostExitedWithGame = $true
        }
    }

    if (Test-Path -LiteralPath $clientLog -PathType Leaf) {
        $persistedClientLog = [IO.File]::ReadAllText($clientLog)
        if ($persistedClientLog -match '(?i)(^|\s)loginClientPassword\s*=') {
            throw "The SWG auto-login password parameter was persisted in the client log."
        }
        foreach ($secret in $redactionSecrets) {
            $secretText = [string]$secret
            if ($secretText.Length -ge 8 -and
                $persistedClientLog.IndexOf($secretText, [StringComparison]::Ordinal) -ge 0) {
                throw "A disposable TCG bearer credential was persisted in the client log."
            }
        }
        $clientLogSha256 = (Get-FileHash -LiteralPath $clientLog -Algorithm SHA256).Hash.ToLowerInvariant()
        $persistedClientLog = $null
    }
    else {
        throw "The client warning log disappeared before its evidence hash could be recorded."
    }

    foreach ($devKitLog in @($serverLog, $serverErrorLog)) {
        if (-not (Test-Path -LiteralPath $devKitLog -PathType Leaf)) {
            continue
        }
        # The maintained dev-kit service keeps its redirected log handle open;
        # Get-Content reads it with compatible sharing while the service runs.
        $persistedDevKitLog = Get-Content -LiteralPath $devKitLog -Raw -ErrorAction Stop
        if ($null -eq $persistedDevKitLog) {
            $persistedDevKitLog = [string]::Empty
        }
        foreach ($secret in $redactionSecrets) {
            $secretText = [string]$secret
            if ($secretText.Length -ge 8 -and
                $persistedDevKitLog.IndexOf($secretText, [StringComparison]::Ordinal) -ge 0) {
                throw "A disposable bearer credential was persisted in a dev-kit log."
            }
        }
        if ($persistedDevKitLog -match '(?im)(?:FRAME\s+[0-9]+\s+bytes:|RECV[^\r\n]*:)\s*(?:[0-9a-f]{2}\s+){4}[0-9a-f]{2}') {
            throw "A dev-kit log persisted raw wire-frame hexadecimal data."
        }
        $persistedDevKitLog = $null
    }

    $referenceManifestAfter = Get-ReferenceManifest -Root $protectedTcgRoot
    Assert-ReferenceManifestEqual -Before $referenceManifestBefore -After $referenceManifestAfter

    $pendingEvidence = [ordered]@{
        schema = 1
        passed = $true
        test = if ($GameActionBrowserProbe) {
            "tcg-x64-hud-button-in-game-browser-probe"
        }
        elseif ($Architecture -eq "x64") {
            "tcg-x64-bridge-embedded-in-swg-client"
        }
        else {
            "tcg-embedded-in-swg-client"
        }
        architecture = $Architecture
        trigger = $Trigger
        auto_login_local_server = [bool]$AutoLoginLocalServer
        local_station_id = if ($AutoLoginLocalServer) { $LocalStationId } else { $null }
        tested_at_utc = [DateTime]::UtcNow.ToString("o")
        nonce = $nonce
        disposable_username = $username
        swg_client = [ordered]@{
            pid = $gameProcessId
            path = $gameExecutable
            pe_machine = if ($Architecture -eq "x64") { "0x8664" } else { "0x014c" }
            sha256 = (Get-FileHash -LiteralPath $gameExecutable -Algorithm SHA256).Hash.ToLowerInvariant()
            loaded_swg_tcg_module = $Architecture -ne "x64"
            login_password_on_command_line = $false
            login_password_persisted = $false
            tcg_bearer_credentials_persisted = $false
        }
        compatibility_host = if ($Architecture -eq "x64") {
            [ordered]@{
                pid = [int]$hostProcess.pid
                parent_pid = [int]$hostProcess.parent_pid
                path = $hostProcess.path
                pe_machine = "0x014c"
                sha256 = (Get-FileHash -LiteralPath $compatibilityHost -Algorithm SHA256).Hash.ToLowerInvariant()
                credentials_on_command_line = $false
                # This host rejects SWGTCG_TEST_* before entering runHost; the required ready surface proves the check ran.
                forbidden_test_environment_present = $false
                exited_with_game = $tcgHostExitedWithGame
            }
        }
        else {
            $null
        }
        loaded_module = [ordered]@{
            name = $loadedModule.name
            path = $loadedModule.path
            module_memory_size = $loadedModule.module_memory_size
            sha256 = (Get-FileHash -LiteralPath $tcgDll -Algorithm SHA256).Hash.ToLowerInvariant()
            owner_pid = $lobbyConnection.owning_process
        }
        embedded_surface = [ordered]@{
            windows = $surface.windows
            width = $surface.width
            height = $surface.height
            stride = $surface.stride
        }
        runtime_configuration = [ordered]@{
            stage_manifest_path = $localDevKitRuntimeManifest
            stage_manifest_sha256 = (Get-FileHash -LiteralPath $localDevKitRuntimeManifest -Algorithm SHA256).Hash.ToLowerInvariant()
            host_svr_path = $hostConfig
            host_svr_sha256 = $hostConfigHash
            login_cfg_sha256 = $loginConfigHash
            tcg_gateway = "127.0.0.1:16782"
            swg_login = "127.0.0.1:44453"
        }
        in_game_entry_point = if ($Trigger -eq "GameAction") {
            [ordered]@{
                control = "buttonTcg"
                command = "CMD_uiTcg"
                visible = $true
                enabled = $true
                pressed = $true
            }
        }
        else {
            $null
        }
        browser_probe = if ($GameActionBrowserProbe) {
            [ordered]@{
                enabled = $true
                listener_address = "127.0.0.1"
                listener_port = [int]$browserProbeListenerState.port
                exact_get_count = [int]$browserProbeRequestCount
                duplicate_get_quiescence_seconds = 2
                duplicate_get_detected = $false
                request_method = $browserProbeRequest.method
                exact_path = [bool]$browserProbeRequest.exact_path
                request_target_sha256 = $browserProbeRequest.request_target_sha256
                expected_target_sha256 = (Get-TextSha256 -Text $expectedBrowserProbePath)
                raw_url_persisted = $false
                raw_post_persisted = $false
                external_browser_or_client_created = $false
                frame = [ordered]@{
                    width = $browserFrame.width
                    height = $browserFrame.height
                    first_nonzero = $true
                }
                compatibility_host = [ordered]@{
                    pid = [int]$browserHostProcess.pid
                    parent_pid = [int]$browserHostProcess.parent_pid
                    path = $browserHostProcess.path
                    pe_machine = "0x014c"
                    sha256 = (Get-FileHash -LiteralPath $browserCompatibilityHost -Algorithm SHA256).Hash.ToLowerInvariant()
                    credentials_on_command_line = $false
                    # This host rejects SWGTCG_TEST_* before Mozilla initialization; the required XUL surface proves the check ran.
                    forbidden_test_environment_present = $false
                    navigation_on_command_line = $false
                    exited_with_game = $browserHostExitedWithGame
                }
                xul_module = [ordered]@{
                    name = $browserXulModule.name
                    path = $browserXulModule.path
                    module_memory_size = $browserXulModule.module_memory_size
                    pe_machine = "0x014c"
                    sha256 = (Get-FileHash -LiteralPath $browserXul -Algorithm SHA256).Hash.ToLowerInvariant()
                }
            }
        }
        else {
            $null
        }
        lobby_connection = [ordered]@{
            owning_process = $lobbyConnection.owning_process
            local_address = $lobbyConnection.local_address
            local_port = $lobbyConnection.local_port
            remote_address = $lobbyConnection.remote_address
            remote_port = $lobbyConnection.remote_port
            state = $lobbyConnection.state
        }
        devkit = [ordered]@{
            gateway_connection_id = [int]$gatewayConnectionId
            lobby_connection_id = [int]$lobbyConnectionId
            account_id = $accountId
            bearer_credentials_persisted_in_logs = $false
            raw_wire_frames_persisted_in_logs = $false
            server_log = $safeServerLines
        }
        client_log = $safeClientLines
        client_log_file = [ordered]@{
            path = $clientLog
            sha256 = $clientLogSha256
            selected_lines_sha256 = Get-TextSha256 -Text ($safeClientLines -join "`n")
        }
        protected_reference = [ordered]@{
            path = $protectedTcgRoot
            unchanged = $true
            before_sha256 = $referenceManifestBefore.digest
            after_sha256 = $referenceManifestAfter.digest
            file_count = $referenceManifestAfter.file_count
            directory_count = $referenceManifestAfter.directory_count
            total_bytes = $referenceManifestAfter.total_bytes
        }
    }
}
catch {
    $primaryError = $_
}
finally {
    if ($environmentApplied) {
        Restore-TestEnvironment -Snapshot $environmentSnapshot
        $environmentApplied = $false
    }

    if (-not $KeepProcess -and $gameProcess) {
        try {
            Stop-AllowlistedProcess -ProcessId $gameProcess.Id -AllowedExecutablePath $gameExecutable
        }
        catch {
            if (-not $primaryError) {
                $primaryError = $_
            }
        }
    }
    if (-not $KeepProcess -and $hostProcess -and (Get-Process -Id $hostProcess.pid -ErrorAction SilentlyContinue)) {
        try {
            Stop-AllowlistedProcess -ProcessId $hostProcess.pid -AllowedExecutablePath $compatibilityHost
        }
        catch {
            if (-not $primaryError) {
                $primaryError = $_
            }
        }
    }
    if (-not $KeepProcess -and $browserHostProcess -and (Get-Process -Id $browserHostProcess.pid -ErrorAction SilentlyContinue)) {
        try {
            Stop-AllowlistedProcess -ProcessId $browserHostProcess.pid -AllowedExecutablePath $browserCompatibilityHost
        }
        catch {
            if (-not $primaryError) {
                $primaryError = $_
            }
        }
    }
    if ($browserProbeListenerState -and $browserProbeListenerState.listener) {
        try {
            $browserProbeListenerState.listener.Stop()
        }
        catch {
            if (-not $primaryError) {
                $primaryError = $_
            }
        }
    }

    try {
        $finalReferenceManifest = Get-ReferenceManifest -Root $protectedTcgRoot
        Assert-ReferenceManifestEqual -Before $referenceManifestBefore -After $finalReferenceManifest
        if (-not $referenceManifestAfter) {
            $referenceManifestAfter = $finalReferenceManifest
        }
    }
    catch {
        $integrityError = $_
    }
}

try {
    if ($integrityError) {
        throw $integrityError
    }
    if ($primaryError) {
        throw $primaryError
    }

    $evidenceRoot = Get-NormalizedPath (Join-Path $runtimeRootPath "test-evidence")
    if (-not (Test-PathWithin -Path $evidenceRoot -Parent $outputRoot)) {
        throw "Evidence output resolved outside _whitengold_client."
    }
    [IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
    $evidencePath = Join-Path $evidenceRoot "embedded_$nonce.json"
    $evidenceJson = ($pendingEvidence | ConvertTo-Json -Depth 8) + [Environment]::NewLine
    $evidenceJson = Protect-EvidenceText -Text $evidenceJson -Secrets $redactionSecrets.ToArray()
    foreach ($secret in $redactionSecrets) {
        if (-not [string]::IsNullOrEmpty($secret) -and
            $evidenceJson.IndexOf($secret, [StringComparison]::Ordinal) -ge 0) {
            throw "Evidence redaction failed; refusing to persist the result."
        }
    }
    [IO.File]::WriteAllText($evidencePath, $evidenceJson, [Text.UTF8Encoding]::new($false))

    if ($Architecture -eq "x64") {
        if ($GameActionBrowserProbe) {
            Write-Host "PASS: the visible buttonTcg/CMD_uiTcg control launched SWGTCG, whose authentic navigation callback opened the in-game browser, completed the exact loopback GET, rendered a nonzero browser frame, and both compatibility hosts exited with SwgClient."
        }
        elseif ($Trigger -eq "GameAction") {
            Write-Host "PASS: the live in-game HUD TCG action activated the embedded mediator, launched the allowlisted Win32 compatibility child, and reached the local dev-kit Home screen."
        }
        else {
            Write-Host "PASS: x64 SwgClient launched its allowlisted Win32 compatibility child; SWGTCG.dll rendered a shared embedded surface and the child connected to the local dev-kit lobby and reached Home."
        }
    }
    else {
        Write-Host "PASS: SWGTCG.dll loaded and rendered inside Win32 SwgClient, whose PID connected to the local dev-kit lobby and reached Home."
    }
    Write-Host "Evidence: $evidencePath"
}
finally {
    $sessionId = $null
    $challenge = $null
    $characterId = $null
    $password = $null
    $environmentSnapshot.Clear()
    $environmentSnapshot = $null
    $localSwgLoginPassword = $null
    $redactionSecrets.Clear()
    $redactionSecrets = $null
}
