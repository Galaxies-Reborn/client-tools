from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OPT_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOpt.cpp"
OPT_PAGE_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOptGalaxiesReborn.cpp"
STATUS_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiMfdStatusBar.cpp"
PREFERENCES_CPP = ROOT / "src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp"
PROJECT = ROOT / "src/game/client/library/swgClientUserInterface/build/win32/swgClientUserInterface.vcxproj"


class GalaxiesRebornHamEnhanceTests(unittest.TestCase):
    def test_options_page_is_compiled_and_binds_the_saved_preference(self) -> None:
        project = PROJECT.read_text(encoding="utf-8")
        page = OPT_PAGE_CPP.read_text(encoding="utf-8")
        preferences = PREFERENCES_CPP.read_text(encoding="utf-8")

        self.assertIn("SwgCuiOptGalaxiesReborn.cpp", project)
        self.assertIn('"checkHamEnhance"', page)
        self.assertIn("if (checkbox)", page)
        self.assertIn("CuiPreferences::setHamEnhance", page)
        self.assertIn("CuiPreferences::getHamEnhance", page)
        self.assertIn("REGISTER_OPTION_USER(hamEnhance);", preferences)

    def test_keymap_remains_a_standalone_controls_route(self) -> None:
        options = OPT_CPP.read_text(encoding="utf-8")
        self.assertIn('narrowPath == "target.keymap"', options)
        self.assertIn("m_standaloneKeymap->activate();", options)
        self.assertIn("m_tabs->SetActiveTab(9);", options)

    def test_live_status_bars_apply_and_restore_the_reviewed_layout(self) -> None:
        status = STATUS_CPP.read_text(encoding="utf-8")
        self.assertIn('UILowerString("HamEnhanceEligible")', status)
        self.assertIn("CuiPreferences::getHamEnhance()", status)
        self.assertIn("collectHamEnhanceWidgets", status)
        self.assertIn("if (state.widget == valueText)", status)
        self.assertNotIn("state.visible", status)
        self.assertIn("compactOverheadBar", status)


if __name__ == "__main__":
    unittest.main()
