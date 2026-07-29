import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
RADAR_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiGroundRadar.cpp"
)
RADAR_HEADER = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiGroundRadar.h"
)


class Publish14GroundRadarCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = RADAR_SOURCE.read_text(encoding="utf-8")
        cls.header = RADAR_HEADER.read_text(encoding="utf-8")

    def test_compass_top_accepts_publish14_image_widget(self):
        self.assertIn("UIWidget *                   m_radarCompassTop;", self.header)
        self.assertEqual(
            1,
            self.source.count(
                'getCodeDataObject (TUIWidget,   m_radarCompassTop,    '
                '"RadarCompassTop");'
            ),
        )
        self.assertNotIn(
            'getCodeDataObject (TUIPage,     m_radarCompassTop,',
            self.source,
        )

    def test_later_resizable_radar_skins_are_optional(self):
        for name in (
            "RadarSkinSmall",
            "RadarSkinMedium",
            "RadarSkinLarge",
            "RadarCompassTopSmall",
            "RadarCompassTopMedium",
            "RadarCompassTopLarge",
        ):
            with self.subTest(name=name):
                matching_lines = [
                    line
                    for line in self.source.splitlines()
                    if f'"{name}"' in line and "getCodeDataObject" in line
                ]
                self.assertEqual(1, len(matching_lines))
                self.assertTrue(matching_lines[0].rstrip().endswith(", true);"))

    def test_missing_later_entrance_blip_uses_publish14_waypoint_blip(self):
        self.assertIn(
            "if (!blipEntrance)\n\t\t\tblipEntrance = blipWaypoint;",
            self.source,
        )
        self.assertNotIn("blipEntrance = blipStructure", self.source)
        self.assertNotIn("blipEntrance = blip;", self.source)
        fallback = self.source.index("if (!blipEntrance)")
        pane_guard = self.source.index(
            "if (blip && blipCorpse && blipWaypoint && blipStructure",
            fallback,
        )
        self.assertLess(fallback, pane_guard)

    def test_missing_later_region_label_is_optional_and_guarded(self):
        self.assertEqual(
            1,
            self.source.count(
                'getCodeDataObject (TUIText,     m_regionIndicatorText,'
                '"RegionIndicator", true);'
            ),
        )
        self.assertIn(
            "if (m_regionIndicatorText)\n"
            "\t\t\tm_regionIndicatorText->SetText(regionString);",
            self.source,
        )
        self.assertEqual(1, self.source.count("m_regionIndicatorText->SetText("))


if __name__ == "__main__":
    unittest.main()
