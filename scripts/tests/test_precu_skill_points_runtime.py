from __future__ import annotations

import csv
import os
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
SKILL_OBJECT_HEADER = ROOT / (
    "src/engine/shared/library/sharedSkillSystem/src/shared/SkillObject.h"
)
SKILL_OBJECT_CPP = SKILL_OBJECT_HEADER.with_suffix(".cpp")
SKILLS_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiSkills.cpp"
)
SKILLS_DATA_HEADER = SKILLS_CPP.with_name("SwgCuiSkillsData.h")


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


def load_points_required(path: Path) -> dict[str, int]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        if not reader.fieldnames or "POINTS_REQUIRED" not in reader.fieldnames:
            raise AssertionError(f"{path} has no POINTS_REQUIRED column")
        return {
            row["NAME"].strip(): int(row["POINTS_REQUIRED"])
            for row in reader
            if row.get("NAME")
            and row["NAME"].strip() not in {"s", "string"}
            and (row.get("POINTS_REQUIRED") or "").strip()
        }


class PrecuSkillPointsRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.skill_object_header = SKILL_OBJECT_HEADER.read_text(encoding="utf-8")
        cls.skill_object_cpp = SKILL_OBJECT_CPP.read_text(encoding="utf-8")
        cls.skills_cpp = SKILLS_CPP.read_text(encoding="utf-8")
        cls.skills_data_header = SKILLS_DATA_HEADER.read_text(encoding="utf-8")

    def test_skill_object_loads_points_required_from_runtime_datatable(self) -> None:
        self.assertIn("getSkillPointsRequired", self.skill_object_header)
        self.assertIn("int                                         skillPointsRequired;", self.skill_object_header)
        self.assertIn(
            'SkillObject::ms_skillPointsRequiredLabel          = "POINTS_REQUIRED"',
            self.skill_object_cpp,
        )

        load = function_body(self.skill_object_cpp, "bool SkillObject::load(")
        self.assertIn(
            "skillData.skillPointsRequired = "
            "dataTable.getIntValue(SkillObject::ms_skillPointsRequiredLabel, skillRow)",
            load,
        )
        getter = function_body(
            self.skill_object_cpp, "const int SkillObject::getSkillPointsRequired() const"
        )
        self.assertIn("return skillData.skillPointsRequired;", getter)

        for lifecycle_copy in (
            r"skillPointsRequired\s+\(source\.skillPointsRequired\)",
            r"skillPointsRequired\s*=\s*rhs\.skillPointsRequired",
        ):
            self.assertRegex(self.skill_object_cpp, lifecycle_copy)

    def test_available_points_come_from_held_runtime_skill_costs(self) -> None:
        update = function_body(
            self.skills_cpp, "void SwgCuiSkills::updateSkillPointsDisplay()"
        )
        self.assertIn("(*it)->getSkillPointsRequired()", update)
        self.assertIn(
            "calculateAvailableSkillPoints(usedSkillPoints)", update
        )
        self.assertIn(
            '"%d / %d", availableSkillPoints, k_skillPointCap', update
        )

        calculate = function_body(
            self.skills_cpp, "int calculateAvailableSkillPoints(int usedSkillPoints)"
        )
        self.assertIn(
            "std::max(0, std::min(k_skillPointCap, "
            "k_skillPointCap - usedSkillPoints))",
            calculate,
        )
        self.assertIn("static int const k_skillPointCap = 250;", self.skills_data_header)
        self.assertNotIn("k_skillCosts", self.skills_cpp)
        self.assertNotIn("SkillCostEntry", self.skills_data_header)

        populate = function_body(
            self.skills_cpp, "void SwgCuiSkills::populateSelectedSkill()"
        )
        self.assertIn(
            "availableSkillPoints = calculateAvailableSkillPoints(usedSkillPoints)",
            populate,
        )
        self.assertIn(
            "learningBarWidth, availableSkillPoints, k_skillPointCap", populate
        )
        self.assertNotIn(
            "learningBarWidth, usedSkillPoints, k_skillPointCap", populate
        )

    def test_p14_representative_costs_produce_250_235_233(self) -> None:
        source_value = os.environ.get("PRECU_AUTHENTIC_SKILLS_TAB")
        if not source_value:
            self.skipTest("set PRECU_AUTHENTIC_SKILLS_TAB to audit the P14 source table")

        points = load_points_required(Path(source_value))
        expected = {
            "crafting_artisan_novice": 15,
            "crafting_artisan_master": 6,
            "crafting_artisan_engineering_01": 2,
            "combat_marksman_novice": 15,
            "combat_marksman_rifle_01": 2,
        }
        self.assertEqual(expected, {name: points[name] for name in expected})

        def available(*skills: str) -> int:
            used = sum(max(0, points[name]) for name in skills)
            return max(0, min(250, 250 - used))

        self.assertEqual(250, available())
        self.assertEqual(235, available("crafting_artisan_novice"))
        self.assertEqual(
            233,
            available(
                "crafting_artisan_novice", "crafting_artisan_engineering_01"
            ),
        )


if __name__ == "__main__":
    unittest.main()
