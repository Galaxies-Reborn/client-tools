from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
COMBAT_MANAGER = ROOT / (
    "src/engine/client/library/clientUserInterface/src/shared/core/"
    "CuiCombatManager.cpp"
)
COMMAND_HEADER = ROOT / (
    "src/engine/shared/library/sharedGame/src/shared/command/Command.h"
)
COMMAND_SOURCE = ROOT / (
    "src/engine/shared/library/sharedGame/src/shared/command/Command.cpp"
)
COMMAND_TABLE = ROOT / (
    "src/engine/shared/library/sharedGame/src/shared/command/CommandTable.cpp"
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

    def test_runtime_command_category_is_retained(self) -> None:
        header = COMMAND_HEADER.read_text(encoding="utf-8")
        command = COMMAND_SOURCE.read_text(encoding="utf-8")
        table = COMMAND_TABLE.read_text(encoding="utf-8")

        self.assertIn("uint32                                         m_commandCategory;", header)
        self.assertIn("m_commandCategory(0)", command)
        self.assertIn("m_commandCategory(rhs.m_commandCategory)", command)
        self.assertIn("m_commandCategory = rhs.m_commandCategory", command)
        self.assertIn('t.findColumnNumber("commandCategory")', table)
        self.assertIn("cmd.m_commandCategory =", table)

    def test_only_explicit_and_restored_combat_groups_are_combat_commands(self) -> None:
        body = function_body(
            self.source,
            "bool CuiCombatManager::isCombatCommand (const Command & command)",
        )
        self.assertIn('Crc::normalizeAndCalculate ("391413347")', body)
        self.assertIn('Crc::normalizeAndCalculate ("combat")', body)
        self.assertIn("command.m_commandCategory == combatCategoryCrc", body)
        self.assertIn(
            "command.m_commandGroup == precuRehashedCombatGroupCrc", body
        )
        self.assertIn('Crc::normalizeAndCalculate ("combat_ranged")', body)
        self.assertNotIn("command.m_addToCombatQueue", body)

    def test_named_attacks_render_the_localized_command_name(self) -> None:
        formatter = function_body(
            self.source,
            "Unicode::String buildPrecuNamedCombatMessage",
        )
        self.assertIn("spamMsg.m_attackName.localize()", formatter)
        self.assertIn('Unicode::narrowToWide("You use ")', formatter)
        self.assertIn("spamMsg.m_finalDamage + spamMsg.m_elementalDamage", formatter)

        processor = function_body(
            self.source,
            "void CuiCombatManager::processCombatSpam",
        )
        self.assertIn("buildPrecuNamedCombatMessage(", processor)

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
