<#
.SYNOPSIS
Sends opt-in test input to a Pre-CU SWG client without activating its window.

.DESCRIPTION
The client must be built with the Pre-CU background input bridge and started with
enableBackgroundInputBridge=true in the [SwgClient] configuration section. The
bridge is disabled by default. This helper never moves the Windows cursor, sends
system-wide input, or changes the foreground window.

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Ping

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action LeftClick -X 512 -Y 384

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Key -KeyName Enter

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Chord -DikCode 31 -ModifierDikCode 29

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Text -Text "hello"
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Ping", "Move", "LeftClick", "RightClick", "MiddleClick", "Key", "Chord", "Text", "Reset")]
    [string]$Action,

    [ValidateRange(1, [int]::MaxValue)]
    [int]$ClientProcessId,

    [ValidateRange(0, 32767)]
    [int]$X,

    [ValidateRange(0, 32767)]
    [int]$Y,

    [ValidateSet("Escape", "Backspace", "Tab", "Enter", "Space", "Home", "Up", "PageUp", "Left", "Right", "End", "Down", "PageDown", "Delete")]
    [string]$KeyName,

    [ValidateRange(0, 255)]
    [int]$DikCode,

    [ValidateRange(0, 255)]
    [int[]]$ModifierDikCode,

    [AllowEmptyString()]
    [string]$Text
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$clientProcessIdWasSpecified = $PSBoundParameters.ContainsKey("ClientProcessId")

$messageName = "SWGSource.PreCU.BackgroundInput.v1"
$expectedProtocolVersion = 1
$command = @{
    Ping            = 0
    MouseMove       = 1
    LeftMouseDown   = 2
    LeftMouseUp     = 3
    RightMouseDown  = 4
    RightMouseUp    = 5
    MiddleMouseDown = 6
    MiddleMouseUp   = 7
    KeyDown         = 8
    KeyUp           = 9
    Character       = 10
    InputReset      = 11
}
$dikByName = @{
    Escape    = 0x01
    Backspace = 0x0e
    Tab       = 0x0f
    Enter     = 0x1c
    Space     = 0x39
    Home      = 0xc7
    Up        = 0xc8
    PageUp    = 0xc9
    Left      = 0xcb
    Right     = 0xcd
    End       = 0xcf
    Down      = 0xd0
    PageDown  = 0xd1
    Delete    = 0xd3
}

if (-not ("PrecuBackgroundInput.NativeMethods" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace PrecuBackgroundInput
{
    public static class NativeMethods
    {
        [DllImport("user32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
        public static extern uint RegisterWindowMessage(string messageName);

        [DllImport("user32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool PostMessage(
            IntPtr window,
            uint message,
            IntPtr wParam,
            IntPtr lParam);

        [DllImport("user32.dll", SetLastError = true)]
        public static extern IntPtr SendMessageTimeout(
            IntPtr window,
            uint message,
            IntPtr wParam,
            IntPtr lParam,
            uint flags,
            uint timeoutMilliseconds,
            out IntPtr result);
    }
}
"@
}

function Resolve-ClientWindow {
    if ($clientProcessIdWasSpecified) {
        $client = Get-Process -Id $ClientProcessId -ErrorAction Stop
        if ($client.MainWindowHandle -eq [IntPtr]::Zero) {
            throw "Process $ClientProcessId does not have a main window."
        }
        return $client
    }

    $clients = @(
        Get-Process | Where-Object {
            $_.ProcessName -match '^SwgClient(?:_[dor])?$' -and
            $_.MainWindowHandle -ne [IntPtr]::Zero
        }
    )
    if ($clients.Count -eq 0) {
        throw "No running SwgClient window was found. Use -ClientProcessId to select one explicitly."
    }
    if ($clients.Count -gt 1) {
        $ids = ($clients.Id | Sort-Object) -join ", "
        throw "Multiple SwgClient windows were found ($ids). Use -ClientProcessId to select one explicitly."
    }

    return $clients[0]
}

function ConvertTo-PointLParam {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ClientX,

        [Parameter(Mandatory = $true)]
        [int]$ClientY
    )

    [long]$packedPoint = ([long]$ClientY * 0x10000L) + [long]$ClientX
    return [IntPtr]::new($packedPoint)
}

function Send-BridgeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Window,

        [Parameter(Mandatory = $true)]
        [uint32]$Message,

        [Parameter(Mandatory = $true)]
        [int]$Command,

        [long]$Data = 0
    )

    $posted = [PrecuBackgroundInput.NativeMethods]::PostMessage(
        $Window,
        $Message,
        [IntPtr]::new([long]$Command),
        [IntPtr]::new($Data))
    if (-not $posted) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw [ComponentModel.Win32Exception]::new($errorCode, "Could not post the background input command.")
    }
}

function Test-BridgeProtocol {
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Window,

        [Parameter(Mandatory = $true)]
        [uint32]$Message
    )

    [IntPtr]$protocolResult = [IntPtr]::Zero
    $sendResult = [PrecuBackgroundInput.NativeMethods]::SendMessageTimeout(
        $Window,
        $Message,
        [IntPtr]::new([long]$command.Ping),
        [IntPtr]::Zero,
        0x0002,
        1000,
        [ref]$protocolResult)
    if ($sendResult -eq [IntPtr]::Zero) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw [ComponentModel.Win32Exception]::new($errorCode, "The client did not answer the background input bridge ping.")
    }

    $protocolVersion = $protocolResult.ToInt64()
    if ($protocolVersion -ne $expectedProtocolVersion) {
        throw "Unexpected background input protocol version $protocolVersion (expected $expectedProtocolVersion)."
    }

    return $protocolVersion
}

function Send-BridgeKeySequence {
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Window,

        [Parameter(Mandatory = $true)]
        [uint32]$Message,

        [Parameter(Mandatory = $true)]
        [ValidateRange(0, 255)]
        [int]$KeyCode,

        [ValidateRange(0, 255)]
        [int[]]$Modifiers = @()
    )

    $pressedModifiers = [System.Collections.Generic.List[int]]::new()
    $keyIsDown = $false
    $actionError = $null
    $cleanupErrors = [System.Collections.Generic.List[object]]::new()

    try {
        foreach ($modifier in $Modifiers) {
            Send-BridgeCommand -Window $Window -Message $Message -Command $command.KeyDown -Data $modifier
            $pressedModifiers.Add($modifier)
        }

        Send-BridgeCommand -Window $Window -Message $Message -Command $command.KeyDown -Data $KeyCode
        $keyIsDown = $true
        Send-BridgeCommand -Window $Window -Message $Message -Command $command.KeyUp -Data $KeyCode
        $keyIsDown = $false
    }
    catch {
        $actionError = $_
    }
    finally {
        if ($keyIsDown) {
            try {
                Send-BridgeCommand -Window $Window -Message $Message -Command $command.KeyUp -Data $KeyCode
            }
            catch {
                [void]$cleanupErrors.Add($_)
            }
        }

        for ($index = $pressedModifiers.Count - 1; $index -ge 0; --$index) {
            try {
                Send-BridgeCommand -Window $Window -Message $Message -Command $command.KeyUp -Data $pressedModifiers[$index]
            }
            catch {
                [void]$cleanupErrors.Add($_)
            }
        }

        if ($actionError -or $cleanupErrors.Count -gt 0) {
            try {
                Send-BridgeCommand -Window $Window -Message $Message -Command $command.InputReset
            }
            catch {
                [void]$cleanupErrors.Add($_)
            }
        }
    }

    if ($actionError) {
        throw $actionError
    }
    if ($cleanupErrors.Count -gt 0) {
        throw $cleanupErrors[0]
    }
}

$client = Resolve-ClientWindow
$window = [IntPtr]$client.MainWindowHandle
$message = [PrecuBackgroundInput.NativeMethods]::RegisterWindowMessage($messageName)
if ($message -eq 0) {
    $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw [ComponentModel.Win32Exception]::new($errorCode, "Could not register the background input message.")
}

$protocolVersion = Test-BridgeProtocol -Window $window -Message $message
$detail = $null

switch ($Action) {
    "Ping" {
        $detail = "protocol=$protocolVersion"
    }

    "Move" {
        if (-not ($PSBoundParameters.ContainsKey("X") -and $PSBoundParameters.ContainsKey("Y"))) {
            throw "Move requires -X and -Y client coordinates."
        }
        $point = ConvertTo-PointLParam -ClientX $X -ClientY $Y
        Send-BridgeCommand -Window $window -Message $message -Command $command.MouseMove -Data $point.ToInt64()
        $detail = "x=$X y=$Y"
    }

    { $_ -in @("LeftClick", "RightClick", "MiddleClick") } {
        if (-not ($PSBoundParameters.ContainsKey("X") -and $PSBoundParameters.ContainsKey("Y"))) {
            throw "$Action requires -X and -Y client coordinates."
        }
        $point = ConvertTo-PointLParam -ClientX $X -ClientY $Y
        $buttonCommands = switch ($Action) {
            "LeftClick"   { @($command.LeftMouseDown, $command.LeftMouseUp) }
            "RightClick"  { @($command.RightMouseDown, $command.RightMouseUp) }
            "MiddleClick" { @($command.MiddleMouseDown, $command.MiddleMouseUp) }
        }
        Send-BridgeCommand -Window $window -Message $message -Command $command.MouseMove -Data $point.ToInt64()
        Send-BridgeCommand -Window $window -Message $message -Command $buttonCommands[0] -Data $point.ToInt64()
        Send-BridgeCommand -Window $window -Message $message -Command $buttonCommands[1] -Data $point.ToInt64()
        $detail = "x=$X y=$Y"
    }

    { $_ -in @("Key", "Chord") } {
        $hasKeyName = $PSBoundParameters.ContainsKey("KeyName")
        $hasDikCode = $PSBoundParameters.ContainsKey("DikCode")
        if ($hasKeyName -eq $hasDikCode) {
            throw "$Action requires exactly one of -KeyName or -DikCode."
        }
        $keyCode = if ($hasDikCode) { $DikCode } else { $dikByName[$KeyName] }

        $hasModifiers = $PSBoundParameters.ContainsKey("ModifierDikCode") -and
            @($ModifierDikCode).Count -gt 0
        if ($Action -eq "Chord" -and -not $hasModifiers) {
            throw "Chord requires at least one -ModifierDikCode."
        }
        if ($Action -eq "Key" -and $hasModifiers) {
            throw "ModifierDikCode is valid only with -Action Chord."
        }

        [int[]]$modifiers = if ($hasModifiers) { @($ModifierDikCode) } else { @() }
        Send-BridgeKeySequence -Window $window -Message $message -KeyCode $keyCode -Modifiers $modifiers
        $modifierDetail = if (@($modifiers).Count -gt 0) { $modifiers -join "," } else { "none" }
        $detail = "queued dik=$keyCode modifiers=$modifierDetail"
    }

    "Text" {
        if (-not $PSBoundParameters.ContainsKey("Text")) {
            throw "Text requires -Text (an empty string is permitted)."
        }
        foreach ($character in $Text.ToCharArray()) {
            $characterCode = [int]$character
            if ($characterCode -eq 0) {
                throw "Text cannot contain a null character."
            }
            Send-BridgeCommand -Window $window -Message $message -Command $command.Character -Data $characterCode
        }
        $detail = "characters=$($Text.Length)"
    }

    "Reset" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.InputReset
        $detail = "queued"
    }
}

[pscustomobject]@{
    Action          = $Action
    ProcessId       = $client.Id
    WindowHandle    = ('0x{0:x}' -f $window.ToInt64())
    ProtocolVersion = $protocolVersion
    Detail          = $detail
}
