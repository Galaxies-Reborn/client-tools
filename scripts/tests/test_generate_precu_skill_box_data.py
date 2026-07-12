from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "generate_precu_skill_box_data.py"
SPEC = importlib.util.spec_from_file_location("generate_precu_skill_box_data", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class GeneratePrecuSkillBoxDataTests(unittest.TestCase):
    def test_loads_commands_schematics_and_public_modifiers(self) -> None:
        content = (
            "NAME\tCOMMANDS\tSKILL_MODS\tSCHEMATICS_GRANTED\n"
            "s\ts\ts\ts\n"
            "combat_marksman_novice\t"
            '"private_marksman_novice,headShot1"\t'
            '"rifle_accuracy=10,private_rifle_difficulty=100"\t'
            '"schematic_one"\n'
        )
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "skills.tab"
            source.write_text(content, encoding="utf-8")
            data = MODULE.load_skill_box_data(source)

        self.assertEqual(1, data.skill_count)
        self.assertEqual(
            (
                MODULE.CommandGrant("combat_marksman_novice", "headShot1"),
                MODULE.CommandGrant("combat_marksman_novice", "schematic_one"),
            ),
            data.commands,
        )
        self.assertEqual(
            (
                MODULE.SkillModGrant(
                    "combat_marksman_novice", "rifle_accuracy", 10
                ),
            ),
            data.modifiers,
        )

    def test_rendered_header_round_trips_through_audit_parser(self) -> None:
        data = MODULE.SkillBoxData(
            skill_count=1,
            commands=(MODULE.CommandGrant("skill_one", "command_one"),),
            modifiers=(MODULE.SkillModGrant("skill_one", "accuracy", -5),),
        )
        with tempfile.TemporaryDirectory() as directory:
            header = Path(directory) / "SwgCuiSkillBoxData.h"
            header.write_text(
                MODULE.render_header(data, "0" * 64), encoding="utf-8"
            )
            parsed = MODULE.parse_existing_header(header)

        self.assertEqual(data.commands, parsed.commands)
        self.assertEqual(data.modifiers, parsed.modifiers)


if __name__ == "__main__":
    unittest.main()
