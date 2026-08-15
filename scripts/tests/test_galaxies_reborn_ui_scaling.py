from __future__ import annotations

import unittest
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = ROOT.parent / "pre-cu-reborn-assets"
PREFERENCES_H = ROOT / "src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.h"
PREFERENCES_CPP = ROOT / "src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp"
OPTIONS_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOptGalaxiesReborn.cpp"
OPTIONS_UI = ASSET_ROOT / "ui/ui_options.inc"


SCALE_CONTROLS = {
    "SkillBar": "Skill Bar Scale",
    "Ham": "HAM Scale",
    "SelectedTargetHam": "Selected Target HAM Scale",
    "Party": "Party Scale",
    "Chat": "Chat Scale",
    "Menu": "Menu Scale",
}


class GalaxiesRebornUiScalingTests(unittest.TestCase):
    def test_preferences_are_per_user_persistent_and_clamped(self) -> None:
        header = PREFERENCES_H.read_text(encoding="utf-8")
        source = PREFERENCES_CPP.read_text(encoding="utf-8")

        self.assertIn("cms_individualUiScaleMinimum        = 0.75f", source)
        self.assertIn("cms_individualUiScaleMaximum        = 1.50f", source)
        for category in SCALE_CONTROLS:
            lower = category[0].lower() + category[1:]
            self.assertIn(f"set{category}UiScale(float scale)", header)
            self.assertIn(f"get{category}UiScale()", header)
            self.assertRegex(source, rf"ms_{lower}UiScale\s*= 1\.0f")
            self.assertIn(f"REGISTER_OPTION_USER({lower}UiScale);", source)
            self.assertIn(
                f"ms_{lower}UiScale = clampIndividualUiScale(scale);",
                source,
            )
            self.assertIn(
                f"return clampIndividualUiScale(ms_{lower}UiScale);",
                source,
            )
            self.assertIn(f"set{category}UiScale(ms_{lower}UiScale);", source)

        self.assertIn("if (scale != scale)", source)
        self.assertIn("return 1.0f;", source)

    def test_options_mediator_guards_and_registers_every_slider(self) -> None:
        source = OPTIONS_CPP.read_text(encoding="utf-8")

        self.assertIn('#include "UISliderbar.h"', source)
        for category in SCALE_CONTROLS:
            code_name = f"slider{category}UiScale"
            self.assertIn(
                f'getCodeDataObject(TUISliderbar, slider, "{code_name}", true);',
                source,
            )
            self.assertIn(f"CuiPreferences::set{category}UiScale", source)
            self.assertIn(f"CuiPreferences::get{category}UiScale", source)
        self.assertEqual(source.count("if (slider)"), len(SCALE_CONTROLS))
        self.assertEqual(source.count("SwgCuiOptBase::getOne"), len(SCALE_CONTROLS))

    def test_options_asset_has_named_slider_rows_and_valid_code_paths(self) -> None:
        source = OPTIONS_UI.read_text(encoding="utf-8")
        galaxies_reborn = source[source.index("Name='galaxiesReborn'") :]

        for category, label in SCALE_CONTROLS.items():
            row_name = f"slider{category}UiScale"
            self.assertIn(f"{row_name}='main.comp.{row_name}.slider'", galaxies_reborn)
            row_match = re.search(
                rf"<Page\s+.*?Name='{row_name}'.*?</Page>", galaxies_reborn, re.DOTALL
            )
            self.assertIsNotNone(row_match)
            row = row_match.group(0)
            self.assertIn(f"LocalText='{label}'", row)
            self.assertIn(f">{label}</Text>", row)
            self.assertRegex(row, r"LocalTooltip='[^']+'")
            self.assertIn("Style='/Styles.New.Slider.default.horizontal.style_test'", row)


if __name__ == "__main__":
    unittest.main()
