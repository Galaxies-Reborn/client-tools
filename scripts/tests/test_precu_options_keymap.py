from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OPTIONS_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOpt.cpp"
)
KEYMAP_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiOptKeymap.cpp"
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


class Publish14OptionsKeymapContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = OPTIONS_CPP.read_text(encoding="utf-8")
        cls.keymap_source = KEYMAP_CPP.read_text(encoding="utf-8")
        cls.constructor = function_body(cls.source, "SwgCuiOpt::SwgCuiOpt")
        cls.tab_changed = function_body(
            cls.source, "void SwgCuiOpt::OnTabbedPaneChanged"
        )

    def test_missing_embedded_keymap_target_routes_to_publish14_dialog(self) -> None:
        self.assertIn(
            'UIPage::DuplicateInto(*dupParent, "/PDA.keymap")',
            self.constructor,
        )
        self.assertIn(
            'routesToStandaloneKeymap = narrowPath == "target.keymap"',
            self.constructor,
        )
        self.assertIn(
            "if (!target && !routesToStandaloneKeymap)",
            self.constructor,
        )

    def test_keymap_tab_detection_uses_its_stable_target_not_localized_text(self) -> None:
        self.assertIn(
            "UITabbedPane::DataProperties::DATA_TARGET",
            self.tab_changed,
        )
        self.assertIn(
            'Unicode::wideToNarrow(activeTarget) == "target.keymap"',
            self.tab_changed,
        )
        self.assertNotIn('activeName.find("keymap")', self.tab_changed)
        self.assertIn("m_standaloneKeymap->activate()", self.tab_changed)

    def test_keymap_mediator_has_its_own_settings_and_debug_identity(self) -> None:
        self.assertIn(
            'SwgCuiOptBase ("SwgCuiOptKeymap", page)',
            self.keymap_source,
        )
        self.assertNotIn(
            'SwgCuiOptBase ("SwgCuiOptChat", page)',
            self.keymap_source,
        )


if __name__ == "__main__":
    unittest.main()
