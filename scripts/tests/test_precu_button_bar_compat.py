import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
BUTTON_BAR_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiButtonBar.cpp"
)


class Publish14ButtonBarCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = BUTTON_BAR_SOURCE.read_text(encoding="utf-8")

    def test_root_message_path_accepts_missing_later_flyout(self):
        method_match = re.search(
            r"bool SwgCuiButtonBar::OnMessage\b.*?\n\}",
            self.source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(method_match)
        method = method_match.group(0)
        self.assertEqual(
            1,
            method.count(
                "if(!m_buttonsComposite || !m_buttonsComposite->IsVisible())"
            ),
        )
        self.assertNotIn(
            "if(!m_buttonsComposite->IsVisible())",
            method,
        )

    def test_publish14_direct_command_bar_is_not_replaced(self):
        self.assertNotIn('GetObjectFromPath("vs.', self.source)
        self.assertNotIn('GetChild("vs")', self.source)
        self.assertNotIn("FindObjectByName", self.source)

    def test_opacity_callback_accepts_missing_later_menu_page(self):
        method_match = re.search(
            r"void SwgCuiButtonBar::onOpacityCallback\(\)\s*\{.*?\n\}",
            self.source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(method_match)
        method = method_match.group(0)
        self.assertIn("if (m_menuButtonPage)", method)
        self.assertIn(
            "m_menuButtonPage->SetOpacity(CuiPreferences::getCommandButtonOpacity());",
            method,
        )

    def test_publish14_update_keeps_native_button_effects_without_flyout(self):
        method_match = re.search(
            r"void SwgCuiButtonBar::update \(float deltaTimeSecs\)\s*\{.*?\n\}",
            self.source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(method_match)
        method = method_match.group(0)
        self.assertNotIn(
            "if (!m_menuButtonPage || !m_buttonsComposite)\n\t\treturn;",
            method,
        )
        self.assertIn(
            "if (m_buttonsComposite && m_buttonsComposite->IsVisible())",
            method,
        )
        self.assertIn("updateJournalEffector();", method)
        self.assertIn("updateMenuEffector();", method)


if __name__ == "__main__":
    unittest.main()
