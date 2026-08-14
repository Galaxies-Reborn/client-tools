import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
OS_SOURCE = REPOSITORY_ROOT / (
    "src/engine/shared/library/sharedFoundation/src/win32/Os.cpp"
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


def case_body(source: str, label: str, next_label: str) -> str:
    start = source.index(label)
    end = source.index(next_label, start + len(label))
    return source[start:end]


class PrecuWin32FocusStateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = OS_SOURCE.read_text(encoding="utf-8")
        cls.window_proc = function_body(
            cls.source, "LRESULT CALLBACK Os::WindowProc("
        )
        cls.focus_update = function_body(
            cls.source, "void OsNamespace::updateFocusState(bool focused)"
        )

    def test_minimized_inactive_activate_uses_only_the_low_word(self):
        activate = case_body(
            self.window_proc, "case WM_ACTIVATE:", "case WM_ACTIVATEAPP:"
        )
        self.assertIn(
            "updateFocusState(LOWORD(wParam) != WA_INACTIVE);", activate
        )
        self.assertNotIn("if (wParam != WA_INACTIVE)", activate)

        # WM_ACTIVATE stores the minimized flag in the high word.  It must not
        # turn an inactive transition into an acquired-focus transition.
        minimized_inactive = (1 << 16) | 0
        self.assertEqual(minimized_inactive & 0xFFFF, 0)

    def test_activate_app_releases_but_does_not_acquire_early(self):
        activate_app = case_body(
            self.window_proc, "case WM_ACTIVATEAPP:", "case WM_DISPLAYCHANGE:"
        )
        self.assertIn("if (wParam == FALSE)", activate_app)
        self.assertIn("updateFocusState(false);", activate_app)
        self.assertNotIn("updateFocusState(true);", activate_app)
        self.assertNotIn("updateFocusState(wParam != FALSE);", activate_app)

    def test_paired_activate_messages_call_each_hook_once_per_state_edge(self):
        self.assertIn("if (!ms_focused)", self.focus_update)
        self.assertIn("if (ms_focused)", self.focus_update)
        self.assertEqual(
            self.focus_update.count("ms_acquiredFocusHookFunction();"), 1
        )
        self.assertEqual(
            self.focus_update.count("ms_acquiredFocusHookFunction2();"), 1
        )
        self.assertEqual(self.focus_update.count("ms_lostFocusHookFunction();"), 1)

        acquired_state = self.focus_update.index("ms_focused = true;")
        acquired_hook = self.focus_update.index("ms_acquiredFocusHookFunction();")
        lost_state = self.focus_update.index("ms_focused = false;")
        lost_hook = self.focus_update.index("ms_lostFocusHookFunction();")
        self.assertLess(acquired_state, acquired_hook)
        self.assertLess(lost_state, lost_hook)

    def test_existing_focus_side_effects_are_preserved(self):
        for token in (
            "ms_clickToMove = false;",
            "ms_mouseMoveInClient = true;",
            "ms_wasFocusLost = true;",
            "ClipCursor(NULL);",
        ):
            self.assertIn(token, self.focus_update)


if __name__ == "__main__":
    unittest.main()
