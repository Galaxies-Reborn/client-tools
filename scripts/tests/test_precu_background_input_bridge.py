import base64
import re
import json
import subprocess
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CLIENT_MAIN_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/application/SwgClient/src/win32/ClientMain.cpp"
)
CLIENT_PROJECT = REPOSITORY_ROOT / (
    "src/game/client/application/SwgClient/build/win32/SwgClient.vcxproj"
)
HELPER_SOURCE = REPOSITORY_ROOT / "scripts/Invoke-PrecuBackgroundInput.ps1"
COMMAND_QUEUE_HEADER = REPOSITORY_ROOT / (
    "src/engine/client/library/clientGame/src/shared/command/ClientCommandQueue.h"
)
COMMAND_QUEUE_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientGame/src/shared/command/ClientCommandQueue.cpp"
)
DIRECT_INPUT_CONFIG_HEADER = REPOSITORY_ROOT / (
    "src/engine/client/library/clientDirectInput/src/shared/"
    "ConfigClientDirectInput.h"
)
DIRECT_INPUT_CONFIG_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientDirectInput/src/shared/"
    "ConfigClientDirectInput.cpp"
)
DIRECT_INPUT_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientDirectInput/src/win32/DirectInput.cpp"
)
SPLASH_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiSplash.cpp"
)
LOGIN_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiLoginScreen.cpp"
)
GAME_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientGame/src/shared/core/Game.cpp"
)


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    open_brace = source.index("{", start + len(signature))
    depth = 0
    for position in range(open_brace, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[start : position + 1]
    raise ValueError(f"unterminated function body: {signature}")


class PrecuBackgroundInputBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.client_main = CLIENT_MAIN_SOURCE.read_text(encoding="utf-8")
        cls.client_project = CLIENT_PROJECT.read_text(encoding="utf-8")
        cls.helper = HELPER_SOURCE.read_text(encoding="utf-8")
        cls.command_queue_header = COMMAND_QUEUE_HEADER.read_text(encoding="utf-8")
        cls.command_queue_source = COMMAND_QUEUE_SOURCE.read_text(encoding="utf-8")
        cls.direct_input_config_header = DIRECT_INPUT_CONFIG_HEADER.read_text(
            encoding="utf-8"
        )
        cls.direct_input_config_source = DIRECT_INPUT_CONFIG_SOURCE.read_text(
            encoding="utf-8"
        )
        cls.direct_input_source = DIRECT_INPUT_SOURCE.read_text(encoding="utf-8")
        cls.splash_source = SPLASH_SOURCE.read_text(encoding="utf-8")
        cls.login_source = LOGIN_SOURCE.read_text(encoding="utf-8")
        cls.game_source = GAME_SOURCE.read_text(encoding="utf-8")

    def test_bridge_is_explicitly_opt_in_and_disabled_by_default(self):
        self.assertIn(
            'ConfigFile::getKeyBool("SwgClient", '
            '"enableBackgroundInputBridge", false)',
            self.client_main,
        )
        install_start = self.client_main.index("bool installBackgroundInputBridge()")
        install_end = self.client_main.index(
            "void removeBackgroundInputBridge()", install_start
        )
        install_body = self.client_main[install_start:install_end]
        self.assertLess(
            install_body.index("enableBackgroundInputBridge"),
            install_body.index("RegisterWindowMessageA"),
        )

    def test_unattended_host_can_explicitly_disable_physical_input_devices(self):
        self.assertIn("static bool         getUseKeyboard();", self.direct_input_config_header)
        self.assertIn("KEY_BOOL(useKeyboard,                     true);", self.direct_input_config_source)
        keyboard_install = function_body(
            self.direct_input_source,
            "void DirectInputNamespace::installKeyboardDevice(DWORD menuKey)",
        )
        self.assertIn("ConfigClientDirectInput::getUseKeyboard()", keyboard_install)
        self.assertIn("ConfigClientDirectInput::getUseMouse()", self.direct_input_source)

    def test_bridge_enabled_client_bypasses_splash_without_foreground_input(self):
        activation = function_body(
            self.splash_source, "void SwgCuiSplash::performActivate ()"
        )
        self.assertIn(
            'ConfigFile::getKeyBool("SwgClient", '
            '"enableBackgroundInputBridge", false)',
            activation,
        )
        self.assertLess(activation.index("enableBackgroundInputBridge"), activation.index("proceed()"))

    def test_bridge_enabled_client_attempts_login_without_foreground_input(self):
        activation = function_body(
            self.login_source, "void SwgCuiLoginScreen::performActivate ()"
        )
        self.assertIn(
            'ConfigFile::getKeyBool("SwgClient", '
            '"enableBackgroundInputBridge", false)',
            activation,
        )
        self.assertIn(
            "ConfigClientGame::getAutoConnectToLoginServer() || bridgeAutoConnect",
            activation,
        )

    def test_bridge_enabled_client_scheduler_runs_without_window_focus(self):
        self.assertIn(
            'ConfigFile::getKeyBool("SwgClient", '
            '"enableBackgroundInputBridge", false)',
            self.game_source,
        )
        focus_gate = self.game_source.index(
            "if (GetActiveWindow() == Os::getWindow()"
        )
        scheduler = self.game_source.index("GameScheduler::alter(elapsedTime)", focus_gate)
        bridge_config = self.game_source.rindex(
            "enableBackgroundInputBridge", 0, focus_gate
        )
        self.assertLess(bridge_config, scheduler)
        self.assertIn("backgroundInputBridge)", self.game_source[focus_gate:scheduler])

    def test_protocol_name_and_command_numbers_are_stable(self):
        self.assertIn(
            '"SWGSource.PreCU.BackgroundInput.v1"', self.client_main
        )
        expected_commands = [
            "BIC_ping = 0",
            "BIC_mouseMove",
            "BIC_leftMouseDown",
            "BIC_leftMouseUp",
            "BIC_rightMouseDown",
            "BIC_rightMouseUp",
            "BIC_middleMouseDown",
            "BIC_middleMouseUp",
            "BIC_keyDown",
            "BIC_keyUp",
            "BIC_character",
            "BIC_inputReset",
            "BIC_examineCharacterSheet",
            "BIC_inviteTarget",
            "BIC_joinGroup",
            "BIC_disbandGroup",
            "BIC_openStatMigration",
            "BIC_startImageDesign",
            "BIC_targetCounterpart",
            "BIC_queueCombatCanary",
            "BIC_clearCombatQueue",
            "BIC_combatQueueStatus",
            "BIC_equipCdefRifle",
            "BIC_stand",
            "BIC_queueBodyShot1",
            "BIC_queueLegShot1",
            "BIC_equipCdefPistol",
            "BIC_equipCdefCarbine",
            "BIC_combatTimerStatus",
            "BIC_queueDurationControl",
            "BIC_equipFixtureLightsaber",
            "BIC_equipFixtureFallbackSword",
            "BIC_queueHealWound",
            "BIC_queueHealDamage",
            "BIC_queueTendDamage",
            "BIC_queueTendWound",
            "BIC_queueDiagnose",
            "BIC_queueMedicalForage",
            "BIC_queueFirstAid",
            "BIC_queueDragIncapacitatedPlayer",
            "BIC_queueQuickHeal",
            "BIC_queueHealState",
            "BIC_queueCurePoison",
            "BIC_queueHealEnhance",
            "BIC_queueExtinguishFire",
            "BIC_queueCureDisease",
            "BIC_queueRevivePlayer",
            "BIC_queueDeathBlow",
            "BIC_selectCloneLocation",
            "BIC_confirmCloneLocation",
            "BIC_startDanceRhythmic",
            "BIC_flourishOne",
            "BIC_stopDance",
            "BIC_startMusicStarwars1",
            "BIC_stopMusic",
            "BIC_startBandStarwars1",
            "BIC_bandFlourishOne",
            "BIC_stopBand",
            "BIC_startMusicRock",
            "BIC_surrenderEntertainerMusicOne",
            "BIC_startMusicStarwars2",
            "BIC_surrenderEntertainerMusicTwo",
            "BIC_startMusicFolk",
            "BIC_surrenderEntertainerMusicThree",
            "BIC_startMusicStarwars3",
            "BIC_surrenderEntertainerMusicFour",
            "BIC_startMusicCeremonial",
            "BIC_surrenderEntertainerMaster",
            "BIC_startDanceBasicTwo",
            "BIC_surrenderEntertainerDanceOne",
            "BIC_startDanceRhythmicTwo",
            "BIC_surrenderEntertainerDanceTwo",
            "BIC_startDanceFootloose",
            "BIC_surrenderEntertainerDanceThree",
            "BIC_startDanceFormal",
            "BIC_surrenderEntertainerDanceFour",
            "BIC_surrenderEntertainerHairstyleOne",
            "BIC_surrenderEntertainerHairstyleTwo",
            "BIC_surrenderEntertainerHairstyleThree",
            "BIC_surrenderEntertainerHairstyleFour",
            "BIC_startDancePopular",
            "BIC_surrenderDancerNovice",
            "BIC_surrenderDancerAbilityOne",
            "BIC_surrenderDancerAbilityTwo",
            "BIC_surrenderDancerAbilityThree",
            "BIC_surrenderDancerAbilityFour",
            "BIC_surrenderDancerWoundOne",
            "BIC_surrenderDancerWoundTwo",
            "BIC_surrenderDancerWoundThree",
            "BIC_surrenderDancerWoundFour",
            "BIC_surrenderDancerShockOne",
            "BIC_surrenderDancerShockTwo",
            "BIC_surrenderDancerShockThree",
            "BIC_surrenderDancerShockFour",
            "BIC_surrenderDancerKnowledgeOne",
            "BIC_surrenderDancerKnowledgeTwo",
            "BIC_surrenderDancerKnowledgeThree",
            "BIC_surrenderDancerKnowledgeFour",
            "BIC_surrenderDancerMaster",
            "BIC_surrenderMusicianNovice",
            "BIC_surrenderMusicianAbilityOne",
            "BIC_surrenderMusicianAbilityTwo",
            "BIC_surrenderMusicianAbilityThree",
            "BIC_surrenderMusicianAbilityFour",
            "BIC_surrenderMusicianWoundOne",
            "BIC_surrenderMusicianWoundTwo",
            "BIC_surrenderMusicianWoundThree",
            "BIC_surrenderMusicianWoundFour",
            "BIC_surrenderMusicianShockOne",
            "BIC_surrenderMusicianShockTwo",
            "BIC_surrenderMusicianShockThree",
            "BIC_surrenderMusicianShockFour",
            "BIC_surrenderMusicianKnowledgeOne",
            "BIC_surrenderMusicianKnowledgeTwo",
            "BIC_surrenderMusicianKnowledgeThree",
            "BIC_surrenderMusicianKnowledgeFour",
            "BIC_surrenderMusicianMaster",
            "BIC_showAllProfessions",
            "BIC_selectAllProfession",
            "BIC_equipFixturePolearm",
            "BIC_unequipHeldWeapon",
            "BIC_queuePolearmLegHit1",
            "BIC_queueUnarmedHeadHit1",
            "BIC_polearmLegHit1WeaponStatus",
            "BIC_unarmedHeadHit1WeaponStatus",
            "BIC_queuePolearmSpinAttack1",
            "BIC_polearmSpinAttack1WeaponStatus",
            "BIC_equipFixtureOneHand",
            "BIC_equipFixtureTwoHand",
            "BIC_queueMelee1hSpinAttack1",
            "BIC_melee1hSpinAttack1WeaponStatus",
            "BIC_queueMelee2hSpinAttack1",
            "BIC_melee2hSpinAttack1WeaponStatus",
            "BIC_queueBodyShot2",
            "BIC_bodyShot2WeaponStatus",
            "BIC_queueBodyShot3",
            "BIC_bodyShot3WeaponStatus",
            "BIC_headShot2WeaponStatus",
            "BIC_queueHeadShot3",
            "BIC_headShot3WeaponStatus",
        ]
        positions = [self.client_main.index(command) for command in expected_commands]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("cms_backgroundInputProtocolVersion = 106", self.client_main)
        self.assertIn("$expectedProtocolVersion = 106", self.helper)

    def test_bridge_exposes_core3_random_area_pilot(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"polearmSpinAttack1"',
            'getBackgroundGeneratedCombatWeaponStatus("polearmSpinAttack1")',
            "BIC_queuePolearmSpinAttack1",
            "BIC_polearmSpinAttack1WeaponStatus",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueuePolearmSpinAttack1"',
            '"PolearmSpinAttack1WeaponStatus"',
            "QueuePolearmSpinAttack1 = 125",
            "PolearmSpinAttack1WeaponStatus = 126",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_one_and_two_hand_area_actions(self):
        for token in [
            'performBackgroundEquipCdefWeapon("sword_rantok.iff")',
            'performBackgroundEquipCdefWeapon("2h_sword_cleaver.iff")',
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"melee1hSpinAttack1"',
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"melee2hSpinAttack1"',
            'getBackgroundGeneratedCombatWeaponStatus("melee1hSpinAttack1")',
            'getBackgroundGeneratedCombatWeaponStatus("melee2hSpinAttack1")',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"EquipFixtureOneHand"',
            '"EquipFixtureTwoHand"',
            '"QueueMelee1hSpinAttack1"',
            '"QueueMelee2hSpinAttack1"',
            '"Melee1hSpinAttack1WeaponStatus"',
            '"Melee2hSpinAttack1WeaponStatus"',
            "EquipFixtureOneHand = 127",
            "EquipFixtureTwoHand = 128",
            "QueueMelee1hSpinAttack1 = 129",
            "Melee1hSpinAttack1WeaponStatus = 130",
            "QueueMelee2hSpinAttack1 = 131",
            "Melee2hSpinAttack1WeaponStatus = 132",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_body_shot_continuation(self):
        for token in [
            'performBackgroundQueueMarksmanTier1("bodyShot2"',
            'performBackgroundQueueMarksmanTier1("bodyShot3"',
            'getBackgroundGeneratedCombatWeaponStatus("bodyShot2")',
            'getBackgroundGeneratedCombatWeaponStatus("bodyShot3")',
            "BIC_queueBodyShot2",
            "BIC_bodyShot2WeaponStatus",
            "BIC_queueBodyShot3",
            "BIC_bodyShot3WeaponStatus",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueBodyShot2"',
            '"BodyShot2WeaponStatus"',
            '"QueueBodyShot3"',
            '"BodyShot3WeaponStatus"',
            "QueueBodyShot2 = 133",
            "BodyShot2WeaponStatus = 134",
            "QueueBodyShot3 = 135",
            "BodyShot3WeaponStatus = 136",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_head_shot_continuation(self):
        for token in [
            'getBackgroundGeneratedCombatWeaponStatus("headShot2")',
            'performBackgroundQueueMarksmanTier1("headShot3"',
            'getBackgroundGeneratedCombatWeaponStatus("headShot3")',
            "BIC_headShot2WeaponStatus",
            "BIC_queueHeadShot3",
            "BIC_headShot3WeaponStatus",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"HeadShot2WeaponStatus"',
            '"QueueHeadShot3"',
            '"HeadShot3WeaponStatus"',
            "HeadShot2WeaponStatus = 137",
            "QueueHeadShot3 = 138",
            "HeadShot3WeaponStatus = 139",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_queues_internal_input_events(self):
        required_calls = [
            "IoWinManager::queueSetSystemMouseCursorPosition",
            "IoWinManager::queueMouseButtonDown(0, 0)",
            "IoWinManager::queueMouseButtonUp(0, 0)",
            "IoWinManager::queueMouseButtonDown(0, 1)",
            "IoWinManager::queueMouseButtonUp(0, 1)",
            "IoWinManager::queueMouseButtonDown(0, 2)",
            "IoWinManager::queueMouseButtonUp(0, 2)",
            "IoWinManager::queueKeyDown(0, key)",
            "IoWinManager::queueKeyUp(0, key)",
            "IoWinManager::queueCharacter",
            "IoWinManager::queueInputReset",
        ]
        for call in required_calls:
            with self.subTest(call=call):
                self.assertIn(call, self.client_main)
        self.assertIn("lParam < 0 || lParam > 255", self.client_main)

    def test_bridge_does_not_reinterpret_native_window_input(self):
        forbidden_native_cases = [
            "case WM_KEYDOWN",
            "case WM_KEYUP",
            "case WM_LBUTTONDOWN",
            "case WM_LBUTTONUP",
            "case WM_RBUTTONDOWN",
            "case WM_RBUTTONUP",
        ]
        for native_case in forbidden_native_cases:
            with self.subTest(native_case=native_case):
                self.assertNotIn(native_case, self.client_main)
        self.assertNotIn("DISCL_BACKGROUND", self.client_main)

    def test_bridge_lifetime_is_inside_iowin_and_ui_lifetime(self):
        shared_iowin_install = self.client_main.rindex(
            "SetupSharedIoWin::install();"
        )
        ui_install = self.client_main.rindex("SetupSwgClientUserInterface::install();")
        bridge_install = self.client_main.rindex(
            "bool const backgroundInputBridgeInstalled = "
            "installBackgroundInputBridge();"
        )
        game_run = self.client_main.rindex(
            "SetupSharedFoundation::callbackWithExceptionHandling(Game::run);"
        )
        bridge_remove = self.client_main.rindex("removeBackgroundInputBridge();")
        self.assertLess(
            shared_iowin_install,
            ui_install,
        )
        self.assertLess(ui_install, bridge_install)
        self.assertLess(bridge_install, game_run)
        self.assertLess(game_run, bridge_remove)

    def test_window_subclass_is_chained_and_restored(self):
        self.assertIn("SetWindowLongPtr", self.client_main)
        self.assertIn("CallWindowProc(s_backgroundInputPreviousWindowProc", self.client_main)
        self.assertIn(
            "reinterpret_cast<LONG_PTR>(s_backgroundInputPreviousWindowProc)",
            self.client_main,
        )

    def test_helper_uses_only_window_targeted_protocol_calls(self):
        self.assertIn(
            '$messageName = "SWGSource.PreCU.BackgroundInput.v1"', self.helper
        )
        for required_api in (
            "RegisterWindowMessage",
            "PostMessage",
            "SendMessageTimeout",
            "GetForegroundWindow",
        ):
            with self.subTest(required_api=required_api):
                self.assertIn(required_api, self.helper)

        forbidden_apis = (
            "SetForegroundWindow",
            "SetCursorPos",
            "SendInput",
            "mouse_event",
            "System.Windows.Forms.SendKeys",
        )
        for forbidden_api in forbidden_apis:
            with self.subTest(forbidden_api=forbidden_api):
                self.assertNotIn(forbidden_api, self.helper)

    def test_ui_state_dependent_actions_are_foreground_guarded_before_queueing(self):
        documentation = self.helper[: self.helper.index("[CmdletBinding()]")]
        self.assertNotIn("without activating its window", documentation)
        self.assertIn("Successful actions are only", documentation)
        self.assertIn("processing is asynchronous", documentation)
        self.assertIn("current UI state", documentation)

        guard = function_body(
            self.helper, "function Assert-BridgeForegroundForUiStateAction"
        )
        self.assertIn('$RequestedAction -eq "Text"', guard)
        self.assertIn('$RequestedAction -in @("Key", "Chord")', guard)
        self.assertIn("$ResolvedDikCode -eq 0x1c", guard)
        self.assertIn(
            "[PrecuBackgroundInput.NativeMethods]::GetForegroundWindow()", guard
        )

        key_start = self.helper.index('{ $_ -in @("Key", "Chord") }')
        text_start = self.helper.index('    "Text" {', key_start)
        key_action = self.helper[key_start:text_start]
        self.assertLess(
            key_action.index("Assert-BridgeForegroundForUiStateAction"),
            key_action.index("Send-BridgeKeySequence"),
        )

        reset_start = self.helper.index('    "Reset" {', text_start)
        text_action = self.helper[text_start:reset_start]
        self.assertLess(
            text_action.index("Assert-BridgeForegroundForUiStateAction"),
            text_action.index("Send-BridgeCommand"),
        )

    def test_foreground_guard_rejects_only_off_focus_text_and_enter(self):
        guard = function_body(
            self.helper, "function Assert-BridgeForegroundForUiStateAction"
        )
        powershell = rf"""
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
{guard}

function Invoke-GuardScenario {{
    param(
        [string]$Name,
        [string]$RequestedAction,
        [int]$ResolvedDikCode,
        [IntPtr]$TargetWindow,
        [IntPtr]$ForegroundWindow
    )

    $capturedForegroundWindow = $ForegroundWindow
    $provider = {{ $capturedForegroundWindow }}.GetNewClosure()
    $allowed = $true
    $errorMessage = $null
    try {{
        Assert-BridgeForegroundForUiStateAction `
            -TargetWindow $TargetWindow `
            -RequestedAction $RequestedAction `
            -ResolvedDikCode $ResolvedDikCode `
            -ForegroundWindowProvider $provider
    }}
    catch {{
        $allowed = $false
        $errorMessage = $_.Exception.Message
    }}

    [pscustomobject]@{{
        Name = $Name
        Allowed = $allowed
        Error = $errorMessage
    }}
}}

$target = [IntPtr]::new(1234)
$other = [IntPtr]::new(5678)
$results = @()
$results += Invoke-GuardScenario -Name "text-background" -RequestedAction "Text" -ResolvedDikCode -1 -TargetWindow $target -ForegroundWindow $other
$results += Invoke-GuardScenario -Name "key-enter-background" -RequestedAction "Key" -ResolvedDikCode 0x1c -TargetWindow $target -ForegroundWindow $other
$results += Invoke-GuardScenario -Name "chord-enter-background" -RequestedAction "Chord" -ResolvedDikCode 0x1c -TargetWindow $target -ForegroundWindow $other
$results += Invoke-GuardScenario -Name "text-foreground" -RequestedAction "Text" -ResolvedDikCode -1 -TargetWindow $target -ForegroundWindow $target
$results += Invoke-GuardScenario -Name "key-enter-foreground" -RequestedAction "Key" -ResolvedDikCode 0x1c -TargetWindow $target -ForegroundWindow $target
$results += Invoke-GuardScenario -Name "chord-enter-foreground" -RequestedAction "Chord" -ResolvedDikCode 0x1c -TargetWindow $target -ForegroundWindow $target
$results += Invoke-GuardScenario -Name "escape-background" -RequestedAction "Key" -ResolvedDikCode 0x01 -TargetWindow $target -ForegroundWindow $other
$results += Invoke-GuardScenario -Name "ctrl-s-background" -RequestedAction "Chord" -ResolvedDikCode 31 -TargetWindow $target -ForegroundWindow $other
$results += Invoke-GuardScenario -Name "mouse-background" -RequestedAction "LeftClick" -ResolvedDikCode -1 -TargetWindow $target -ForegroundWindow $other
$results | ConvertTo-Json -Compress
"""
        completed = subprocess.run(
            [
                "powershell.exe",
                "-NoProfile",
                "-NonInteractive",
                "-EncodedCommand",
                base64.b64encode(powershell.encode("utf-16le")).decode("ascii"),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(0, completed.returncode, completed.stderr)
        scenarios = {
            item["Name"]: item for item in json.loads(completed.stdout)
        }

        for name in (
            "text-background",
            "key-enter-background",
            "chord-enter-background",
        ):
            with self.subTest(name=name):
                self.assertFalse(scenarios[name]["Allowed"])
                self.assertIn("is refused", scenarios[name]["Error"])

        for name in (
            "text-foreground",
            "key-enter-foreground",
            "chord-enter-foreground",
            "escape-background",
            "ctrl-s-background",
            "mouse-background",
        ):
            with self.subTest(name=name):
                self.assertTrue(scenarios[name]["Allowed"])
                self.assertIsNone(scenarios[name]["Error"])

    def test_helper_command_numbers_match_client_protocol(self):
        expected_helper_commands = {
            "Ping": 0,
            "MouseMove": 1,
            "LeftMouseDown": 2,
            "LeftMouseUp": 3,
            "RightMouseDown": 4,
            "RightMouseUp": 5,
            "MiddleMouseDown": 6,
            "MiddleMouseUp": 7,
            "KeyDown": 8,
            "KeyUp": 9,
            "Character": 10,
            "InputReset": 11,
            "ExamineCharacterSheet": 12,
            "InviteTarget": 13,
            "JoinGroup": 14,
            "DisbandGroup": 15,
            "OpenStatMigration": 16,
            "StartImageDesign": 17,
            "TargetCounterpart": 18,
            "QueueCombatCanary": 19,
            "ClearCombatQueue": 20,
            "CombatQueueStatus": 21,
            "EquipCdefRifle": 22,
            "Stand": 23,
            "QueueBodyShot1": 24,
            "QueueLegShot1": 25,
            "EquipCdefPistol": 26,
            "EquipCdefCarbine": 27,
            "CombatTimerStatus": 28,
            "QueueDurationControl": 29,
            "EquipFixtureLightsaber": 30,
            "EquipFixtureFallbackSword": 31,
            "QueueHealWound": 32,
            "QueueHealDamage": 33,
            "QueueTendDamage": 34,
            "QueueTendWound": 35,
            "QueueDiagnose": 36,
            "QueueMedicalForage": 37,
            "QueueFirstAid": 38,
            "QueueDragIncapacitatedPlayer": 39,
            "QueueQuickHeal": 40,
            "QueueHealState": 41,
            "QueueCurePoison": 42,
            "QueueHealEnhance": 43,
            "QueueExtinguishFire": 44,
            "QueueCureDisease": 45,
            "QueueRevivePlayer": 46,
            "QueueDeathBlow": 47,
            "SelectCloneLocation": 48,
            "ConfirmCloneLocation": 49,
            "StartDanceRhythmic": 50,
            "FlourishOne": 51,
            "StopDance": 52,
            "StartMusicStarwars1": 53,
            "StopMusic": 54,
            "StartBandStarwars1": 55,
            "BandFlourishOne": 56,
            "StopBand": 57,
            "StartMusicRock": 58,
            "SurrenderEntertainerMusicOne": 59,
            "StartMusicStarwars2": 60,
            "SurrenderEntertainerMusicTwo": 61,
            "StartMusicFolk": 62,
            "SurrenderEntertainerMusicThree": 63,
            "StartMusicStarwars3": 64,
            "SurrenderEntertainerMusicFour": 65,
            "StartMusicCeremonial": 66,
            "SurrenderEntertainerMaster": 67,
            "StartDanceBasicTwo": 68,
            "SurrenderEntertainerDanceOne": 69,
            "StartDanceRhythmicTwo": 70,
            "SurrenderEntertainerDanceTwo": 71,
            "StartDanceFootloose": 72,
            "SurrenderEntertainerDanceThree": 73,
            "StartDanceFormal": 74,
            "SurrenderEntertainerDanceFour": 75,
            "SurrenderEntertainerHairstyleOne": 76,
            "SurrenderEntertainerHairstyleTwo": 77,
            "SurrenderEntertainerHairstyleThree": 78,
            "SurrenderEntertainerHairstyleFour": 79,
            "StartDancePopular": 80,
            "SurrenderDancerNovice": 81,
            "SurrenderDancerAbilityOne": 82,
            "SurrenderDancerAbilityTwo": 83,
            "SurrenderDancerAbilityThree": 84,
            "SurrenderDancerAbilityFour": 85,
            "SurrenderDancerWoundOne": 86,
            "SurrenderDancerWoundTwo": 87,
            "SurrenderDancerWoundThree": 88,
            "SurrenderDancerWoundFour": 89,
            "SurrenderDancerShockOne": 90,
            "SurrenderDancerShockTwo": 91,
            "SurrenderDancerShockThree": 92,
            "SurrenderDancerShockFour": 93,
            "SurrenderDancerKnowledgeOne": 94,
            "SurrenderDancerKnowledgeTwo": 95,
            "SurrenderDancerKnowledgeThree": 96,
            "SurrenderDancerKnowledgeFour": 97,
            "SurrenderDancerMaster": 98,
            "SurrenderMusicianNovice": 99,
            "SurrenderMusicianAbilityOne": 100,
            "SurrenderMusicianAbilityTwo": 101,
            "SurrenderMusicianAbilityThree": 102,
            "SurrenderMusicianAbilityFour": 103,
            "SurrenderMusicianWoundOne": 104,
            "SurrenderMusicianWoundTwo": 105,
            "SurrenderMusicianWoundThree": 106,
            "SurrenderMusicianWoundFour": 107,
            "SurrenderMusicianShockOne": 108,
            "SurrenderMusicianShockTwo": 109,
            "SurrenderMusicianShockThree": 110,
            "SurrenderMusicianShockFour": 111,
            "SurrenderMusicianKnowledgeOne": 112,
            "SurrenderMusicianKnowledgeTwo": 113,
            "SurrenderMusicianKnowledgeThree": 114,
            "SurrenderMusicianKnowledgeFour": 115,
            "SurrenderMusicianMaster": 116,
            "ShowAllProfessions": 117,
            "SelectAllProfession": 118,
        }
        for name, value in expected_helper_commands.items():
            with self.subTest(command=name):
                self.assertRegex(
                    self.helper,
                    rf"(?m)^\s*{re.escape(name)}\s*=\s*{value}\s*$",
                )

    def test_helper_exposes_atomic_chords_not_raw_key_state(self):
        parameter_block = self.helper[: self.helper.index("Set-StrictMode")]
        self.assertIn('"Key", "Chord", "Text"', parameter_block)
        self.assertNotIn('"KeyDown"', parameter_block)
        self.assertNotIn('"KeyUp"', parameter_block)
        self.assertIn("[int[]]$ModifierDikCode", parameter_block)

        sequence = function_body(self.helper, "function Send-BridgeKeySequence")
        self.assertIn("[AllowEmptyCollection()]", sequence)
        self.assertIn("try {", sequence)
        self.assertIn("finally {", sequence)
        self.assertIn("$pressedModifiers.Count - 1", sequence)
        self.assertIn("$command.InputReset", sequence)
        self.assertIn("$actionError", sequence)

    def test_helper_resolves_nonactivating_recreated_client_window(self):
        self.assertIn("FindTopLevelWindow", self.helper)
        self.assertIn("EnumWindows", self.helper)
        self.assertIn("GetWindowThreadProcessId", self.helper)
        resolver = function_body(self.helper, "function Resolve-ClientWindow")
        self.assertIn("$client.MainWindowHandle", resolver)
        self.assertIn("$fallbackWindow", resolver)
        self.assertIn("MainWindowHandle = $fallbackWindow", resolver)

    def test_remote_character_sheet_action_is_narrow_and_target_guarded(self):
        action = function_body(
            self.client_main, "bool performBackgroundExamineCharacterSheet()"
        )
        self.assertIn("Game::getPlayer()", action)
        self.assertIn("CuiAction::findObjectFromFirstParam", action)
        self.assertIn("CuiActions::examineCharacterSheet", action)
        self.assertIn("!target", action)
        self.assertIn("target == player", action)
        self.assertNotIn("externalCommandHandler", action)

        parameter_block = self.helper[: self.helper.index("Set-StrictMode")]
        self.assertIn('"ExamineCharacterSheet"', parameter_block)

    def test_image_designer_actions_use_fixed_production_paths(self):
        target_action = function_body(
            self.client_main, "bool performBackgroundTargetCommand(char const * const command)"
        )
        self.assertIn("CuiAction::findObjectFromFirstParam", target_action)
        self.assertIn("target == player", target_action)
        self.assertIn("CuiMessageQueueManager::executeCommandByString(command, true)", target_action)

        self_action = function_body(
            self.client_main, "bool performBackgroundSelfCommand(char const * const command)"
        )
        self.assertIn("CuiMessageQueueManager::executeCommandByString(command, true)", self_action)

        stat_migration = function_body(
            self.client_main, "bool performBackgroundOpenStatMigration()"
        )
        self.assertIn("CuiMediatorTypes::WS_StatMigration", stat_migration)

        window_proc = function_body(
            self.client_main,
            "LRESULT CALLBACK backgroundInputWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)",
        )
        fixed_routes = {
            'performBackgroundTargetCommand("/invite")',
            'performBackgroundSelfCommand("/join")',
            'performBackgroundSelfCommand("/disband")',
            'performBackgroundTargetCommand("/imagedesign")',
        }
        for route in fixed_routes:
            with self.subTest(route=route):
                self.assertIn(route, window_proc)

        parameter_block = self.helper[: self.helper.index("Set-StrictMode")]
        for action in (
            "InviteTarget",
            "JoinGroup",
            "DisbandGroup",
            "OpenStatMigration",
            "StartImageDesign",
            "TargetCounterpart",
        ):
            with self.subTest(action=action):
                self.assertIn(f'"{action}"', parameter_block)

    def test_counterpart_targeting_is_bound_to_live_fixture_identities(self):
        target_counterpart = function_body(
            self.client_main, "bool performBackgroundTargetCounterpart()"
        )
        for object_id in ("44003778", "39008597"):
            with self.subTest(object_id=object_id):
                self.assertIn(f'"{object_id}"', target_counterpart)
        self.assertIn('CuiCombatManager::setLookAtTarget(NetworkId("39008597"))', target_counterpart)
        self.assertIn('CuiCombatManager::setLookAtTarget(NetworkId("44003778"))', target_counterpart)
        self.assertNotIn("externalCommandHandler", target_counterpart)
        self.assertNotIn("lParam", target_counterpart)

    def test_combat_queue_live_actions_are_fixed_guarded_and_observable(self):
        pair_guard = function_body(
            self.client_main, "bool isBackgroundCombatCanaryPair("
        )
        for object_id in ("44003778", "39008597"):
            with self.subTest(object_id=object_id):
                self.assertIn(f'"{object_id}"', pair_guard)

        queue_action = function_body(
            self.client_main, "bool performBackgroundQueueCombatCanary(int const repeat)"
        )
        self.assertIn('NetworkId("39008597")', queue_action)
        self.assertIn('NetworkId("44003778")', queue_action)
        self.assertIn("isBackgroundCombatCanaryPair(*player, targetId)", queue_action)
        self.assertIn("CuiCombatManager::setCombatTarget", queue_action)
        self.assertIn("ClientCommandQueue::commandsAreNowFromToolbar(true)", queue_action)
        self.assertIn("ClientCommandQueue::enqueueCommand", queue_action)
        self.assertIn('"headShot1"', queue_action)
        self.assertIn("targetId", queue_action)
        self.assertIn("ClientCommandQueue::commandsAreNowFromToolbar(false)", queue_action)
        self.assertIn("index < repeat", queue_action)
        self.assertNotIn("Transceivers::added", queue_action)
        self.assertIn(
            r"game\shared\library\swgSharedUtility\include\public",
            self.client_project,
        )

        duration_control = function_body(
            self.client_main, "bool performBackgroundQueueDurationControl()"
        )
        self.assertIn('getValueString() != "44003778"', duration_control)
        self.assertIn('NetworkId const targetId("39008597")', duration_control)
        self.assertIn("isBackgroundCombatCanaryPair(*player, targetId)", duration_control)
        self.assertIn('"headShot2"', duration_control)
        self.assertIn("targetId", duration_control)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            duration_control,
        )
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            duration_control,
        )
        self.assertNotIn('"headShot1"', duration_control)
        self.assertNotIn("doDamage", duration_control)

        heal_wound = function_body(
            self.client_main,
            "bool performBackgroundQueueHealWound(LPARAM const targetValue)",
        )
        self.assertIn("targetValue <= 0", heal_wound)
        self.assertIn("NetworkId const targetId(targetBuffer)", heal_wound)
        self.assertIn("CuiCombatManager::setLookAtTarget(targetId)", heal_wound)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            heal_wound,
        )
        self.assertIn('"healWound"', heal_wound)
        self.assertIn("targetId", heal_wound)
        self.assertIn("Unicode::emptyString", heal_wound)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            heal_wound,
        )
        for forbidden_mutation in (
            "createObject",
            "grantSkill",
            "setAttrib",
            "healWound(",
        ):
            with self.subTest(forbidden_mutation=forbidden_mutation):
                self.assertNotIn(forbidden_mutation, heal_wound)

        heal_damage = function_body(
            self.client_main,
            "bool performBackgroundQueueHealDamage(LPARAM const targetValue)",
        )
        self.assertIn("targetValue <= 0", heal_damage)
        self.assertIn("NetworkId const targetId(targetBuffer)", heal_damage)
        self.assertIn("CuiCombatManager::setLookAtTarget(targetId)", heal_damage)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            heal_damage,
        )
        self.assertIn('"healDamage"', heal_damage)
        self.assertIn("targetId", heal_damage)
        self.assertIn("Unicode::emptyString", heal_damage)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            heal_damage,
        )
        for forbidden_mutation in (
            "createObject",
            "grantSkill",
            "setAttrib",
            "performMedicalHealDamage",
        ):
            with self.subTest(forbidden_mutation=forbidden_mutation):
                self.assertNotIn(forbidden_mutation, heal_damage)

        tending = function_body(
            self.client_main,
            "bool performBackgroundQueueTending(",
        )
        self.assertIn("targetValue <= 0", tending)
        self.assertIn("NetworkId const targetId(targetBuffer)", tending)
        self.assertIn("CuiCombatManager::setLookAtTarget(targetId)", tending)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            tending,
        )
        self.assertIn("commandName", tending)
        self.assertIn("targetId", tending)
        self.assertIn("Unicode::emptyString", tending)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            tending,
        )
        self.assertIn(
            'performBackgroundQueueTending("tendDamage", targetValue)',
            self.client_main,
        )
        self.assertIn(
            'performBackgroundQueueTending("tendWound", targetValue)',
            self.client_main,
        )
        diagnose = function_body(
            self.client_main,
            "bool performBackgroundQueueDiagnose(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("diagnose", targetValue)',
            diagnose,
        )
        medical_forage = function_body(
            self.client_main,
            "bool performBackgroundQueueMedicalForage()",
        )
        self.assertIn('"medicalForage"', medical_forage)
        self.assertIn("NetworkId::cms_invalid", medical_forage)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            medical_forage,
        )
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            medical_forage,
        )
        first_aid = function_body(
            self.client_main,
            "bool performBackgroundQueueFirstAid(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("firstAid", targetValue)',
            first_aid,
        )
        drag_player = function_body(
            self.client_main,
            "bool performBackgroundQueueDragIncapacitatedPlayer(LPARAM const targetValue)",
        )
        self.assertIn(
            '"dragIncapacitatedPlayer"',
            drag_player,
        )
        self.assertIn(
            "targetValue",
            drag_player,
        )
        quick_heal = function_body(
            self.client_main,
            "bool performBackgroundQueueQuickHeal(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("quickHeal", targetValue)',
            quick_heal,
        )
        heal_state = function_body(
            self.client_main,
            "bool performBackgroundQueueHealState(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("healState", targetValue)',
            heal_state,
        )
        cure_poison = function_body(
            self.client_main,
            "bool performBackgroundQueueCurePoison(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("curePoison", targetValue)',
            cure_poison,
        )
        heal_enhance = function_body(
            self.client_main,
            "bool performBackgroundQueueHealEnhance(LPARAM const targetValue)",
        )
        self.assertIn(
            '"healEnhance",',
            heal_enhance,
        )
        self.assertIn("targetValue,", heal_enhance)
        self.assertIn("true", heal_enhance)
        extinguish_fire = function_body(
            self.client_main,
            "bool performBackgroundQueueExtinguishFire(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("extinguishFire", targetValue)',
            extinguish_fire,
        )
        cure_disease = function_body(
            self.client_main,
            "bool performBackgroundQueueCureDisease(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("cureDisease", targetValue)',
            cure_disease,
        )
        revive_player = function_body(
            self.client_main,
            "bool performBackgroundQueueRevivePlayer(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("revivePlayer", targetValue)',
            revive_player,
        )
        death_blow = function_body(
            self.client_main,
            "bool performBackgroundQueueDeathBlow(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("deathBlow", targetValue)',
            death_blow,
        )
        tending = function_body(
            self.client_main,
            "bool performBackgroundQueueTending(",
        )
        self.assertIn("includeTargetParameter", tending)
        self.assertIn("Unicode::narrowToWide(targetBuffer)", tending)
        for forbidden_mutation in (
            "createObject",
            "grantSkill",
            "setAttrib",
            "performTendDamage",
            "performTendWound",
        ):
            with self.subTest(forbidden_mutation=forbidden_mutation):
                self.assertNotIn(forbidden_mutation, tending)

        window_proc = function_body(
            self.client_main,
            "LRESULT CALLBACK backgroundInputWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)",
        )
        self.assertIn("lParam < 1 || lParam > 16", window_proc)
        self.assertIn("return getBackgroundCombatQueueStatus();", window_proc)

        clear_action = function_body(
            self.client_main, "bool performBackgroundClearCombatQueue()"
        )
        self.assertIn("SwgCuiActions::clearCombatQueue", clear_action)
        self.assertNotIn("ClientCommandQueue::clear", clear_action)

        status_query = function_body(
            self.client_main, "LRESULT getBackgroundCombatQueueStatus()"
        )
        self.assertIn("getCombatCommandsFromQueue(sequenceIds)", status_query)
        self.assertIn("CuiCombatManager::isInCombat", status_query)
        self.assertIn("CuiCombatManager::getCombatTarget().isValid()", status_query)
        self.assertIn("ClientCommandQueue::getLastCommandRemoval", status_query)
        self.assertIn("cms_backgroundCombatQueueStatusLastResult", status_query)
        for forbidden_mutation in (
            "enqueueCommand",
            "removeCommand",
            "performAction",
        ):
            with self.subTest(forbidden_mutation=forbidden_mutation):
                self.assertNotIn(forbidden_mutation, status_query)

        helper_parameters = self.helper[: self.helper.index("Set-StrictMode")]
        for action in (
            "QueueCombatCanary",
            "QueueDurationControl",
            "ClearCombatQueue",
            "CombatQueueStatus",
            "CombatTimerStatus",
            "EquipCdefRifle",
            "QueueBodyShot1",
            "QueueLegShot1",
            "QueueBodyShot2",
            "BodyShot2WeaponStatus",
            "QueueBodyShot3",
            "BodyShot3WeaponStatus",
            "HeadShot2WeaponStatus",
            "QueueHeadShot3",
            "HeadShot3WeaponStatus",
            "EquipCdefPistol",
            "EquipCdefCarbine",
            "EquipFixtureLightsaber",
            "EquipFixtureFallbackSword",
            "QueueHealWound",
            "QueueHealDamage",
            "QueueTendDamage",
            "QueueTendWound",
            "QueueDiagnose",
            "QueueMedicalForage",
            "QueueFirstAid",
            "QueueDragIncapacitatedPlayer",
            "QueueQuickHeal",
            "QueueHealState",
            "QueueCurePoison",
            "QueueHealEnhance",
            "QueueExtinguishFire",
            "QueueCureDisease",
            "QueueRevivePlayer",
            "QueueDeathBlow",
            "SelectCloneLocation",
            "StartDanceRhythmic",
            "FlourishOne",
            "StopDance",
            "StartBandStarwars1",
            "BandFlourishOne",
            "StopBand",
            "StartMusicRock",
            "SurrenderEntertainerMusicOne",
            "StartMusicStarwars2",
            "SurrenderEntertainerMusicTwo",
            "StartMusicFolk",
            "SurrenderEntertainerMusicThree",
            "StartMusicStarwars3",
            "SurrenderEntertainerMusicFour",
            "StartMusicCeremonial",
            "SurrenderEntertainerMaster",
            "StartDanceBasicTwo",
            "SurrenderEntertainerDanceOne",
            "StartDanceRhythmicTwo",
            "SurrenderEntertainerDanceTwo",
            "StartDanceFootloose",
            "SurrenderEntertainerDanceThree",
            "StartDanceFormal",
            "SurrenderEntertainerDanceFour",
            "SurrenderEntertainerHairstyleOne",
            "SurrenderEntertainerHairstyleTwo",
            "SurrenderEntertainerHairstyleThree",
            "SurrenderEntertainerHairstyleFour",
            "StartDancePopular",
            "SurrenderDancerNovice",
            "SurrenderDancerAbilityOne",
            "SurrenderDancerAbilityTwo",
            "SurrenderDancerAbilityThree",
            "SurrenderDancerAbilityFour",
            "SurrenderDancerWoundOne",
            "SurrenderDancerWoundTwo",
            "SurrenderDancerWoundThree",
            "SurrenderDancerWoundFour",
            "SurrenderDancerShockOne",
            "SurrenderDancerShockTwo",
            "SurrenderDancerShockThree",
            "SurrenderDancerShockFour",
            "SurrenderDancerKnowledgeOne",
            "SurrenderDancerKnowledgeTwo",
            "SurrenderDancerKnowledgeThree",
            "SurrenderDancerKnowledgeFour",
            "SurrenderDancerMaster",
            "SurrenderMusicianNovice",
            "SurrenderMusicianAbilityOne",
            "SurrenderMusicianAbilityTwo",
            "SurrenderMusicianAbilityThree",
            "SurrenderMusicianAbilityFour",
            "SurrenderMusicianWoundOne",
            "SurrenderMusicianWoundTwo",
            "SurrenderMusicianWoundThree",
            "SurrenderMusicianWoundFour",
            "SurrenderMusicianShockOne",
            "SurrenderMusicianShockTwo",
            "SurrenderMusicianShockThree",
            "SurrenderMusicianShockFour",
            "SurrenderMusicianKnowledgeOne",
            "SurrenderMusicianKnowledgeTwo",
            "SurrenderMusicianKnowledgeThree",
            "SurrenderMusicianKnowledgeFour",
            "SurrenderMusicianMaster",
            "ShowAllProfessions",
            "SelectAllProfession",
            "Stand",
        ):
            with self.subTest(action=action):
                self.assertIn(f'"{action}"', helper_parameters)
        helper_query = function_body(self.helper, "function Invoke-BridgeQuery")
        self.assertIn("SendMessageTimeout", helper_query)
        self.assertIn("[long]$Data = 0", helper_query)
        self.assertIn("return $queryResult.ToInt64()", helper_query)
        self.assertIn("0x43510000L", self.helper)
        self.assertIn("count=$queueCount", self.helper)
        self.assertIn("lastStatus=$lastStatus($lastStatusName)", self.helper)
        self.assertIn("lastDetail=$lastDetail", self.helper)
        self.assertIn("0x544d0000L", self.helper)
        self.assertIn("0x53500000L", self.helper)
        self.assertIn("selectedProfessionRow=$selectedRow", self.helper)
        self.assertIn("available=true command=$commandName", self.helper)
        self.assertIn("currentMs=$currentMilliseconds", self.helper)
        self.assertIn("maxMs=$maxMilliseconds", self.helper)
        self.assertIn("[ValidateRange(1, 16)]", helper_parameters)
        self.assertIn("-Data $Repeat", self.helper)

        performance_action = function_body(
            self.client_main,
            "bool performBackgroundPerformanceCommand(",
        )
        self.assertIn('!= "39008597"', performance_action)
        self.assertIn("ClientCommandQueue::enqueueCommand", performance_action)
        self.assertIn(
            "Immediate Publish 14 commands legitimately return sequence zero",
            performance_action,
        )
        self.assertIn("NetworkId::cms_invalid", performance_action)
        self.assertIn("Unicode::narrowToWide(commandParameters)", performance_action)
        for forbidden_mutation in (
            "grantSkill",
            "setAttrib",
            "setPerformanceType",
            "attachScript",
        ):
            with self.subTest(forbidden_mutation=forbidden_mutation):
                self.assertNotIn(forbidden_mutation, performance_action)
        self.assertIn(
            'performBackgroundPerformanceCommand(\n\t\t\t\t\t"startDance", "rhythmic")',
            self.client_main,
        )
        self.assertIn(
            'performBackgroundPerformanceCommand(\n\t\t\t\t\t"flourish", "1")',
            self.client_main,
        )
        self.assertIn(
            'performBackgroundPerformanceCommand(\n\t\t\t\t\t"stopDance", "")',
            self.client_main,
        )
        self.assertIn(
            'performBackgroundPerformanceCommand(\n\t\t\t\t\t"startMusic", "rock")',
            self.client_main,
        )
        self.assertIn(
            'performBackgroundPerformanceCommand(\n\t\t\t\t\t"startMusic", "starwars2")',
            self.client_main,
        )
        self.assertIn(
            'performBackgroundPerformanceCommand(\n\t\t\t\t\t"startMusic", "folk")',
            self.client_main,
        )

        surrender_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerMusicOne()",
        )
        self.assertIn('getValueString() != "39008597"', surrender_action)
        self.assertIn('"surrenderSkill"', surrender_action)
        self.assertIn(
            'Unicode::narrowToWide("social_entertainer_music_01")',
            surrender_action,
        )
        self.assertIn("ClientCommandQueue::enqueueCommand", surrender_action)
        self.assertIn("NetworkId::cms_invalid", surrender_action)
        self.assertNotIn("player->getNetworkId(),", surrender_action)
        for forbidden_mutation in (
            "grantSkill",
            "revokeSkill",
            "setAttrib",
            "setPerformanceType",
        ):
            with self.subTest(forbidden_mutation=forbidden_mutation):
                self.assertNotIn(forbidden_mutation, surrender_action)

        surrender_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerMusicTwo()",
        )
        self.assertIn('getValueString() != "39008597"', surrender_two_action)
        self.assertIn('"surrenderSkill"', surrender_two_action)
        self.assertIn(
            'Unicode::narrowToWide("social_entertainer_music_02")',
            surrender_two_action,
        )
        self.assertIn("ClientCommandQueue::enqueueCommand", surrender_two_action)
        self.assertIn("NetworkId::cms_invalid", surrender_two_action)
        self.assertNotIn("player->getNetworkId(),", surrender_two_action)

        surrender_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerMusicThree()",
        )
        self.assertIn('getValueString() != "39008597"', surrender_three_action)
        self.assertIn('"surrenderSkill"', surrender_three_action)
        self.assertIn(
            'Unicode::narrowToWide("social_entertainer_music_03")',
            surrender_three_action,
        )
        self.assertIn("NetworkId::cms_invalid", surrender_three_action)

        surrender_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerMusicFour()",
        )
        self.assertIn('getValueString() != "39008597"', surrender_four_action)
        self.assertIn('"surrenderSkill"', surrender_four_action)
        self.assertIn(
            'Unicode::narrowToWide("social_entertainer_music_04")',
            surrender_four_action,
        )
        self.assertIn("NetworkId::cms_invalid", surrender_four_action)

        surrender_master_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerMaster()",
        )
        self.assertIn('getValueString() != "39008597"', surrender_master_action)
        self.assertIn('"surrenderSkill"', surrender_master_action)
        self.assertIn(
            'Unicode::narrowToWide("social_entertainer_master")',
            surrender_master_action,
        )
        self.assertIn("NetworkId::cms_invalid", surrender_master_action)

        surrender_dance_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerDanceOne()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dance_one_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dance_one_action)
        self.assertIn(
            'Unicode::narrowToWide("social_entertainer_dance_01")',
            surrender_dance_one_action,
        )
        self.assertIn("NetworkId::cms_invalid", surrender_dance_one_action)

        surrender_dance_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerDanceTwo()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dance_two_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dance_two_action)
        self.assertIn(
            'Unicode::narrowToWide("social_entertainer_dance_02")',
            surrender_dance_two_action,
        )
        self.assertIn("NetworkId::cms_invalid", surrender_dance_two_action)

        surrender_dance_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerDanceThree()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dance_three_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dance_three_action)
        self.assertIn(
            'Unicode::narrowToWide("social_entertainer_dance_03")',
            surrender_dance_three_action,
        )
        self.assertIn("NetworkId::cms_invalid", surrender_dance_three_action)

        surrender_dance_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerDanceFour()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dance_four_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dance_four_action)
        self.assertIn(
            'Unicode::narrowToWide("social_entertainer_dance_04")',
            surrender_dance_four_action,
        )
        self.assertIn("NetworkId::cms_invalid", surrender_dance_four_action)

        surrender_hairstyle_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerHairstyleOne()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_hairstyle_one_action,
        )
        self.assertIn('"surrenderSkill"', surrender_hairstyle_one_action)
        self.assertIn(
            '"social_entertainer_hairstyle_01"',
            surrender_hairstyle_one_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_hairstyle_one_action,
        )

        surrender_hairstyle_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerHairstyleTwo()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_hairstyle_two_action,
        )
        self.assertIn('"surrenderSkill"', surrender_hairstyle_two_action)
        self.assertIn(
            '"social_entertainer_hairstyle_02"',
            surrender_hairstyle_two_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_hairstyle_two_action,
        )

        surrender_hairstyle_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerHairstyleThree()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_hairstyle_three_action,
        )
        self.assertIn('"surrenderSkill"', surrender_hairstyle_three_action)
        self.assertIn(
            '"social_entertainer_hairstyle_03"',
            surrender_hairstyle_three_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_hairstyle_three_action,
        )

        surrender_hairstyle_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderEntertainerHairstyleFour()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_hairstyle_four_action,
        )
        self.assertIn('"surrenderSkill"', surrender_hairstyle_four_action)
        self.assertIn(
            '"social_entertainer_hairstyle_04"',
            surrender_hairstyle_four_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_hairstyle_four_action,
        )

        surrender_dancer_novice_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerNovice()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_novice_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_novice_action)
        self.assertIn(
            '"social_dancer_novice"',
            surrender_dancer_novice_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_novice_action,
        )

        surrender_dancer_ability_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerAbilityOne()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_ability_one_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_ability_one_action)
        self.assertIn(
            '"social_dancer_ability_01"',
            surrender_dancer_ability_one_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_ability_one_action,
        )

        surrender_dancer_ability_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerAbilityTwo()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_ability_two_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_ability_two_action)
        self.assertIn(
            '"social_dancer_ability_02"',
            surrender_dancer_ability_two_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_ability_two_action,
        )

        surrender_dancer_ability_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerAbilityThree()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_ability_three_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_ability_three_action)
        self.assertIn(
            '"social_dancer_ability_03"',
            surrender_dancer_ability_three_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_ability_three_action,
        )

        surrender_dancer_ability_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerAbilityFour()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_ability_four_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_ability_four_action)
        self.assertIn(
            '"social_dancer_ability_04"',
            surrender_dancer_ability_four_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_ability_four_action,
        )

        surrender_dancer_wound_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerWoundOne()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_wound_one_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_wound_one_action)
        self.assertIn(
            '"social_dancer_wound_01"',
            surrender_dancer_wound_one_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_wound_one_action,
        )
        surrender_dancer_wound_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerWoundTwo()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_wound_two_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_wound_two_action)
        self.assertIn(
            '"social_dancer_wound_02"',
            surrender_dancer_wound_two_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_wound_two_action,
        )
        surrender_dancer_wound_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerWoundThree()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_wound_three_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_wound_three_action)
        self.assertIn(
            '"social_dancer_wound_03"',
            surrender_dancer_wound_three_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_wound_three_action,
        )
        surrender_dancer_wound_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerWoundFour()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_wound_four_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_wound_four_action)
        self.assertIn(
            '"social_dancer_wound_04"',
            surrender_dancer_wound_four_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_wound_four_action,
        )
        surrender_dancer_shock_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerShockOne()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_shock_one_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_shock_one_action)
        self.assertIn(
            '"social_dancer_shock_01"',
            surrender_dancer_shock_one_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_shock_one_action,
        )
        surrender_dancer_shock_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerShockTwo()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_shock_two_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_shock_two_action)
        self.assertIn(
            '"social_dancer_shock_02"',
            surrender_dancer_shock_two_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_shock_two_action,
        )
        surrender_dancer_shock_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerShockThree()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_shock_three_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_shock_three_action)
        self.assertIn(
            '"social_dancer_shock_03"',
            surrender_dancer_shock_three_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_shock_three_action,
        )
        surrender_dancer_shock_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerShockFour()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_shock_four_action,
        )
        self.assertIn('"surrenderSkill"', surrender_dancer_shock_four_action)
        self.assertIn(
            '"social_dancer_shock_04"',
            surrender_dancer_shock_four_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_shock_four_action,
        )
        surrender_dancer_knowledge_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerKnowledgeOne()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_knowledge_one_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_dancer_knowledge_one_action,
        )
        self.assertIn(
            '"social_dancer_knowledge_01"',
            surrender_dancer_knowledge_one_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_knowledge_one_action,
        )
        surrender_dancer_knowledge_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerKnowledgeTwo()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_knowledge_two_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_dancer_knowledge_two_action,
        )
        self.assertIn(
            '"social_dancer_knowledge_02"',
            surrender_dancer_knowledge_two_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_knowledge_two_action,
        )
        surrender_dancer_knowledge_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerKnowledgeThree()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_knowledge_three_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_dancer_knowledge_three_action,
        )
        self.assertIn(
            '"social_dancer_knowledge_03"',
            surrender_dancer_knowledge_three_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_knowledge_three_action,
        )
        surrender_dancer_knowledge_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerKnowledgeFour()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_knowledge_four_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_dancer_knowledge_four_action,
        )
        self.assertIn(
            '"social_dancer_knowledge_04"',
            surrender_dancer_knowledge_four_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_knowledge_four_action,
        )
        surrender_dancer_master_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderDancerMaster()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_dancer_master_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_dancer_master_action,
        )
        self.assertIn(
            '"social_dancer_master"',
            surrender_dancer_master_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_dancer_master_action,
        )
        surrender_musician_novice_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianNovice()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_novice_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_novice_action,
        )
        self.assertIn(
            '"social_musician_novice"',
            surrender_musician_novice_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_novice_action,
        )
        surrender_musician_ability_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianAbilityOne()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_ability_one_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_ability_one_action,
        )
        self.assertIn(
            '"social_musician_ability_01"',
            surrender_musician_ability_one_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_ability_one_action,
        )
        surrender_musician_ability_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianAbilityTwo()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_ability_two_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_ability_two_action,
        )
        self.assertIn(
            '"social_musician_ability_02"',
            surrender_musician_ability_two_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_ability_two_action,
        )
        surrender_musician_ability_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianAbilityThree()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_ability_three_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_ability_three_action,
        )
        self.assertIn(
            '"social_musician_ability_03"',
            surrender_musician_ability_three_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_ability_three_action,
        )
        surrender_musician_ability_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianAbilityFour()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_ability_four_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_ability_four_action,
        )
        self.assertIn(
            '"social_musician_ability_04"',
            surrender_musician_ability_four_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_ability_four_action,
        )
        surrender_musician_wound_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianWoundOne()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_wound_one_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_wound_one_action,
        )
        self.assertIn(
            '"social_musician_wound_01"',
            surrender_musician_wound_one_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_wound_one_action,
        )
        surrender_musician_wound_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianWoundTwo()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_wound_two_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_wound_two_action,
        )
        self.assertIn(
            '"social_musician_wound_02"',
            surrender_musician_wound_two_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_wound_two_action,
        )
        surrender_musician_wound_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianWoundThree()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_wound_three_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_wound_three_action,
        )
        self.assertIn(
            '"social_musician_wound_03"',
            surrender_musician_wound_three_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_wound_three_action,
        )
        surrender_musician_wound_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianWoundFour()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_wound_four_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_wound_four_action,
        )
        self.assertIn(
            '"social_musician_wound_04"',
            surrender_musician_wound_four_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_wound_four_action,
        )
        surrender_musician_shock_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianShockOne()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_shock_one_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_shock_one_action,
        )
        self.assertIn(
            '"social_musician_shock_01"',
            surrender_musician_shock_one_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_shock_one_action,
        )
        surrender_musician_shock_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianShockTwo()",
        )
        self.assertIn(
            'getValueString() != "39008597"',
            surrender_musician_shock_two_action,
        )
        self.assertIn(
            '"surrenderSkill"',
            surrender_musician_shock_two_action,
        )
        self.assertIn(
            '"social_musician_shock_02"',
            surrender_musician_shock_two_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_shock_two_action,
        )
        surrender_musician_shock_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianShockThree()",
        )
        self.assertIn(
            '"social_musician_shock_03"',
            surrender_musician_shock_three_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_shock_three_action,
        )
        surrender_musician_shock_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianShockFour()",
        )
        self.assertIn(
            '"social_musician_shock_04"',
            surrender_musician_shock_four_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_shock_four_action,
        )
        surrender_musician_knowledge_one_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianKnowledgeOne()",
        )
        self.assertIn(
            '"social_musician_knowledge_01"',
            surrender_musician_knowledge_one_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_knowledge_one_action,
        )
        surrender_musician_knowledge_two_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianKnowledgeTwo()",
        )
        self.assertIn(
            '"social_musician_knowledge_02"',
            surrender_musician_knowledge_two_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_knowledge_two_action,
        )
        surrender_musician_knowledge_three_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianKnowledgeThree()",
        )
        self.assertIn(
            '"social_musician_knowledge_03"',
            surrender_musician_knowledge_three_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_knowledge_three_action,
        )
        surrender_musician_knowledge_four_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianKnowledgeFour()",
        )
        self.assertIn(
            '"social_musician_knowledge_04"',
            surrender_musician_knowledge_four_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_knowledge_four_action,
        )
        surrender_musician_master_action = function_body(
            self.client_main,
            "bool performBackgroundSurrenderMusicianMaster()",
        )
        self.assertIn(
            '"social_musician_master"',
            surrender_musician_master_action,
        )
        self.assertIn(
            "NetworkId::cms_invalid",
            surrender_musician_master_action,
        )
        self.assertIn('"startDance", "popular"', self.client_main)

        equip_action = function_body(
            self.client_main, "bool performBackgroundEquipCdefRifle()"
        )
        self.assertIn(
            'performBackgroundEquipCdefWeapon("rifle_cdef.iff")', equip_action
        )

        tier_one_queue = function_body(
            self.client_main,
            "bool performBackgroundQueueMarksmanTier1(char const * const command, int const repeat)",
        )
        self.assertIn('getValueString() != "44003778"', tier_one_queue)
        self.assertIn('NetworkId const targetId("39008597")', tier_one_queue)
        self.assertIn("ClientCommandQueue::commandsAreNowFromToolbar(true)", tier_one_queue)
        self.assertIn("ClientCommandQueue::enqueueCommand(", tier_one_queue)
        self.assertNotIn("doDamage", tier_one_queue)

        generic_equip = function_body(
            self.client_main,
            "bool performBackgroundEquipCdefWeapon(char const * const templateSuffix)",
        )
        self.assertIn('playerId != "44003778"', generic_equip)
        self.assertIn('playerId != "39008597"', generic_equip)
        self.assertIn("ContainerInterface::getContainer", generic_equip)
        self.assertIn("strstr(templateName, templateSuffix)", generic_equip)
        self.assertIn(
            "return CuiInventoryManager::equipObject(object->getNetworkId())",
            generic_equip,
        )
        for suffix in (
            "rifle_cdef.iff",
            "pistol_cdef.iff",
            "carbine_cdef.iff",
            "pistol_dl44.iff",
            "pistol_dl44_metal.iff",
        ):
            self.assertIn(f'"{suffix}"', self.client_main)

        self.assertIn('performBackgroundSelfCommand("/stand")', window_proc)
        self.assertIn("queued through the production stand command", self.helper)

        timer_query = function_body(
            self.client_main, "LRESULT getBackgroundCombatTimerStatus()"
        )
        self.assertIn("s_backgroundCombatTimerCapture.executeCurrent", timer_query)
        self.assertIn("s_backgroundCombatTimerCapture.executeMax", timer_query)
        self.assertNotIn("m_execTime", timer_query)
        self.assertIn(
            "PlayerCreatureController::Messages::CommandTimerDataReceived",
            self.client_main,
        )
        self.assertIn(
            "commandTimerData.getMaxTime(MessageQueueCommandTimer::F_execute)",
            self.client_main,
        )
        self.assertIn(
            'Crc::normalizeAndCalculate("headShot2")',
            self.client_main,
        )
        self.assertIn('"headShot2"', self.helper)

    def test_command_queue_preserves_the_last_authoritative_removal_result(self):
        self.assertIn("clearLastCommandRemoval()", self.command_queue_header)
        self.assertIn("getLastCommandRemoval", self.command_queue_header)
        removal = function_body(
            self.command_queue_source,
            "void ClientCommandQueue::handleCommandRemoved(uint32 sequenceId, float waitTime, Command::ErrorCode status, int statusDetail)",
        )
        self.assertIn("ms_lastCommandRemovalValid = true", removal)
        self.assertIn("ms_lastCommandRemovalSequenceId = sequenceId", removal)
        self.assertIn("ms_lastCommandRemovalStatus = status", removal)
        self.assertIn("ms_lastCommandRemovalStatusDetail = statusDetail", removal)
        queue_action = function_body(
            self.client_main, "bool performBackgroundQueueCombatCanary(int const repeat)"
        )
        self.assertLess(
            queue_action.index("ClientCommandQueue::clearLastCommandRemoval()"),
            queue_action.index("ClientCommandQueue::enqueueCommand"),
        )

    def test_plain_key_sequence_accepts_no_modifiers(self):
        sequence = function_body(self.helper, "function Send-BridgeKeySequence")
        powershell = rf"""
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$command = @{{ KeyDown = 8; KeyUp = 9; InputReset = 11 }}
{sequence}
$script:queued = [System.Collections.Generic.List[string]]::new()
function Send-BridgeCommand {{
    param([IntPtr]$Window, [uint32]$Message, [int]$Command, [long]$Data = 0)
    $script:queued.Add("$Command`:$Data")
}}
Send-BridgeKeySequence -Window ([IntPtr]1) -Message 1 -KeyCode 1
@($script:queued) | ConvertTo-Json -Compress
"""
        completed = subprocess.run(
            ["powershell", "-NoProfile", "-Command", powershell],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(0, completed.returncode, completed.stderr)
        self.assertEqual(["8:1", "9:1"], json.loads(completed.stdout))

    def test_chord_sequence_releases_keys_and_resets_after_each_post_failure(self):
        sequence = function_body(self.helper, "function Send-BridgeKeySequence")
        powershell = rf"""
$ErrorActionPreference = "Stop"
$command = @{{ KeyDown = 8; KeyUp = 9; InputReset = 11 }}
{sequence}
$results = @()
foreach ($failAt in @(0, 1, 2, 3, 4, 5, 6)) {{
    $script:postIndex = 0
    $script:failAt = $failAt
    $script:queued = [System.Collections.Generic.List[string]]::new()
    function Send-BridgeCommand {{
        param([IntPtr]$Window, [uint32]$Message, [int]$Command, [long]$Data = 0)
        ++$script:postIndex
        if ($script:failAt -gt 0 -and $script:postIndex -eq $script:failAt) {{
            throw "injected post failure $script:postIndex"
        }}
        $script:queued.Add("$Command`:$Data")
    }}
    $failed = $false
    try {{
        Send-BridgeKeySequence -Window ([IntPtr]1) -Message 1 -KeyCode 31 -Modifiers @(29, 42)
    }} catch {{
        $failed = $true
    }}
    $results += [pscustomobject]@{{
        FailAt = $failAt
        Failed = $failed
        Queued = @($script:queued)
    }}
}}
$results | ConvertTo-Json -Depth 4 -Compress
"""
        result = subprocess.run(
            [
                "powershell.exe",
                "-NoProfile",
                "-NonInteractive",
                "-EncodedCommand",
                base64.b64encode(powershell.encode("utf-16le")).decode("ascii"),
            ],
            text=True,
            capture_output=True,
            check=True,
        )
        scenarios = {item["FailAt"]: item for item in json.loads(result.stdout)}
        expected = {
            0: ["8:29", "8:42", "8:31", "9:31", "9:42", "9:29"],
            1: ["11:0"],
            2: ["8:29", "9:29", "11:0"],
            3: ["8:29", "8:42", "9:42", "9:29", "11:0"],
            4: ["8:29", "8:42", "8:31", "9:31", "9:42", "9:29", "11:0"],
            5: ["8:29", "8:42", "8:31", "9:31", "9:29", "11:0"],
            6: ["8:29", "8:42", "8:31", "9:31", "9:42", "11:0"],
        }
        for fail_at, queued in expected.items():
            with self.subTest(fail_at=fail_at):
                self.assertEqual(fail_at != 0, scenarios[fail_at]["Failed"])
                self.assertEqual(queued, scenarios[fail_at]["Queued"])


if __name__ == "__main__":
    unittest.main()
