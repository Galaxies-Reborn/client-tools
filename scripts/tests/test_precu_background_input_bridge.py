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

    def test_hidden_start_uses_a_process_local_fallback_window(self):
        install_start = self.client_main.index("bool installBackgroundInputBridge()")
        install_end = self.client_main.index(
            "void removeBackgroundInputBridge()", install_start
        )
        install_body = self.client_main[install_start:install_end]
        remove_body = function_body(
            self.client_main, "void removeBackgroundInputBridge()"
        )

        self.assertIn("HWND window = Os::getWindow();", install_body)
        self.assertIn("if (!window || !IsWindowVisible(window))", install_body)
        self.assertIn('CreateWindowExA(', install_body)
        self.assertIn('"SWGSource Pre-CU Background Input"', install_body)
        self.assertIn("WS_POPUP", install_body)
        self.assertIn("s_backgroundInputOwnsWindow = ownsWindow;", install_body)
        self.assertIn("DestroyWindow(s_backgroundInputWindow);", remove_body)

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
            "BIC_queueAreaTrack",
            "BIC_selectAreaTrackType",
            "BIC_equipFixtureFlame",
            "BIC_queueFlameSingle1",
            "BIC_flameSingle1WeaponStatus",
            "BIC_queueFlameSingle2",
            "BIC_flameSingle2WeaponStatus",
            "BIC_queueFlameCone1",
            "BIC_flameCone1WeaponStatus",
            "BIC_queueFlameCone2",
            "BIC_flameCone2WeaponStatus",
            "BIC_queueHealthShot1",
            "BIC_healthShot1WeaponStatus",
            "BIC_queueMindShot1",
            "BIC_mindShot1WeaponStatus",
            "BIC_queueActionShot1",
            "BIC_actionShot1WeaponStatus",
            "BIC_queueActionShot2",
            "BIC_actionShot2WeaponStatus",
            "BIC_queueOverChargeShot1",
            "BIC_overChargeShot1WeaponStatus",
            "BIC_queuePointBlankSingle1",
            "BIC_pointBlankSingle1WeaponStatus",
            "BIC_queueThreatenShot",
            "BIC_threatenShotWeaponStatus",
            "BIC_queueWarningShot",
            "BIC_warningShotWeaponStatus",
            "BIC_queueAim",
            "BIC_aimWeaponStatus",
            "BIC_queueSuppressionFire1",
            "BIC_suppressionFire1WeaponStatus",
            "BIC_queueRollShot",
            "BIC_rollShotWeaponStatus",
            "BIC_queueDiveShot",
            "BIC_diveShotWeaponStatus",
            "BIC_queueKipUpShot",
            "BIC_kipUpShotWeaponStatus",
            "BIC_queueTakeCover",
            "BIC_takeCoverWeaponStatus",
            "BIC_queueFullAutoSingle1",
            "BIC_fullAutoSingle1WeaponStatus",
            "BIC_queueScatterShot1",
            "BIC_scatterShot1WeaponStatus",
            "BIC_queueScatterShot2",
            "BIC_scatterShot2WeaponStatus",
            "BIC_queueLegShot2",
            "BIC_legShot2WeaponStatus",
            "BIC_queueLegShot3",
            "BIC_legShot3WeaponStatus",
        ]
        positions = [self.client_main.index(command) for command in expected_commands]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("cms_backgroundInputProtocolVersion = 203", self.client_main)
        self.assertIn("$expectedProtocolVersion = 203", self.helper)

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

    def test_bridge_exposes_core3_one_hand_body_hit_one(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"melee1hBodyHit1"',
            'getBackgroundGeneratedCombatWeaponStatus("melee1hBodyHit1")',
            "BIC_queueMelee1hBodyHit1",
            "BIC_melee1hBodyHit1WeaponStatus",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"melee1hBodyHit2"',
            'getBackgroundGeneratedCombatWeaponStatus("melee1hBodyHit2")',
            "BIC_queueMelee1hBodyHit2",
            "BIC_melee1hBodyHit2WeaponStatus",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"melee1hBodyHit3"',
            'getBackgroundGeneratedCombatWeaponStatus("melee1hBodyHit3")',
            "BIC_queueMelee1hBodyHit3",
            "BIC_melee1hBodyHit3WeaponStatus",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueMelee1hBodyHit1"',
            '"Melee1hBodyHit1WeaponStatus"',
            "QueueMelee1hBodyHit1 = 140",
            "Melee1hBodyHit1WeaponStatus = 141",
            '"QueueMelee1hBodyHit2"',
            '"Melee1hBodyHit2WeaponStatus"',
            "QueueMelee1hBodyHit2 = 142",
            "Melee1hBodyHit2WeaponStatus = 143",
            '"QueueMelee1hBodyHit3"',
            '"Melee1hBodyHit3WeaponStatus"',
            "QueueMelee1hBodyHit3 = 144",
            "Melee1hBodyHit3WeaponStatus = 145",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_two_hand_head_hit_continuation(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"melee2hHeadHit1"',
            'getBackgroundGeneratedCombatWeaponStatus("melee2hHeadHit1")',
            "BIC_queueMelee2hHeadHit1",
            "BIC_melee2hHeadHit1WeaponStatus",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"melee2hHeadHit2"',
            'getBackgroundGeneratedCombatWeaponStatus("melee2hHeadHit2")',
            "BIC_queueMelee2hHeadHit2",
            "BIC_melee2hHeadHit2WeaponStatus",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"melee2hHeadHit3"',
            'getBackgroundGeneratedCombatWeaponStatus("melee2hHeadHit3")',
            "BIC_queueMelee2hHeadHit3",
            "BIC_melee2hHeadHit3WeaponStatus",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueMelee2hHeadHit1"',
            '"Melee2hHeadHit1WeaponStatus"',
            "QueueMelee2hHeadHit1 = 146",
            "Melee2hHeadHit1WeaponStatus = 147",
            '"QueueMelee2hHeadHit2"',
            '"Melee2hHeadHit2WeaponStatus"',
            "QueueMelee2hHeadHit2 = 148",
            "Melee2hHeadHit2WeaponStatus = 149",
            '"QueueMelee2hHeadHit3"',
            '"Melee2hHeadHit3WeaponStatus"',
            "QueueMelee2hHeadHit3 = 150",
            "Melee2hHeadHit3WeaponStatus = 151",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_basic_melee_hit_family(self):
        commands = [
            ("melee1hHit1", "QueueMelee1hHit1", 152),
            ("melee1hHit2", "QueueMelee1hHit2", 154),
            ("melee2hHit1", "QueueMelee2hHit1", 156),
            ("melee2hHit2", "QueueMelee2hHit2", 158),
        ]
        for command, action, number in commands:
            status_action = action.replace("Queue", "") + "WeaponStatus"
            client_enum = "BIC_" + action[0].lower() + action[1:]
            client_status_enum = "BIC_" + status_action[0].lower() + status_action[1:]
            for token in [
                command,
                client_enum,
                client_status_enum,
            ]:
                with self.subTest(command=command, client_token=token):
                    self.assertIn(token, self.client_main)
            for token in [
                f'"{action}"',
                f'"{status_action}"',
                f"{action} = {number}",
                f"{status_action} = {number + 1}",
            ]:
                with self.subTest(command=command, helper_token=token):
                    self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_polearm_leg_hit_continuation(self):
        commands = [
            ("polearmLegHit2", "QueuePolearmLegHit2", 160),
            ("polearmLegHit3", "QueuePolearmLegHit3", 162),
        ]
        for command, action, number in commands:
            status_action = action.replace("Queue", "") + "WeaponStatus"
            for token in [
                command,
                "BIC_" + action[0].lower() + action[1:],
                "BIC_" + status_action[0].lower() + status_action[1:],
            ]:
                with self.subTest(command=command, client_token=token):
                    self.assertIn(token, self.client_main)
            for token in [
                f'"{action}"',
                f'"{status_action}"',
                f"{action} = {number}",
                f"{status_action} = {number + 1}",
            ]:
                with self.subTest(command=command, helper_token=token):
                    self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_polearm_hit_and_area_family(self):
        commands = [
            ("polearmHit1", "QueuePolearmHit1", 164),
            ("polearmArea1", "QueuePolearmArea1", 166),
        ]
        for command, action, number in commands:
            status_action = action.replace("Queue", "") + "WeaponStatus"
            for token in [
                command,
                "BIC_" + action[0].lower() + action[1:],
                "BIC_" + status_action[0].lower() + status_action[1:],
            ]:
                with self.subTest(command=command, client_token=token):
                    self.assertIn(token, self.client_main)
            for token in [
                f'"{action}"',
                f'"{status_action}"',
                f"{action} = {number}",
                f"{status_action} = {number + 1}",
            ]:
                with self.subTest(command=command, helper_token=token):
                    self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_two_hand_spin_attack_continuation(self):
        command = "melee2hSpinAttack2"
        action = "QueueMelee2hSpinAttack2"
        status_action = "Melee2hSpinAttack2WeaponStatus"
        for token in [
            command,
            "BIC_queueMelee2hSpinAttack2",
            "BIC_melee2hSpinAttack2WeaponStatus",
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            f'"{action}"',
            f'"{status_action}"',
            f"{action} = 168",
            f"{status_action} = 169",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_burst_shot_one(self):
        for token in [
            "burstShot1",
            "BIC_queueBurstShot1",
            "BIC_burstShot1WeaponStatus",
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueBurstShot1"',
            '"BurstShot1WeaponStatus"',
            "QueueBurstShot1 = 170",
            "BurstShot1WeaponStatus = 171",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_disarming_shot_one(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"disarmingShot1"',
            'getBackgroundGeneratedCombatWeaponStatus("disarmingShot1")',
            "BIC_queueDisarmingShot1",
            "BIC_disarmingShot1WeaponStatus",
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueDisarmingShot1"',
            '"DisarmingShot1WeaponStatus"',
            "QueueDisarmingShot1 = 172",
            "DisarmingShot1WeaponStatus = 173",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_double_tap(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"doubleTap"',
            'getBackgroundGeneratedCombatWeaponStatus("doubleTap")',
            "BIC_queueDoubleTap",
            "BIC_doubleTapWeaponStatus",
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueDoubleTap"',
            '"DoubleTapWeaponStatus"',
            "QueueDoubleTap = 174",
            "DoubleTapWeaponStatus = 175",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_stopping_shot(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"stoppingShot"',
            'getBackgroundGeneratedCombatWeaponStatus("stoppingShot")',
            "BIC_queueStoppingShot",
            "BIC_stoppingShotWeaponStatus",
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueStoppingShot"',
            '"StoppingShotWeaponStatus"',
            "QueueStoppingShot = 176",
            "StoppingShotWeaponStatus = 177",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_crippling_shot(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"cripplingShot"',
            'getBackgroundGeneratedCombatWeaponStatus("cripplingShot")',
            "BIC_queueCripplingShot",
            "BIC_cripplingShotWeaponStatus",
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueCripplingShot"',
            '"CripplingShotWeaponStatus"',
            "QueueCripplingShot = 178",
            "CripplingShotWeaponStatus = 179",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_point_blank_single_two(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"pointBlankSingle2"',
            'getBackgroundGeneratedCombatWeaponStatus("pointBlankSingle2")',
            "BIC_queuePointBlankSingle2",
            "BIC_pointBlankSingle2WeaponStatus",
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueuePointBlankSingle2"',
            '"PointBlankSingle2WeaponStatus"',
            "QueuePointBlankSingle2 = 180",
            "PointBlankSingle2WeaponStatus = 181",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_point_blank_area_one(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"pointBlankArea1"',
            'getBackgroundGeneratedCombatWeaponStatus("pointBlankArea1")',
            "BIC_queuePointBlankArea1",
            "BIC_pointBlankArea1WeaponStatus",
            "uint64 result = 0x00005400ULL;",
            "result |= static_cast<uint64>(static_cast<uint32>(command.m_weaponTypesValid)) << 16;",
            "result |= static_cast<uint64>(command.m_weaponTypesInvalid & 0xffffU) << 48;",
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueuePointBlankArea1"',
            '"PointBlankArea1WeaponStatus"',
            "QueuePointBlankArea1 = 182",
            "PointBlankArea1WeaponStatus = 183",
            "($PackedStatus -band 0xfc00L) -ne 0x5400L",
            "$validMask = ($PackedStatus -shr 16) -band 0xffffffffL",
            "validMask=0x$($validMask.ToString('x8'))",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_point_blank_area_two(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"pointBlankArea2"',
            'getBackgroundGeneratedCombatWeaponStatus("pointBlankArea2")',
            "BIC_queuePointBlankArea2",
            "BIC_pointBlankArea2WeaponStatus",
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueuePointBlankArea2"',
            '"PointBlankArea2WeaponStatus"',
            "QueuePointBlankArea2 = 184",
            "PointBlankArea2WeaponStatus = 185",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_multi_target_pistol_shot(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"multiTargetPistolShot"',
            'getBackgroundGeneratedCombatWeaponStatus("multiTargetPistolShot")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueMultiTargetPistolShot"',
            '"MultiTargetPistolShotWeaponStatus"',
            "QueueMultiTargetPistolShot = 186",
            "MultiTargetPistolShotWeaponStatus = 187",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_disarming_shot_two(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"disarmingShot2"',
            'getBackgroundGeneratedCombatWeaponStatus("disarmingShot2")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueDisarmingShot2"',
            '"DisarmingShot2WeaponStatus"',
            "QueueDisarmingShot2 = 188",
            "DisarmingShot2WeaponStatus = 189",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_fan_shot(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"fanShot"',
            'getBackgroundGeneratedCombatWeaponStatus("fanShot")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in ['"QueueFanShot"', '"FanShotWeaponStatus"', "QueueFanShot = 190", "FanShotWeaponStatus = 191"]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_burst_shot_two(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"burstShot2"',
            'getBackgroundGeneratedCombatWeaponStatus("burstShot2")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueBurstShot2"',
            '"BurstShot2WeaponStatus"',
            "QueueBurstShot2 = 192",
            "BurstShot2WeaponStatus = 193",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_unarmed_hit_one(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"unarmedHit1"',
            'getBackgroundGeneratedCombatWeaponStatus("unarmedHit1")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueUnarmedHit1"',
            '"UnarmedHit1WeaponStatus"',
            "QueueUnarmedHit1 = 194",
            "UnarmedHit1WeaponStatus = 195",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_unarmed_hit_two(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"unarmedHit2"',
            'getBackgroundGeneratedCombatWeaponStatus("unarmedHit2")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueUnarmedHit2"',
            '"UnarmedHit2WeaponStatus"',
            "QueueUnarmedHit2 = 196",
            "UnarmedHit2WeaponStatus = 197",
        ]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_unarmed_body_hit_one(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"unarmedBodyHit1"',
            'getBackgroundGeneratedCombatWeaponStatus("unarmedBodyHit1")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in ['"QueueUnarmedBodyHit1"', '"UnarmedBodyHit1WeaponStatus"',
                      "QueueUnarmedBodyHit1 = 198", "UnarmedBodyHit1WeaponStatus = 199"]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_unarmed_leg_hit_one(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"unarmedLegHit1"',
            'getBackgroundGeneratedCombatWeaponStatus("unarmedLegHit1")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in ['"QueueUnarmedLegHit1"', '"UnarmedLegHit1WeaponStatus"',
                      "QueueUnarmedLegHit1 = 200", "UnarmedLegHit1WeaponStatus = 201"]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_unarmed_spin_attack_one(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"unarmedSpinAttack1"',
            'getBackgroundGeneratedCombatWeaponStatus("unarmedSpinAttack1")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in ['"QueueUnarmedSpinAttack1"', '"UnarmedSpinAttack1WeaponStatus"',
                      "QueueUnarmedSpinAttack1 = 202", "UnarmedSpinAttack1WeaponStatus = 203"]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_unarmed_spin_attack_two(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"unarmedSpinAttack2"',
            'getBackgroundGeneratedCombatWeaponStatus("unarmedSpinAttack2")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in ['"QueueUnarmedSpinAttack2"', '"UnarmedSpinAttack2WeaponStatus"',
                      "QueueUnarmedSpinAttack2 = 204", "UnarmedSpinAttack2WeaponStatus = 205"]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_overcharge_shot_two(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"overChargeShot2"',
            'getBackgroundGeneratedCombatWeaponStatus("overChargeShot2")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in ['"QueueOverChargeShot2"', '"OverChargeShot2WeaponStatus"',
                      "QueueOverChargeShot2 = 206", "OverChargeShot2WeaponStatus = 207"]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_fire_acid_single_one(self):
        for token in [
            'performBackgroundEquipCdefWeapon("heavy_acid_beam.iff")',
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"fireAcidSingle1"',
            'getBackgroundGeneratedCombatWeaponStatus("fireAcidSingle1")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in ['"EquipFixtureAcid"', '"QueueFireAcidSingle1"',
                      '"FireAcidSingle1WeaponStatus"', "EquipFixtureAcid = 208",
                      "QueueFireAcidSingle1 = 209", "FireAcidSingle1WeaponStatus = 210"]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_fire_lightning_single_one(self):
        for token in [
            'performBackgroundEquipCdefWeapon("rifle_lightning.iff")',
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"fireLightningSingle1"',
            'getBackgroundGeneratedCombatWeaponStatus("fireLightningSingle1")',
        ]:
            with self.subTest(client_token=token):
                self.assertIn(token, self.client_main)
        for token in ['"EquipFixtureLightning"', '"QueueFireLightningSingle1"',
                      '"FireLightningSingle1WeaponStatus"', "EquipFixtureLightning = 211",
                      "QueueFireLightningSingle1 = 212", "FireLightningSingle1WeaponStatus = 213"]:
            with self.subTest(helper_token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_special_heavy_family_closure(self):
        commands = [
            ("fireAcidCone1", "QueueFireAcidCone1", "FireAcidCone1WeaponStatus", 214),
            ("fireAcidCone2", "QueueFireAcidCone2", "FireAcidCone2WeaponStatus", 216),
            ("fireAcidSingle2", "QueueFireAcidSingle2", "FireAcidSingle2WeaponStatus", 218),
            ("fireLightningCone1", "QueueFireLightningCone1", "FireLightningCone1WeaponStatus", 220),
            ("fireLightningCone2", "QueueFireLightningCone2", "FireLightningCone2WeaponStatus", 222),
            ("fireLightningSingle2", "QueueFireLightningSingle2", "FireLightningSingle2WeaponStatus", 224),
        ]
        for command, queue_action, status_action, queue_id in commands:
            with self.subTest(command=command):
                self.assertIn(
                    'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"' + command + '"',
                    self.client_main,
                )
                self.assertIn(
                    'getBackgroundGeneratedCombatWeaponStatus("' + command + '")',
                    self.client_main,
                )
                self.assertIn('"' + queue_action + '"', self.helper)
                self.assertIn('"' + status_action + '"', self.helper)
                self.assertIn(queue_action + " = " + str(queue_id), self.helper)
                self.assertIn(status_action + " = " + str(queue_id + 1), self.helper)

    def test_bridge_exposes_core3_flame_dot_family(self):
        self.assertIn(
            'performBackgroundEquipCdefWeapon("rifle_flame_thrower.iff")',
            self.client_main,
        )
        self.assertIn("EquipFixtureFlame = 243", self.helper)
        commands = [
            ("flameSingle1", "QueueFlameSingle1", "FlameSingle1WeaponStatus", 244),
            ("flameSingle2", "QueueFlameSingle2", "FlameSingle2WeaponStatus", 246),
            ("flameCone1", "QueueFlameCone1", "FlameCone1WeaponStatus", 248),
            ("flameCone2", "QueueFlameCone2", "FlameCone2WeaponStatus", 250),
        ]
        for command, queue_action, status_action, queue_id in commands:
            with self.subTest(command=command):
                self.assertIn(
                    'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"' + command + '"',
                    self.client_main,
                )
                self.assertIn(
                    'getBackgroundGeneratedCombatWeaponStatus("' + command + '")',
                    self.client_main,
                )
                self.assertIn('"' + queue_action + '"', self.helper)
                self.assertIn('"' + status_action + '"', self.helper)
                self.assertIn(queue_action + " = " + str(queue_id), self.helper)
                self.assertIn(status_action + " = " + str(queue_id + 1), self.helper)

    def test_bridge_exposes_pool_specific_bleeding_shots(self):
        commands = [
            ("healthShot1", "QueueHealthShot1", "HealthShot1WeaponStatus", 252),
            ("mindShot1", "QueueMindShot1", "MindShot1WeaponStatus", 254),
        ]
        for command, queue_action, status_action, queue_id in commands:
            with self.subTest(command=command):
                self.assertIn(
                    'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"' + command + '"',
                    self.client_main,
                )
                self.assertIn(
                    'getBackgroundGeneratedCombatWeaponStatus("' + command + '")',
                    self.client_main,
                )
                self.assertIn('"' + queue_action + '"', self.helper)
                self.assertIn('"' + status_action + '"', self.helper)
                self.assertIn(queue_action + " = " + str(queue_id), self.helper)
                self.assertIn(status_action + " = " + str(queue_id + 1), self.helper)

    def test_bridge_exposes_core3_action_shot_posture_down(self):
        command = "actionShot1"
        queue_action = "QueueActionShot1"
        status_action = "ActionShot1WeaponStatus"
        self.assertIn(
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"actionShot1"',
            self.client_main,
        )
        self.assertIn(
            'getBackgroundGeneratedCombatWeaponStatus("actionShot1")',
            self.client_main,
        )
        self.assertIn('"' + queue_action + '"', self.helper)
        self.assertIn('"' + status_action + '"', self.helper)
        self.assertIn(queue_action + " = 256", self.helper)
        self.assertIn(status_action + " = 257", self.helper)

    def test_bridge_exposes_core3_action_shot_two_cone(self):
        command = "actionShot2"
        queue_action = "QueueActionShot2"
        status_action = "ActionShot2WeaponStatus"
        self.assertIn(
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"actionShot2"',
            self.client_main,
        )
        self.assertIn(
            'getBackgroundGeneratedCombatWeaponStatus("actionShot2")',
            self.client_main,
        )
        self.assertIn('"' + queue_action + '"', self.helper)
        self.assertIn('"' + status_action + '"', self.helper)
        self.assertIn(queue_action + " = 258", self.helper)
        self.assertIn(status_action + " = 259", self.helper)

    def test_bridge_exposes_core3_marksman_novice_shot_closure(self):
        commands = [
            ("overChargeShot1", "QueueOverChargeShot1", "OverChargeShot1WeaponStatus", 260),
            ("pointBlankSingle1", "QueuePointBlankSingle1", "PointBlankSingle1WeaponStatus", 262),
        ]
        for command, queue_action, status_action, command_id in commands:
            with self.subTest(command=command):
                self.assertIn(
                    'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"' + command + '"',
                    self.client_main,
                )
                self.assertIn(
                    'getBackgroundGeneratedCombatWeaponStatus("' + command + '")',
                    self.client_main,
                )
                self.assertIn('"' + queue_action + '"', self.helper)
                self.assertIn('"' + status_action + '"', self.helper)
                self.assertIn(queue_action + " = " + str(command_id), self.helper)
                self.assertIn(status_action + " = " + str(command_id + 1), self.helper)

    def test_bridge_exposes_core3_marksman_support_shots(self):
        commands = [
            ("threatenShot", "QueueThreatenShot", "ThreatenShotWeaponStatus", 264),
            ("warningShot", "QueueWarningShot", "WarningShotWeaponStatus", 266),
        ]
        for command, queue_action, status_action, command_id in commands:
            with self.subTest(command=command):
                self.assertIn(
                    'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"' + command + '"',
                    self.client_main,
                )
                self.assertIn(
                    'getBackgroundGeneratedCombatWeaponStatus("' + command + '")',
                    self.client_main,
                )
                self.assertIn('"' + queue_action + '"', self.helper)
                self.assertIn('"' + status_action + '"', self.helper)
                self.assertIn(queue_action + " = " + str(command_id), self.helper)
                self.assertIn(status_action + " = " + str(command_id + 1), self.helper)

    def test_bridge_exposes_core3_aim_lifecycle(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"aim"',
            'getBackgroundGeneratedCombatWeaponStatus("aim")',
            "BIC_queueAim",
            "BIC_aimWeaponStatus",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueAim"',
            '"AimWeaponStatus"',
            "QueueAim = 268",
            "AimWeaponStatus = 269",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_suppression_fire_one(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"suppressionFire1"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"suppressionFire1")',
            "BIC_queueSuppressionFire1",
            "BIC_suppressionFire1WeaponStatus",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueSuppressionFire1"',
            '"SuppressionFire1WeaponStatus"',
            "QueueSuppressionFire1 = 270",
            "SuppressionFire1WeaponStatus = 271",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_pistol_acrobatics(self):
        for command, enum_name in [
            ("rollShot", "RollShot"),
            ("diveShot", "DiveShot"),
            ("kipUpShot", "KipUpShot"),
        ]:
            with self.subTest(command=command):
                self.assertIn(
                    f'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"{command}"',
                    self.client_main,
                )
                self.assertIn(
                    f'getBackgroundGeneratedCombatWeaponStatus("{command}")',
                    self.client_main,
                )
                self.assertIn(f"BIC_queue{enum_name}", self.client_main)
                self.assertIn(
                    f"BIC_{command}WeaponStatus", self.client_main
                )
                self.assertIn(f'"Queue{enum_name}"', self.helper)
                self.assertIn(f'"{enum_name}WeaponStatus"', self.helper)
        for token in [
            "QueueRollShot = 272",
            "RollShotWeaponStatus = 273",
            "QueueDiveShot = 274",
            "DiveShotWeaponStatus = 275",
            "QueueKipUpShot = 276",
            "KipUpShotWeaponStatus = 277",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_take_cover(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"takeCover"',
            'getBackgroundGeneratedCombatWeaponStatus("takeCover")',
            "BIC_queueTakeCover",
            "BIC_takeCoverWeaponStatus",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueTakeCover"',
            '"TakeCoverWeaponStatus"',
            "QueueTakeCover = 278",
            "TakeCoverWeaponStatus = 279",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_full_auto_single_one(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"fullAutoSingle1"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"fullAutoSingle1")',
            "BIC_queueFullAutoSingle1",
            "BIC_fullAutoSingle1WeaponStatus",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.client_main)
        for token in [
            '"QueueFullAutoSingle1"',
            '"FullAutoSingle1WeaponStatus"',
            "QueueFullAutoSingle1 = 280",
            "FullAutoSingle1WeaponStatus = 281",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.helper)

    def test_bridge_exposes_core3_scatter_shots(self):
        for command in ("scatterShot1", "scatterShot2"):
            for token in [
                f'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"{command}"',
                f'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"{command}")',
            ]:
                with self.subTest(command=command, token=token):
                    self.assertIn(token, self.client_main)
        for token in [
            "BIC_queueScatterShot1",
            "BIC_scatterShot1WeaponStatus",
            "BIC_queueScatterShot2",
            "BIC_scatterShot2WeaponStatus",
            '"QueueScatterShot1"',
            '"ScatterShot1WeaponStatus"',
            '"QueueScatterShot2"',
            '"ScatterShot2WeaponStatus"',
            "QueueScatterShot1 = 282",
            "ScatterShot1WeaponStatus = 283",
            "QueueScatterShot2 = 284",
            "ScatterShot2WeaponStatus = 285",
        ]:
            with self.subTest(token=token):
                self.assertIn(
                    token,
                    self.client_main if token.startswith("BIC_") else self.helper,
                )

    def test_bridge_exposes_core3_leg_shot_continuation(self):
        for command in ("legShot2", "legShot3"):
            for token in [
                f'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"{command}"',
                f'getBackgroundGeneratedCombatWeaponStatus("{command}")',
            ]:
                with self.subTest(command=command, token=token):
                    self.assertIn(token, self.client_main)
        for token in [
            "BIC_queueLegShot2",
            "BIC_legShot2WeaponStatus",
            "BIC_queueLegShot3",
            "BIC_legShot3WeaponStatus",
            '"QueueLegShot2"',
            '"LegShot2WeaponStatus"',
            '"QueueLegShot3"',
            '"LegShot3WeaponStatus"',
            "QueueLegShot2 = 286",
            "LegShot2WeaponStatus = 287",
            "QueueLegShot3 = 288",
            "LegShot3WeaponStatus = 289",
        ]:
            with self.subTest(token=token):
                self.assertIn(
                    token,
                    self.client_main if token.startswith("BIC_") else self.helper,
                )

    def test_bridge_queues_internal_input_events(self):
        for token in [
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"fullAutoSingle2"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"fullAutoSingle2")',
            "BIC_queueFullAutoSingle2",
            "BIC_fullAutoSingle2WeaponStatus",
            '"QueueFullAutoSingle2"',
            '"FullAutoSingle2WeaponStatus"',
            "QueueFullAutoSingle2 = 290",
            "FullAutoSingle2WeaponStatus = 291",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"suppressionFire2"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"suppressionFire2")',
            "BIC_queueSuppressionFire2",
            "BIC_suppressionFire2WeaponStatus",
            '"QueueSuppressionFire2"',
            '"SuppressionFire2WeaponStatus"',
            "QueueSuppressionFire2 = 292",
            "SuppressionFire2WeaponStatus = 293",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"wildShot1"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"wildShot1")',
            "BIC_queueWildShot1",
            "BIC_wildShot1WeaponStatus",
            '"QueueWildShot1"',
            '"WildShot1WeaponStatus"',
            "QueueWildShot1 = 294",
            "WildShot1WeaponStatus = 295",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"wildShot2"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"wildShot2")',
            "BIC_queueWildShot2",
            "BIC_wildShot2WeaponStatus",
            '"QueueWildShot2"',
            '"WildShot2WeaponStatus"',
            "QueueWildShot2 = 296",
            "WildShot2WeaponStatus = 297",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"fullAutoArea1"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"fullAutoArea1")',
            "BIC_queueFullAutoArea1",
            "BIC_fullAutoArea1WeaponStatus",
            '"QueueFullAutoArea1"',
            '"FullAutoArea1WeaponStatus"',
            "QueueFullAutoArea1 = 298",
            "FullAutoArea1WeaponStatus = 299",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"chargeShot1"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"chargeShot1")',
            "BIC_queueChargeShot1",
            "BIC_chargeShot1WeaponStatus",
            '"QueueChargeShot1"',
            '"ChargeShot1WeaponStatus"',
            "QueueChargeShot1 = 300",
            "ChargeShot1WeaponStatus = 301",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"fullAutoArea2"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"fullAutoArea2")',
            "BIC_queueFullAutoArea2",
            "BIC_fullAutoArea2WeaponStatus",
            '"QueueFullAutoArea2"',
            '"FullAutoArea2WeaponStatus"',
            "QueueFullAutoArea2 = 302",
            "FullAutoArea2WeaponStatus = 303",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"chargeShot2"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"chargeShot2")',
            "BIC_queueChargeShot2",
            "BIC_chargeShot2WeaponStatus",
            '"QueueChargeShot2"',
            '"ChargeShot2WeaponStatus"',
            "QueueChargeShot2 = 304",
            "ChargeShot2WeaponStatus = 305",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"strafeShot1"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"strafeShot1")',
            "BIC_queueStrafeShot1",
            "BIC_strafeShot1WeaponStatus",
            '"QueueStrafeShot1"',
            '"StrafeShot1WeaponStatus"',
            "QueueStrafeShot1 = 306",
            "StrafeShot1WeaponStatus = 307",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"mindShot2"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"mindShot2")',
            "BIC_queueMindShot2",
            "BIC_mindShot2WeaponStatus",
            '"QueueMindShot2"',
            '"MindShot2WeaponStatus"',
            "QueueMindShot2 = 308",
            "MindShot2WeaponStatus = 309",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"surpriseShot"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"surpriseShot")',
            "BIC_queueSurpriseShot",
            "BIC_surpriseShotWeaponStatus",
            '"QueueSurpriseShot"',
            '"SurpriseShotWeaponStatus"',
            "QueueSurpriseShot = 310",
            "SurpriseShotWeaponStatus = 311",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"sniperShot"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"sniperShot")',
            "BIC_queueSniperShot",
            "BIC_sniperShotWeaponStatus",
            '"QueueSniperShot"',
            '"SniperShotWeaponStatus"',
            "QueueSniperShot = 312",
            "SniperShotWeaponStatus = 313",
            'performBackgroundQueueConcealShot(lParam)',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"concealShot")',
            "BIC_queueConcealShot",
            "BIC_concealShotWeaponStatus",
            '"QueueConcealShot"',
            '"ConcealShotWeaponStatus"',
            "QueueConcealShot = 314",
            "ConcealShotWeaponStatus = 315",
            'QueueConcealShot requires -TargetOid.',
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"flurryShot1"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"flurryShot1")',
            "BIC_queueFlurryShot1",
            "BIC_flurryShot1WeaponStatus",
            '"QueueFlurryShot1"',
            '"FlurryShot1WeaponStatus"',
            "QueueFlurryShot1 = 316",
            "FlurryShot1WeaponStatus = 317",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"flurryShot2"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"flurryShot2")',
            "BIC_queueFlurryShot2",
            "BIC_flurryShot2WeaponStatus",
            '"QueueFlurryShot2"',
            '"FlurryShot2WeaponStatus"',
            "QueueFlurryShot2 = 318",
            "FlurryShot2WeaponStatus = 319",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"strafeShot2"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"strafeShot2")',
            "BIC_queueStrafeShot2",
            "BIC_strafeShot2WeaponStatus",
            '"QueueStrafeShot2"',
            '"StrafeShot2WeaponStatus"',
            "QueueStrafeShot2 = 320",
            "StrafeShot2WeaponStatus = 321",
            'performBackgroundQueueMarksmanTier1(\n\t\t\t\t\t\t"startleShot1"',
            'getBackgroundGeneratedCombatWeaponStatus(\n\t\t\t\t\t"startleShot1")',
            "BIC_queueStartleShot1",
            "BIC_startleShot1WeaponStatus",
            '"QueueStartleShot1"',
            '"StartleShot1WeaponStatus"',
            "QueueStartleShot1 = 322",
            "StartleShot1WeaponStatus = 323",
        ]:
            with self.subTest(full_auto_single_two_token=token):
                self.assertIn(
                    token,
                    self.client_main if token.startswith("BIC_") or
                    token.startswith("perform") or token.startswith("getBackground")
                    else self.helper,
                )

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
            "ShowMyProfessions": 226,
            "SelectMyProfession": 227,
            "QueueSampleDNA": 228,
            "QueueTame": 229,
            "QueueEmboldenPets": 230,
            "QueueHealMind": 231,
            "QueueBerserk1": 232,
            "QueueBerserk2": 233,
            "TargetSquadCounterpart": 234,
            "QueueFormup": 235,
            "QueueRetreat": 236,
            "QueueBoostMorale": 237,
            "QueueSteadyAim": 238,
            "QueueApplyPoison": 239,
            "QueueApplyDisease": 240,
            "QueueAreaTrack": 241,
            "SelectAreaTrackType": 242,
            "EquipFixtureFlame": 243,
            "QueueFlameSingle1": 244,
            "FlameSingle1WeaponStatus": 245,
            "QueueFlameSingle2": 246,
            "FlameSingle2WeaponStatus": 247,
            "QueueFlameCone1": 248,
            "FlameCone1WeaponStatus": 249,
            "QueueFlameCone2": 250,
            "FlameCone2WeaponStatus": 251,
            "QueueHealthShot1": 252,
            "HealthShot1WeaponStatus": 253,
            "QueueMindShot1": 254,
            "MindShot1WeaponStatus": 255,
            "QueueActionShot1": 256,
            "ActionShot1WeaponStatus": 257,
            "QueueActionShot2": 258,
            "ActionShot2WeaponStatus": 259,
            "QueueOverChargeShot1": 260,
            "OverChargeShot1WeaponStatus": 261,
            "QueuePointBlankSingle1": 262,
            "PointBlankSingle1WeaponStatus": 263,
            "QueueThreatenShot": 264,
            "ThreatenShotWeaponStatus": 265,
            "QueueWarningShot": 266,
            "WarningShotWeaponStatus": 267,
            "QueueAim": 268,
            "AimWeaponStatus": 269,
            "QueueSuppressionFire1": 270,
            "SuppressionFire1WeaponStatus": 271,
            "QueueRollShot": 272,
            "RollShotWeaponStatus": 273,
            "QueueDiveShot": 274,
            "DiveShotWeaponStatus": 275,
            "QueueKipUpShot": 276,
            "KipUpShotWeaponStatus": 277,
            "QueueTakeCover": 278,
            "TakeCoverWeaponStatus": 279,
            "QueueFullAutoSingle1": 280,
            "FullAutoSingle1WeaponStatus": 281,
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
            "TargetSquadCounterpart",
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

        squad_counterpart = function_body(
            self.client_main, "bool performBackgroundTargetSquadCounterpart()"
        )
        for object_id in ("44003778", "207005062"):
            with self.subTest(squad_object_id=object_id):
                self.assertIn(f'"{object_id}"', squad_counterpart)
        self.assertNotIn("39008597", squad_counterpart)
        self.assertNotIn("lParam", squad_counterpart)

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
        sample_dna = function_body(
            self.client_main,
            "bool performBackgroundQueueSampleDna(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("sampleDNA", targetValue)',
            sample_dna,
        )
        tame = function_body(
            self.client_main,
            "bool performBackgroundQueueTame(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("tame", targetValue)',
            tame,
        )
        embolden_pets = function_body(
            self.client_main,
            "bool performBackgroundQueueEmboldenPets()",
        )
        self.assertIn('"emboldenpets"', embolden_pets)
        self.assertIn("NetworkId::cms_invalid", embolden_pets)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            embolden_pets,
        )
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            embolden_pets,
        )
        heal_mind = function_body(
            self.client_main,
            "bool performBackgroundQueueHealMind(LPARAM const targetValue)",
        )
        self.assertIn(
            'performBackgroundQueueTending("healMind", targetValue)',
            heal_mind,
        )
        berserk_one = function_body(
            self.client_main,
            "bool performBackgroundQueueBerserk1()",
        )
        self.assertIn('"berserk1"', berserk_one)
        self.assertIn("NetworkId::cms_invalid", berserk_one)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            berserk_one,
        )
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            berserk_one,
        )
        berserk_two = function_body(
            self.client_main,
            "bool performBackgroundQueueBerserk2()",
        )
        self.assertIn('"berserk2"', berserk_two)
        self.assertIn("NetworkId::cms_invalid", berserk_two)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            berserk_two,
        )
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            berserk_two,
        )
        formup = function_body(
            self.client_main,
            "bool performBackgroundQueueFormup()",
        )
        self.assertIn('"formup"', formup)
        self.assertIn("NetworkId::cms_invalid", formup)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            formup,
        )
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            formup,
        )
        retreat = function_body(
            self.client_main,
            "bool performBackgroundQueueRetreat()",
        )
        self.assertIn('"retreat"', retreat)
        self.assertIn("NetworkId::cms_invalid", retreat)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            retreat,
        )
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            retreat,
        )
        boost_morale = function_body(
            self.client_main,
            "bool performBackgroundQueueBoostMorale()",
        )
        self.assertIn('"boostmorale"', boost_morale)
        self.assertIn("NetworkId::cms_invalid", boost_morale)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            boost_morale,
        )
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            boost_morale,
        )
        steady_aim = function_body(
            self.client_main,
            "bool performBackgroundQueueSteadyAim()",
        )
        self.assertIn('"steadyaim"', steady_aim)
        self.assertIn("NetworkId::cms_invalid", steady_aim)
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(true)",
            steady_aim,
        )
        self.assertIn(
            "ClientCommandQueue::commandsAreNowFromToolbar(false)",
            steady_aim,
        )
        for function_name, command_name in [
            ("performBackgroundQueueApplyPoison", "applyPoison"),
            ("performBackgroundQueueApplyDisease", "applyDisease"),
        ]:
            body = function_body(
                self.client_main,
                f"bool {function_name}(LPARAM const targetValue)",
            )
            self.assertIn(
                f'performBackgroundQueueTending("{command_name}", targetValue)',
                body,
            )
        area_track = function_body(
            self.client_main,
            "bool performBackgroundQueueAreaTrack()",
        )
        self.assertIn('"44003778"', area_track)
        self.assertIn('enqueueCommand(\n\t\t\t"areatrack"', area_track)
        area_track_selection = function_body(
            self.client_main,
            "LRESULT performBackgroundSelectAreaTrackType(LPARAM const selectionIndex)",
        )
        self.assertIn('"44003778"', area_track_selection)
        self.assertIn(
            "CuiDataDrivenPageManager::selectAndConfirmSingleAreaTrackRow",
            area_track_selection,
        )
        self.assertIn("selectionIndex))", area_track_selection)
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
            "ShowMyProfessions",
            "SelectMyProfession",
            "QueueSampleDNA",
            "QueueTame",
            "QueueEmboldenPets",
            "QueueHealMind",
            "QueueBerserk1",
            "QueueBerserk2",
            "TargetSquadCounterpart",
            "QueueFormup",
            "QueueRetreat",
            "QueueBoostMorale",
            "QueueSteadyAim",
            "QueueApplyPoison",
            "QueueApplyDisease",
            "QueueAreaTrack",
            "SelectAreaTrackType",
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
