from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PAGE = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page"
CORE = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/core"


class PrecuCharacterAppearanceSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.character_sheet = (PAGE / "SwgCuiCharacterSheet.cpp").read_text(
            encoding="utf-8"
        )
        cls.appearance = (PAGE / "SwgCuiAppearanceTab.cpp").read_text(
            encoding="utf-8"
        )
        cls.hud_action = (PAGE / "SwgCuiHudAction.cpp").read_text(encoding="utf-8")
        cls.window_manager = (PAGE / "SwgCuiHudWindowManager.cpp").read_text(
            encoding="utf-8"
        )
        cls.factory = (CORE / "SwgCuiMediatorFactorySetup.cpp").read_text(
            encoding="utf-8"
        )

    def test_character_sheet_owns_embedded_authentic_mediator(self) -> None:
        self.assertIn(
            'UIPage::DuplicateInto(*m_appearanceHost, "/PDA.AppearanceTab")',
            self.character_sheet,
        )
        self.assertIn("new SwgCuiAppearanceTab(*appearancePage, true)", self.character_sheet)
        self.assertIn("m_appearanceTab->fetch()", self.character_sheet)
        self.assertIn("m_appearanceTab->release()", self.character_sheet)

    def test_tab_is_self_only_and_tracks_parent_lifecycle(self) -> None:
        self.assertIn("appearanceTab->SetVisible(examiningSelf)", self.character_sheet)
        self.assertIn(
            "isActive() && isExaminingSelf() && m_tabbedPane->GetActiveTab() == TAB_appearance",
            self.character_sheet,
        )
        self.assertIn("m_appearanceTab->deactivate()", self.character_sheet)

    def test_embedded_page_uses_sheet_bounds_without_own_pointer_or_close(self) -> None:
        for contract in (
            "layoutForEmbeddedParent()",
            "getPage().SetSize(UISize(width, height))",
            "m_viewerPage->SetSize(UISize(viewerWidth, height))",
            "loadingRunner->SetVisible(false)",
            "originalRunner->SetVisible(false)",
            "hostSize.x < 280",
            "hostSize.y < 252",
            "long const lineHeight = slotHeight - 33",
            "text->SetSize(UISize(slotWidth - 2, 31))",
            "line->SetLocation(UIPoint(2, 31))",
            "if (!m_embeddedInCharacterSheet)",
            "context == m_closeButton && !m_embeddedInCharacterSheet",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, self.appearance)

    def test_appearance_action_opens_in_sheet_not_standalone(self) -> None:
        self.assertIn("spawnCharacterAppearanceSheet()", self.hud_action)
        self.assertIn("showCharacterAppearance()", self.window_manager)
        self.assertNotIn(
            "toggleInWorkspace(CuiMediatorTypes::WS_AppearanceTab)", self.hud_action
        )
        self.assertNotIn("MAKE_SWG_CTOR_WS (AppearanceTab", self.factory)

    def test_constructor_defers_preview_duplication_until_a_player_exists(self) -> None:
        constructor = self.appearance.split("SwgCuiAppearanceTab::~", 1)[0]
        perform_activate = self.appearance.split(
            "void SwgCuiAppearanceTab::performActivate()", 1
        )[1]
        self.assertNotIn("duplicateCreatureWithClothesAndCustomization", constructor)
        self.assertIn("if (!playerCreature)", perform_activate)
        self.assertIn(
            "duplicateCreatureWithClothesAndCustomization(*playerCreature, false)",
            perform_activate,
        )

    def test_teardown_removes_duplicate_page_before_mediator_release(self) -> None:
        remove_position = self.character_sheet.index(
            "m_appearanceHost->RemoveChild(appearancePage)"
        )
        release_position = self.character_sheet.index("m_appearanceTab->release()")
        self.assertLess(remove_position, release_position)

    def test_each_slot_registers_its_own_viewer(self) -> None:
        self.assertIn("registerMediatorObject(*objectListViewer, true)", self.appearance)
        self.assertNotIn("registerMediatorObject(*viewer, true);\n\t\t}", self.appearance)


if __name__ == "__main__":
    unittest.main()
