from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUTTON_BAR = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiButtonBar.cpp"
)
HUD_ACTION = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiHudAction.cpp"
)


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    open_brace = source.index("{", start + len(signature))
    depth = 0
    for position in range(open_brace, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[start : position + 1]
    raise ValueError(f"unterminated function body: {signature}")


class PrecuProgressionSurfaceRetirementTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.button_bar = BUTTON_BAR.read_text(encoding="utf-8")
        cls.hud_action = HUD_ACTION.read_text(encoding="utf-8")

    def test_legacy_progression_buttons_are_disabled_with_their_parents(self) -> None:
        helper = function_body(
            self.button_bar, "void hideNgeProgressionButton("
        )
        self.assertIn("button->SetEnabled(false)", helper)
        self.assertIn("button->SetVisible(false)", helper)
        self.assertIn("parent->SetEnabled(false)", helper)
        self.assertIn("parent->SetVisible(false)", helper)

        activate = function_body(
            self.button_bar, "void  SwgCuiButtonBar::performActivate()"
        )
        self.assertIn("hideNgeProgressionButton(m_roadmapButton)", activate)
        self.assertIn("hideNgeProgressionButton(m_expertiseButton)", activate)

    def test_flyout_never_reenables_roadmap_or_expertise(self) -> None:
        toggle = function_body(self.button_bar, "void SwgCuiButtonBar::toggleMenu()")
        self.assertIn("hideNgeProgressionButton(m_roadmapButton)", toggle)
        self.assertIn("hideNgeProgressionButton(m_expertiseButton)", toggle)
        self.assertNotIn("RoadmapManager::", toggle)
        self.assertNotIn("ClientExpertiseManager::", toggle)
        self.assertNotIn("m_roadmapButton->GetParentWidget()->SetVisible(true)", toggle)
        self.assertNotIn("m_expertiseButton->GetParentWidget()->SetVisible(true)", toggle)

    def test_expertise_effector_cannot_restore_the_button(self) -> None:
        update = function_body(
            self.button_bar, "void SwgCuiButtonBar::updateExpertiseEffector()"
        )
        self.assertIn("m_effectingExpertise = false", update)
        self.assertIn("hideNgeProgressionButton(m_expertiseButton)", update)
        self.assertNotIn("ExecuteEffector", update)
        self.assertNotIn("ClientExpertiseManager::", update)

    def test_inherited_actions_open_only_the_precu_skills_window(self) -> None:
        perform = function_body(
            self.hud_action, "bool  SwgCuiHudAction::performAction ("
        )
        roadmap = perform[
            perform.index("else if (id == CuiActions::roadmap)") :
            perform.index("else if (id == CuiActions::expertise)")
        ]
        expertise = perform[
            perform.index("else if (id == CuiActions::expertise)") :
            perform.index("else if (id == CuiActions::ticketPurchase)")
        ]
        for action in (roadmap, expertise):
            self.assertIn(
                "CuiMediatorFactory::toggleInWorkspace"
                "(CuiMediatorTypes::WS_Skills)",
                action,
            )
            self.assertNotIn("WS_Roadmap", action)
            self.assertNotIn("WS_Expertise", action)


if __name__ == "__main__":
    unittest.main()
