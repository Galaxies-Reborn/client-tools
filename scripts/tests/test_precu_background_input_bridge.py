import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CLIENT_MAIN_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/application/SwgClient/src/win32/ClientMain.cpp"
)
HELPER_SOURCE = REPOSITORY_ROOT / "scripts/Invoke-PrecuBackgroundInput.ps1"


class PrecuBackgroundInputBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.client_main = CLIENT_MAIN_SOURCE.read_text(encoding="utf-8")
        cls.helper = HELPER_SOURCE.read_text(encoding="utf-8")

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
        ]
        positions = [self.client_main.index(command) for command in expected_commands]
        self.assertEqual(positions, sorted(positions))
        self.assertIn("cms_backgroundInputProtocolVersion = 1", self.client_main)

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
        }
        for name, value in expected_helper_commands.items():
            with self.subTest(command=name):
                self.assertRegex(
                    self.helper,
                    rf"(?m)^\s*{re.escape(name)}\s*=\s*{value}\s*$",
                )


if __name__ == "__main__":
    unittest.main()
