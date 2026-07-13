import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
LOADING_SPACE_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiLoadingSpace.cpp"
)


class Publish14LoadingScreenCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = LOADING_SPACE_SOURCE.read_text(encoding="utf-8")

    def test_publish14_missing_widgets_use_optional_code_data_bindings(self):
        optional_bindings = (
            'getCodeDataObject (TUIButton,   m_escButton,          "buttonEsc", true);',
            'getCodeDataObject (TUIPage,     m_defaultBgPage,      "default", true);',
        )
        for binding in optional_bindings:
            with self.subTest(binding=binding):
                self.assertEqual(1, self.source.count(binding))

    def test_authentic_publish14_widgets_remain_required(self):
        required_bindings = (
            'getCodeDataObject (TUIText,     m_textScreenshotName, "screenshotname");',
            'getCodeDataObject (TUIText,     m_text,               "text");',
            'getCodeDataObject (TUIPie,      m_pie,                "pie");',
            'getCodeDataObject (TUIButton,   m_backButton,         "backbutton");',
            'getCodeDataObject (TUIImage,    m_image,              "screenshot");',
            'getCodeDataObject (TUIText,     m_textLoad,           "textprogress");',
        )
        for binding in required_bindings:
            with self.subTest(binding=binding):
                self.assertEqual(1, self.source.count(binding))

    def test_button_registration_is_null_safe(self):
        self.assertIn(
            "if (m_backButton) registerMediatorObject (*m_backButton, true);",
            self.source,
        )
        self.assertIn(
            "if (m_escButton)  registerMediatorObject (*m_escButton, true);",
            self.source,
        )
        self.assertEqual(
            1, self.source.count("registerMediatorObject (*m_escButton, true);")
        )

    def test_optional_default_background_is_guarded(self):
        self.assertIn(
            "if (m_defaultBgPage)\n\t\tm_defaultBgPage->SetVisible(true);",
            self.source,
        )
        self.assertEqual(1, self.source.count("m_defaultBgPage->SetVisible(true);"))


if __name__ == "__main__":
    unittest.main()
