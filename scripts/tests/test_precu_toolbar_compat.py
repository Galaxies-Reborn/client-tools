import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TOOLBAR_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiToolbar.cpp"
)


class Publish14ToolbarCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = TOOLBAR_SOURCE.read_text(encoding="utf-8")

    def test_later_current_action_overlay_requires_its_optional_assets(self):
        self.assertEqual(
            1,
            self.source.count(
                "if(showNewCurrentActionPages && m_currentActionPage && "
                "m_effectorCurrent && (compareCrc == m_commandExecutingCrc))"
            ),
        )

    def test_publish14_missing_pet_highlight_page_is_guarded(self):
        method_match = re.search(
            r"void SwgCuiToolbar::setPetBarVisible\(const bool visible\)\s*"
            r"\{.*?\n\}",
            self.source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(method_match)
        method = method_match.group(0)
        self.assertIn("if (m_petVolumeHighlightsPage)", method)
        self.assertIn(
            "m_petVolumeHighlightsPage->GetChildrenRef()",
            method,
        )

    def test_pet_slot_refresh_is_preserved_when_highlights_are_absent(self):
        method_start = self.source.index("void SwgCuiToolbar::setPetBarVisible")
        method_end = self.source.index(
            "void SwgCuiToolbar::updateCommandRange", method_start
        )
        method = self.source[method_start:method_end]
        highlight_guard = method.index("if (m_petVolumeHighlightsPage)")
        refresh = method.index("repopulateSlots(true);")
        self.assertLess(highlight_guard, refresh)
        self.assertEqual(1, method.count("repopulateSlots(true);"))


if __name__ == "__main__":
    unittest.main()
