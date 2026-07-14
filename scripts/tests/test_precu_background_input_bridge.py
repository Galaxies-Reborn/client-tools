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
HELPER_SOURCE = REPOSITORY_ROOT / "scripts/Invoke-PrecuBackgroundInput.ps1"


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

    def test_helper_exposes_atomic_chords_not_raw_key_state(self):
        parameter_block = self.helper[: self.helper.index("Set-StrictMode")]
        self.assertIn('"Key", "Chord", "Text"', parameter_block)
        self.assertNotIn('"KeyDown"', parameter_block)
        self.assertNotIn('"KeyUp"', parameter_block)
        self.assertIn("[int[]]$ModifierDikCode", parameter_block)

        sequence = function_body(self.helper, "function Send-BridgeKeySequence")
        self.assertIn("try {", sequence)
        self.assertIn("finally {", sequence)
        self.assertIn("$pressedModifiers.Count - 1", sequence)
        self.assertIn("$command.InputReset", sequence)
        self.assertIn("$actionError", sequence)

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
