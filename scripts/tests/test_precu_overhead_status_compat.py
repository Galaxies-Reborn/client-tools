import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
STATUS_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiStatusGround.cpp"
)


class Publish14OverheadStatusCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = STATUS_SOURCE.read_text(encoding="utf-8")
        match = re.search(
            r"SwgCuiStatusGround::SwgCuiStatusGround\b.*?\n\}",
            cls.source,
            flags=re.DOTALL,
        )
        if match is None:
            raise AssertionError("SwgCuiStatusGround constructor was not found")
        cls.constructor = match.group(0)

    def test_publish14_accuracy_preview_is_located_without_code_data(self):
        self.assertIn(
            'page.GetObjectFromPath("Accuracy", TUIPage)',
            self.constructor,
        )
        self.assertIn(
            'legacyAccuracyPage->GetObjectFromPath("text", TUIText)',
            self.constructor,
        )

    def test_publish14_accuracy_preview_text_is_cleared_and_hidden(self):
        compatibility = re.search(
            r"UIPage \* const legacyAccuracyPage\b.*?"
            r"legacyAccuracyPage->SetVisible\(false\);",
            self.constructor,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(compatibility)
        self.assertIn(
            "legacyAccuracyText->SetLocalText(Unicode::emptyString);",
            compatibility.group(0),
        )

    def test_retired_preview_is_not_repurposed_as_distance(self):
        self.assertNotIn(
            "m_textDistance = legacyAccuracyText",
            self.constructor,
        )


if __name__ == "__main__":
    unittest.main()
