import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
HUD_MANAGER_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiHudWindowManager.cpp"
)


class Publish14HudWindowManagerCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = HUD_MANAGER_SOURCE.read_text(encoding="utf-8")
        constructor_match = re.search(
            r"SwgCuiHudWindowManager::SwgCuiHudWindowManager\b.*?\n\{",
            cls.source,
            flags=re.DOTALL,
        )
        if constructor_match is None:
            raise AssertionError("SwgCuiHudWindowManager constructor was not found")
        cls.initializer_list = constructor_match.group(0)

    def test_publish14_missing_mediators_start_null(self):
        for initializer in (
            "m_notificationsMediator     (0)",
            "m_highlightMediator         (0)",
            "m_questHelper               (0)",
        ):
            with self.subTest(initializer=initializer):
                self.assertEqual(1, self.initializer_list.count(initializer))

    def test_double_toolbar_is_an_optional_publish14_binding(self):
        self.assertEqual(
            1,
            self.source.count(
                'hud.getCodeDataObject (TUIPage,     m_doubleToolbarPage,    '
                '"DoubleToolbar", true);'
            ),
        )

    def test_later_hud_mediators_are_optional_publish14_bindings(self):
        for binding in (
            'hud.getCodeDataObject (TUIPage,     mediatorPage,           '
            '"Notifications", true);',
            'hud.getCodeDataObject (TUIPage,     mediatorPage,           '
            '"Highlight", true);',
            'hud.getCodeDataObject(TUIPage, mediatorPage, '
            '"questHelper", true);',
        ):
            with self.subTest(binding=binding):
                self.assertEqual(1, self.source.count(binding))

    def test_only_available_toolbar_layout_is_selected(self):
        self.assertIn(
            "if (m_singleToolbarPage && !m_doubleToolbarPage)\n"
            "\t\t\t\tCuiPreferences::setUseDoubleToolbar(false);",
            self.source,
        )
        self.assertIn(
            "else if (!m_singleToolbarPage && m_doubleToolbarPage)\n"
            "\t\t\t\tCuiPreferences::setUseDoubleToolbar(true);",
            self.source,
        )

        toolbar_page_match = re.search(
            r"UIPage \*SwgCuiHudWindowManager::getToolbarPage\(\)\s*"
            r"\{.*?\n\}",
            self.source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(toolbar_page_match)
        toolbar_page = toolbar_page_match.group(0)
        self.assertIn(
            "CuiPreferences::getUseDoubleToolbar() && m_doubleToolbarPage",
            toolbar_page,
        )
        self.assertIn("else if (m_singleToolbarPage)", toolbar_page)
        self.assertIn("return m_doubleToolbarPage;", toolbar_page)


if __name__ == "__main__":
    unittest.main()
