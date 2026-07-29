from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ATTRIBUTES_DEF = ROOT / (
    "src/game/shared/library/swgSharedUtility/src/shared/Attributes.def"
)
PLAYER_CREATION_CPP = ROOT / (
    "src/engine/shared/library/sharedGame/src/shared/core/PlayerCreationManager.cpp"
)
STAT_MESSAGE_CPP = ROOT / (
    "src/engine/shared/library/sharedNetworkMessages/src/shared/clientGameServer/"
    "StatMigrationTargetsMessage.cpp"
)
STAT_MESSAGE_HEADER = STAT_MESSAGE_CPP.with_suffix(".h")
CHARACTER_SHEET_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiCharacterSheet.cpp"
)

ATTRIBUTE_LAYOUT = (
    ("Health", 0),
    ("Strength", 1),
    ("Constitution", 2),
    ("Action", 3),
    ("Quickness", 4),
    ("Stamina", 5),
    ("Mind", 6),
    ("Focus", 7),
    ("Willpower", 8),
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


class PrecuNineAttributeRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.attributes = ATTRIBUTES_DEF.read_text(encoding="utf-8")
        cls.player_creation = PLAYER_CREATION_CPP.read_text(encoding="utf-8")
        cls.stat_message = STAT_MESSAGE_CPP.read_text(encoding="utf-8")
        cls.stat_message_header = STAT_MESSAGE_HEADER.read_text(encoding="utf-8")
        cls.character_sheet = CHARACTER_SHEET_CPP.read_text(encoding="utf-8")

    def test_shared_enum_uses_the_publish14_wire_order(self) -> None:
        for name, index in ATTRIBUTE_LAYOUT:
            self.assertRegex(
                self.attributes,
                rf"const\s+int\s+{name}\s*=\s*{index}\s*;",
            )
        self.assertRegex(
            self.attributes, r"const\s+int\s+NumberOfAttributes\s*=\s*9\s*;"
        )
        self.assertRegex(
            self.attributes,
            r"const\s+int\s+POOLS\[\]\s*=\s*\{\s*"
            r"Attributes::Health\s*,\s*Attributes::Action\s*,\s*"
            r"Attributes::Mind\s*\}",
        )

    def test_creation_tables_are_read_and_emitted_in_wire_order(self) -> None:
        racial = function_body(
            self.player_creation, "void PlayerCreationManager::loadRacialModifiers()"
        )
        profession = function_body(
            self.player_creation,
            "void PlayerCreationManager::loadProfessionModifiers()",
        )
        expected_names = tuple(name.casefold() for name, _ in ATTRIBUTE_LAYOUT)
        for body in (racial, profession):
            table_columns = tuple(
                re.findall(r'get(?:IntValue|IntValue\s*)\s*\(\s*"([^"]+)"', body)
            )
            for name in expected_names:
                self.assertIn(name, table_columns)
            pushes = tuple(re.findall(r"modifiers\.push_back\((\w+)\)", body))
            self.assertEqual(pushes[-9:], expected_names)

    def test_stat_migration_message_serializes_all_nine_values(self) -> None:
        member_order = tuple(
            re.findall(r"Archive::AutoVariable<int>\s+m_(\w+)\s*;", self.stat_message_header)
        )
        self.assertEqual(member_order[:-1], tuple(name.casefold() for name, _ in ATTRIBUTE_LAYOUT))
        self.assertEqual(member_order[-1], "pointsLeft")

        write_constructor = function_body(
            self.stat_message,
            "StatMigrationTargetsMessage::StatMigrationTargetsMessage(const std::vector<int>& currentTargets, int pointsLeft)",
        )
        wire_order = tuple(
            re.findall(r"AutoByteStream::addVariable\(m_(\w+)\)", write_constructor)
        )
        self.assertEqual(
            wire_order,
            tuple(name.casefold() for name, _ in ATTRIBUTE_LAYOUT) + ("pointsLeft",),
        )

    def test_publish14_character_sheet_reads_every_attribute(self) -> None:
        for name, _ in ATTRIBUTE_LAYOUT:
            self.assertIn(
                f"Attributes::{name}",
                self.character_sheet,
            )
        self.assertIn("int const value = creature->getAttribute(enumerator);", self.character_sheet)
        self.assertIn("int const maximum = creature->getMaxAttribute(enumerator);", self.character_sheet)
        self.assertNotRegex(
            self.character_sheet,
            r'addAttributeRow\(\s*"(?:strength|quickness|focus)"\s*,\s*-1',
        )


if __name__ == "__main__":
    unittest.main()
