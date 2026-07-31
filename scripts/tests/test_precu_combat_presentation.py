from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
COMBAT_MANAGER = ROOT / (
    "src/engine/client/library/clientUserInterface/src/shared/core/"
    "CuiCombatManager.cpp"
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


class PrecuCombatPresentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = COMBAT_MANAGER.read_text(encoding="utf-8")

    def test_precu_queue_commands_are_combat_commands(self) -> None:
        body = function_body(
            self.source,
            "bool CuiCombatManager::isCombatCommand (const Command & command)",
        )
        self.assertIn("command.m_addToCombatQueue ||", body)

    def test_legacy_combat_spam_selects_one_audience_line(self) -> None:
        selector = function_body(
            self.source,
            "Unicode::String selectPrecuCombatSpamLine",
        )
        self.assertIn('Unicode::narrowToWide("~")', selector)
        self.assertIn("if (playerIsDefender)", selector)
        self.assertIn("return messageLines.back()", selector)
        self.assertIn("messageLines.size() >= 3", selector)

        processor = function_body(
            self.source,
            "void CuiCombatManager::processCombatSpam",
        )
        self.assertIn("selectPrecuCombatSpamLine(", processor)
        self.assertIn("attacker == Game::getClientPlayer()", processor)
        self.assertIn("defender == Game::getClientPlayer()", processor)


if __name__ == "__main__":
    unittest.main()
