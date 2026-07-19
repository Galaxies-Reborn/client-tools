<#
.SYNOPSIS
Queues opt-in, window-targeted test input for a Pre-CU SWG client.

.DESCRIPTION
The client must be built with the Pre-CU background input bridge and started with
enableBackgroundInputBridge=true in the [SwgClient] configuration section. The
bridge is disabled by default. This helper does not move the Windows cursor, send
system-wide input, or request foreground activation. Successful actions are only
queued; processing is asynchronous and depends on the client's current UI state.
Text and Enter are refused unless the target client is already foreground because
they can select or submit stateful UI text input.

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Ping

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action LeftClick -X 512 -Y 384

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Key -KeyName Enter

Queues Enter only when the target client is already the foreground window.

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Chord -DikCode 31 -ModifierDikCode 29

.EXAMPLE
.\scripts\Invoke-PrecuBackgroundInput.ps1 -Action Text -Text "hello"

Queues text only when the target client is already the foreground window.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Ping", "Move", "LeftClick", "RightClick", "MiddleClick", "Key", "Chord", "Text", "Reset", "ExamineCharacterSheet", "InviteTarget", "JoinGroup", "DisbandGroup", "OpenStatMigration", "StartImageDesign", "TargetCounterpart", "QueueCombatCanary", "QueueBodyShot1", "QueueLegShot1", "QueueDurationControl", "QueueHealWound", "QueueHealDamage", "QueueTendDamage", "QueueTendWound", "QueueDiagnose", "QueueMedicalForage", "QueueFirstAid", "QueueDragIncapacitatedPlayer", "QueueQuickHeal", "QueueHealState", "QueueCurePoison", "QueueHealEnhance", "QueueExtinguishFire", "QueueCureDisease", "QueueRevivePlayer", "QueueDeathBlow", "SelectCloneLocation", "StartDanceRhythmic", "FlourishOne", "StopDance", "StartMusicStarwars1", "StopMusic", "StartBandStarwars1", "BandFlourishOne", "StopBand", "StartMusicRock", "SurrenderEntertainerMusicOne", "ClearCombatQueue", "CombatQueueStatus", "CombatTimerStatus", "EquipCdefRifle", "EquipCdefPistol", "EquipCdefCarbine", "EquipFixtureLightsaber", "EquipFixtureFallbackSword", "Stand")]
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

    [ValidateRange(1, 16)]
    [int]$Repeat = 1,

    [ValidateRange(1, [long]::MaxValue)]
    [long]$TargetOid,

    [ValidateRange(0, 127)]
    [int]$SelectionIndex,

    [AllowEmptyString()]
    [string]$Text
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$clientProcessIdWasSpecified = $PSBoundParameters.ContainsKey("ClientProcessId")

$messageName = "SWGSource.PreCU.BackgroundInput.v1"
$expectedProtocolVersion = 35
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
    ExamineCharacterSheet = 12
    InviteTarget    = 13
    JoinGroup       = 14
    DisbandGroup    = 15
    OpenStatMigration = 16
    StartImageDesign = 17
    TargetCounterpart = 18
    QueueCombatCanary = 19
    ClearCombatQueue = 20
    CombatQueueStatus = 21
    EquipCdefRifle = 22
    Stand           = 23
    QueueBodyShot1  = 24
    QueueLegShot1   = 25
    EquipCdefPistol = 26
    EquipCdefCarbine = 27
    CombatTimerStatus = 28
    QueueDurationControl = 29
    EquipFixtureLightsaber = 30
    EquipFixtureFallbackSword = 31
    QueueHealWound = 32
    QueueHealDamage = 33
    QueueTendDamage = 34
    QueueTendWound = 35
    QueueDiagnose = 36
    QueueMedicalForage = 37
    QueueFirstAid = 38
    QueueDragIncapacitatedPlayer = 39
    QueueQuickHeal = 40
    QueueHealState = 41
    QueueCurePoison = 42
    QueueHealEnhance = 43
    QueueExtinguishFire = 44
    QueueCureDisease = 45
    QueueRevivePlayer = 46
    QueueDeathBlow = 47
    SelectCloneLocation = 48
    ConfirmCloneLocation = 49
    StartDanceRhythmic = 50
    FlourishOne = 51
    StopDance = 52
    StartMusicStarwars1 = 53
    StopMusic = 54
    StartBandStarwars1 = 55
    BandFlourishOne = 56
    StopBand = 57
    StartMusicRock = 58
    SurrenderEntertainerMusicOne = 59
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

        [DllImport("user32.dll")]
        public static extern IntPtr GetForegroundWindow();
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

function Assert-BridgeForegroundForUiStateAction {
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$TargetWindow,

        [Parameter(Mandatory = $true)]
        [string]$RequestedAction,

        [int]$ResolvedDikCode = -1,

        [scriptblock]$ForegroundWindowProvider
    )

    $requiresForeground = $RequestedAction -eq "Text" -or
        ($RequestedAction -in @("Key", "Chord") -and $ResolvedDikCode -eq 0x1c)
    if (-not $requiresForeground) {
        return
    }

    [IntPtr]$foregroundWindow = if ($null -ne $ForegroundWindowProvider) {
        & $ForegroundWindowProvider
    }
    else {
        [PrecuBackgroundInput.NativeMethods]::GetForegroundWindow()
    }

    if ($foregroundWindow -ne $TargetWindow) {
        $actionDescription = if ($RequestedAction -eq "Text") {
            "Text"
        }
        else {
            "$RequestedAction Enter (DIK 0x1c)"
        }
        throw "$actionDescription is UI-state dependent and is refused while the target client is not the foreground window."
    }
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

function Invoke-BridgeQuery {
    param(
        [Parameter(Mandatory = $true)]
        [IntPtr]$Window,

        [Parameter(Mandatory = $true)]
        [uint32]$Message,

        [Parameter(Mandatory = $true)]
        [int]$Command,

        [long]$Data = 0
    )

    [IntPtr]$queryResult = [IntPtr]::Zero
    $sendResult = [PrecuBackgroundInput.NativeMethods]::SendMessageTimeout(
        $Window,
        $Message,
        [IntPtr]::new([long]$Command),
        [IntPtr]::new($Data),
        0x0002,
        1000,
        [ref]$queryResult)
    if ($sendResult -eq [IntPtr]::Zero) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw [ComponentModel.Win32Exception]::new($errorCode, "The client did not answer the background input bridge query.")
    }

    return $queryResult.ToInt64()
}

function ConvertTo-CombatQueueStatusDetail {
    param(
        [Parameter(Mandatory = $true)]
        [long]$PackedStatus
    )

    if (($PackedStatus -band 0xffff0000L) -ne 0x43510000L) {
        throw ("Unexpected combat-queue status marker 0x{0:x8}." -f $PackedStatus)
    }
    $queueCount = $PackedStatus -band 0x3fffL
    $inCombat = ($PackedStatus -band 0x8000L) -ne 0
    $hasTarget = ($PackedStatus -band 0x4000L) -ne 0
    $hasLastResult = ($PackedStatus -band 0x0100000000000000L) -ne 0
    $lastResult = "lastStatus=none lastDetail=none"
    if ($hasLastResult) {
        $lastStatus = ($PackedStatus -shr 32) -band 0xffL
        $lastDetail = ($PackedStatus -shr 40) -band 0xffffL
        if ($lastDetail -ge 0x8000L) {
            $lastDetail -= 0x10000L
        }
        $statusNames = @("Success", "Locomotion", "Ability", "TargetType", "TargetRange", "StateMustNotHave", "StateMustHave", "GodLevel", "Cancelled")
        $lastStatusName = if ($lastStatus -lt $statusNames.Count) { $statusNames[$lastStatus] } else { "Unknown" }
        $lastResult = "lastStatus=$lastStatus($lastStatusName) lastDetail=$lastDetail"
    }
    return "count=$queueCount inCombat=$($inCombat.ToString().ToLowerInvariant()) hasTarget=$($hasTarget.ToString().ToLowerInvariant()) $lastResult packed=0x$($PackedStatus.ToString('x16'))"
}

function ConvertTo-CombatTimerStatusDetail {
    param(
        [Parameter(Mandatory = $true)]
        [long]$PackedStatus
    )

    if (($PackedStatus -band 0xffff0000L) -ne 0x544d0000L) {
        throw ("Unexpected combat-timer status marker 0x{0:x8}." -f $PackedStatus)
    }
    $valid = ($PackedStatus -band 0x100L) -ne 0
    if (-not $valid) {
        return "available=false packed=0x$($PackedStatus.ToString('x16'))"
    }

    $commandValue = $PackedStatus -band 0xffL
    $commandNames = @("none", "headShot1", "bodyShot1", "legShot1", "headShot2")
    $commandName = if ($commandValue -lt $commandNames.Count) {
        $commandNames[$commandValue]
    }
    else {
        "unknown"
    }
    $maxMilliseconds = ($PackedStatus -shr 32) -band 0xffffL
    $currentMilliseconds = ($PackedStatus -shr 48) -band 0xffffL
    return "available=true command=$commandName currentMs=$currentMilliseconds maxMs=$maxMilliseconds packed=0x$($PackedStatus.ToString('x16'))"
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

        [AllowEmptyCollection()]
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

        Assert-BridgeForegroundForUiStateAction `
            -TargetWindow $window `
            -RequestedAction $Action `
            -ResolvedDikCode $keyCode

        [int[]]$modifiers = if ($hasModifiers) { @($ModifierDikCode) } else { @() }
        if ($hasModifiers) {
            Send-BridgeKeySequence -Window $window -Message $message -KeyCode $keyCode -Modifiers $modifiers
        }
        else {
            Send-BridgeKeySequence -Window $window -Message $message -KeyCode $keyCode
        }
        $modifierDetail = if ($hasModifiers) { $modifiers -join "," } else { "none" }
        $detail = "queued dik=$keyCode modifiers=$modifierDetail"
    }

    "Text" {
        if (-not $PSBoundParameters.ContainsKey("Text")) {
            throw "Text requires -Text (an empty string is permitted)."
        }
        Assert-BridgeForegroundForUiStateAction -TargetWindow $window -RequestedAction $Action
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

    "ExamineCharacterSheet" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.ExamineCharacterSheet
        $detail = "queued for current targeted player"
    }

    "InviteTarget" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.InviteTarget
        $detail = "queued for current targeted player"
    }

    "JoinGroup" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.JoinGroup
        $detail = "queued"
    }

    "DisbandGroup" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.DisbandGroup
        $detail = "queued"
    }

    "OpenStatMigration" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.OpenStatMigration
        $detail = "queued"
    }

    "StartImageDesign" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.StartImageDesign
        $detail = "queued for current targeted player"
    }

    "TargetCounterpart" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.TargetCounterpart
        $detail = "queued for the bound live-test counterpart"
    }

    "QueueCombatCanary" {
        [long]$packedStatus = Invoke-BridgeQuery -Window $window -Message $message -Command $command.QueueCombatCanary -Data $Repeat
        $detail = "repeat=$Repeat $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    { $_ -in @("QueueBodyShot1", "QueueLegShot1") } {
        [long]$packedStatus = Invoke-BridgeQuery -Window $window -Message $message -Command $command[$Action] -Data $Repeat
        $detail = "command=$($Action.Replace('Queue', '')) repeat=$Repeat $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "ClearCombatQueue" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.ClearCombatQueue
        $detail = "queued through the production combat-queue clear action"
    }

    "CombatQueueStatus" {
        [long]$packedStatus = Invoke-BridgeQuery -Window $window -Message $message -Command $command.CombatQueueStatus
        $detail = ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus
    }

    "CombatTimerStatus" {
        [long]$packedStatus = Invoke-BridgeQuery -Window $window -Message $message -Command $command.CombatTimerStatus
        $detail = ConvertTo-CombatTimerStatusDetail -PackedStatus $packedStatus
    }

    "QueueDurationControl" {
        [long]$packedStatus = Invoke-BridgeQuery -Window $window -Message $message -Command $command.QueueDurationControl
        $detail = "command=headShot2 $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueHealWound" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueHealWound requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueHealWound `
            -Data $TargetOid
        $detail = "target=$TargetOid $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueHealDamage" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueHealDamage requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueHealDamage `
            -Data $TargetOid
        $detail = "target=$TargetOid $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueTendDamage" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueTendDamage requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueTendDamage `
            -Data $TargetOid
        $detail = "target=$TargetOid $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueTendWound" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueTendWound requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueTendWound `
            -Data $TargetOid
        $detail = "target=$TargetOid $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueDiagnose" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueDiagnose requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueDiagnose `
            -Data $TargetOid
        $detail = "target=$TargetOid $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueMedicalForage" {
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueMedicalForage
        $detail = "command=medicalForage $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueFirstAid" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueFirstAid requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueFirstAid `
            -Data $TargetOid
        $detail = "target=$TargetOid command=firstAid $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueDragIncapacitatedPlayer" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueDragIncapacitatedPlayer requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueDragIncapacitatedPlayer `
            -Data $TargetOid
        $detail = "target=$TargetOid command=dragIncapacitatedPlayer $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueQuickHeal" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueQuickHeal requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueQuickHeal `
            -Data $TargetOid
        $detail = "target=$TargetOid command=quickHeal $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueHealState" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueHealState requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueHealState `
            -Data $TargetOid
        $detail = "target=$TargetOid command=healState $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueCurePoison" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueCurePoison requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueCurePoison `
            -Data $TargetOid
        $detail = "target=$TargetOid command=curePoison $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueHealEnhance" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueHealEnhance requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueHealEnhance `
            -Data $TargetOid
        $detail = "target=$TargetOid command=healEnhance $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueExtinguishFire" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueExtinguishFire requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueExtinguishFire `
            -Data $TargetOid
        $detail = "target=$TargetOid command=extinguishFire $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueCureDisease" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueCureDisease requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueCureDisease `
            -Data $TargetOid
        $detail = "target=$TargetOid command=cureDisease $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueRevivePlayer" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueRevivePlayer requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueRevivePlayer `
            -Data $TargetOid
        $detail = "target=$TargetOid command=revivePlayer $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "QueueDeathBlow" {
        if (-not $PSBoundParameters.ContainsKey("TargetOid")) {
            throw "QueueDeathBlow requires -TargetOid."
        }
        [long]$packedStatus = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.QueueDeathBlow `
            -Data $TargetOid
        $detail = "target=$TargetOid command=deathBlow $(ConvertTo-CombatQueueStatusDetail -PackedStatus $packedStatus)"
    }

    "SelectCloneLocation" {
        if (-not $PSBoundParameters.ContainsKey("SelectionIndex")) {
            throw "SelectCloneLocation requires -SelectionIndex."
        }
        [long]$selected = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.SelectCloneLocation `
            -Data $SelectionIndex
        if ($selected -ne 1) {
            throw ("The identity-bound dead client did not expose exactly one selectable clone list for row " +
                "$SelectionIndex (selectionResult=0x{0:x})." -f $selected)
        }
        [long]$confirmed = 0
        for ($attempt = 0; $attempt -lt 20; $attempt++) {
            Start-Sleep -Milliseconds 250
            $confirmed = Invoke-BridgeQuery `
                -Window $window `
                -Message $message `
                -Command $command.ConfirmCloneLocation `
                -Data $SelectionIndex
            if ($confirmed -eq 1) {
                break
            }
            if (($confirmed -band 0xf0000) -ne 0x50000) {
                break
            }
        }
        if ($confirmed -ne 1) {
            throw ("The identity-bound clone list did not receive the server-authored row $SelectionIndex " +
                "prompt update or retain that row for confirmation " +
                "(confirmationResult=0x{0:x})." -f $confirmed)
        }
        $detail = "selectionIndex=$SelectionIndex realSuiSelectionRoundTripAndOk=true"
    }

    { $_ -in @("StartDanceRhythmic", "FlourishOne", "StopDance", "StartMusicStarwars1", "StopMusic", "StartBandStarwars1", "BandFlourishOne", "StopBand", "StartMusicRock") } {
        [long]$accepted = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command[$Action]
        if ($accepted -ne 1) {
            throw "The identity-bound client did not submit $Action through the production command queue."
        }
        $productionCommand = @{
            StartDanceRhythmic = "startDance rhythmic"
            FlourishOne = "flourish 1"
            StopDance = "stopDance"
            StartMusicStarwars1 = "startMusic starwars1"
            StopMusic = "stopMusic"
            StartBandStarwars1 = "startBand starwars1"
            BandFlourishOne = "bandFlourish 1"
            StopBand = "stopBand"
            StartMusicRock = "startMusic rock"
        }[$Action]
        $detail = "submitted=true productionCommand=$productionCommand"
    }

    "SurrenderEntertainerMusicOne" {
        [long]$accepted = Invoke-BridgeQuery `
            -Window $window `
            -Message $message `
            -Command $command.SurrenderEntertainerMusicOne
        if ($accepted -ne 1) {
            throw "The identity-bound client did not queue the production Music I surrender."
        }
        $detail = "submitted=true productionCommand=surrenderSkill skill=social_entertainer_music_01"
    }

    "EquipCdefRifle" {
        [long]$equipResult = Invoke-BridgeQuery -Window $window -Message $message -Command $command.EquipCdefRifle
        $detail = "equipAccepted=$([bool]$equipResult) result=$equipResult for the bound combat fixture player"
    }

    { $_ -in @("EquipCdefPistol", "EquipCdefCarbine", "EquipFixtureLightsaber", "EquipFixtureFallbackSword") } {
        [long]$equipResult = Invoke-BridgeQuery -Window $window -Message $message -Command $command[$Action]
        $detail = "equipAccepted=$([bool]$equipResult) result=$equipResult for the bound combat fixture player"
    }

    "Stand" {
        Send-BridgeCommand -Window $window -Message $message -Command $command.Stand
        $detail = "queued through the production stand command"
    }
}

[pscustomobject]@{
    Action          = $Action
    ProcessId       = $client.Id
    WindowHandle    = ('0x{0:x}' -f $window.ToInt64())
    ProtocolVersion = $protocolVersion
    Detail          = $detail
}
