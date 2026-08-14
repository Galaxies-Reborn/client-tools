from __future__ import annotations

import unittest
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PAGE_ROOT = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page"
CORE_ROOT = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/core"
ASSETS = ROOT.parent / "pre-cu-reborn-assets"


class GalaxiesRebornUiScaleRoutingTests(unittest.TestCase):
    def test_each_slider_drives_the_intended_live_mediator_root(self) -> None:
        expected = {
            "SwgCuiToolbar.cpp": "CuiPreferences::getSkillBarUiScale()",
            "SwgCuiStatusGround.cpp": "CuiPreferences::getHamUiScale()",
            "SwgCuiGroup.cpp": "CuiPreferences::getPartyUiScale()",
            "SwgCuiChatWindow.cpp": "CuiPreferences::getChatUiScale()",
            "SwgCuiButtonBar.cpp": "CuiPreferences::getMenuUiScale()",
        }
        for filename, getter in expected.items():
            with self.subTest(filename=filename):
                source = (PAGE_ROOT / filename).read_text(encoding="utf-8")
                self.assertIn(getter, source)
                self.assertIn("SetScale(uiScale);", source)

    def test_ham_scale_marker_excludes_party_and_overhead_clones(self) -> None:
        ground = (ASSETS / "ui/ui_ground_hud.inc").read_text(encoding="ascii")
        group = (ASSETS / "ui/ui_ground_hud_group.inc").read_text(encoding="ascii")
        self.assertEqual(2, ground.count("IndividualUiScale='ham'"))
        self.assertRegex(
            ground,
            re.compile(r"Name='MFDStatus'.*?IndividualUiScale='ham'", re.DOTALL),
        )
        self.assertRegex(
            ground,
            re.compile(r"Name='Target'.*?IndividualUiScale='ham'", re.DOTALL),
        )
        sample = ground[ground.index("Name='sampleStatus'") :]
        self.assertNotIn("IndividualUiScale='ham'", sample[:1200])
        self.assertNotIn("IndividualUiScale='ham'", group)

    def test_scaled_popups_and_containment_use_world_transform_helpers(self) -> None:
        lockable = (CORE_ROOT / "SwgCuiLockableMediator.cpp").read_text(encoding="utf-8")
        status = (PAGE_ROOT / "SwgCuiStatusGround.cpp").read_text(encoding="utf-8")
        toolbar = (PAGE_ROOT / "SwgCuiToolbar.cpp").read_text(encoding="utf-8")
        chat = (PAGE_ROOT / "SwgCuiChatWindow.cpp").read_text(encoding="utf-8")
        group = (PAGE_ROOT / "SwgCuiGroup.cpp").read_text(encoding="utf-8")

        for source in (lockable, status, toolbar, chat, group):
            self.assertNotIn("GetWorldLocation() + msg.MouseCoords", source)
        self.assertGreaterEqual(status.count("GetWorldRect()"), 2)
        self.assertIn("context->GetWorldPointFromLocal(msg.MouseCoords)", status)
        self.assertIn("m_volumePage->GetWorldRect()", toolbar)
        self.assertIn("m_tabs->GetWorldRect()", chat)

        button_bar = (PAGE_ROOT / "SwgCuiButtonBar.cpp").read_text(encoding="utf-8")
        self.assertIn(
            "m_buttonsComposite->GetLocalPointFromWorld(mouseCoord)", button_bar
        )
        self.assertNotIn("worldRect.Height()", button_bar)

    def test_chat_slider_updates_inactive_clones_and_reset_defaults(self) -> None:
        chat = (PAGE_ROOT / "SwgCuiChatWindow.cpp").read_text(encoding="utf-8")
        options = (PAGE_ROOT / "SwgCuiOptGalaxiesReborn.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("applyUiScaleToAllWindows", chat)
        self.assertIn("ms_activeChatWindows[sceneType]", chat)
        self.assertIn("onChatUiScaleChanged", options)
        self.assertIn(
            "SwgCuiChatWindow::applyUiScaleToAllWindows(CuiPreferences::getChatUiScale())",
            options,
        )


if __name__ == "__main__":
    unittest.main()
