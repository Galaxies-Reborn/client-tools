import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
STATUS_BAR_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiMfdStatusBar.cpp"
)


class Publish14MfdStatusBarCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = STATUS_BAR_SOURCE.read_text(encoding="utf-8")
        match = re.search(
            r"SwgCuiMfdStatusBar::SwgCuiMfdStatusBar\b.*?\n\}",
            cls.source,
            flags=re.DOTALL,
        )
        if match is None:
            raise AssertionError("SwgCuiMfdStatusBar constructor was not found")
        cls.constructor = match.group(0)

    def test_status_pages_use_publish14_code_data_bindings(self):
        required_bindings = (
            'getCodeDataObject (TUIPage, m_currentPage,    "current");',
            'getCodeDataObject (TUIPage, m_rechargePage,   "recentCurrent");',
        )
        for binding in required_bindings:
            with self.subTest(binding=binding):
                self.assertEqual(1, self.constructor.count(binding))

        self.assertEqual(
            1,
            self.constructor.count(
                'getCodeDataObject (TUIPage, m_currentMaxPage, "currentMax", true);'
            ),
        )

    def test_optional_publish14_bindings_remain_optional(self):
        self.assertEqual(
            1,
            self.constructor.count(
                'getCodeDataObject (TUIText, m_valueText,      "ValueText", true);'
            ),
        )
        self.assertEqual(
            1,
            self.constructor.count(
                'getCodeDataObject (TUIPage, m_currentTickPage, "currentTick", true);'
            ),
        )

    def test_later_status_binding_names_are_not_requested(self):
        for later_name in ('"juice"', '"moves"', '"cap"'):
            with self.subTest(later_name=later_name):
                self.assertNotIn(later_name, self.constructor)

    def test_optional_current_max_page_remains_guarded_during_updates(self):
        self.assertIn("if (m_currentMaxPage)", self.source)


if __name__ == "__main__":
    unittest.main()
