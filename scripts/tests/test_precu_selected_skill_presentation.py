from __future__ import annotations

import hashlib
import os
import unittest
import xml.etree.ElementTree as ElementTree
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SKILLS_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiSkills.cpp"
)
SKILLS_HEADER = SKILLS_CPP.with_suffix(".h")

ASSET_ENVIRONMENT_VARIABLE = "PRECU_SKILLS_ASSET"
ASSET_OVERRIDE = os.environ.get(ASSET_ENVIRONMENT_VARIABLE)
DEFAULT_SKILLS_ASSET = ROOT.parents[1] / (
    "Staging/m2-authentic-ui/ui/ui_skill.inc"
)
SKILLS_ASSET = (
    Path(ASSET_OVERRIDE).expanduser()
    if ASSET_OVERRIDE
    else DEFAULT_SKILLS_ASSET
)

EXPECTED_ASSET_SIZE = 326_431
EXPECTED_ASSET_SHA256 = (
    "10004451a46a1ebe5cdf314adda1c0f73b62edcbe4f261ae1c9880aefcdb225a"
)


@dataclass(frozen=True)
class Binding:
    ui_type: str
    asset_path: str


SELECTED_SKILL_BINDINGS = {
    "textAcquire": Binding("Text", "all.skillPoints.textAcquire"),
    "textSurrender": Binding("Text", "all.skillPoints.textSurrender"),
    "pageLearningCurrent": Binding(
        "Page", "all.skillPoints.bar.all.full.current"
    ),
    "pageLearningCost": Binding("Page", "all.skillPoints.bar.all.full.cost"),
    "pageLearningRecover": Binding(
        "Page", "all.skillPoints.bar.all.full.recover"
    ),
    "textExpRequired": Binding("Text", "all.exp.textRequired"),
    "barExp": Binding("Page", "all.exp.barParent.pageBar"),
}


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


def immediate_named_child(
    parent: ElementTree.Element, name: str
) -> ElementTree.Element:
    result = next(
        (
            child
            for child in parent
            if child.attrib.get("Name", "").casefold() == name.casefold()
        ),
        None,
    )
    if result is None:
        raise AssertionError(
            f"unable to resolve child {name!r} beneath "
            f"{parent.attrib.get('Name', parent.tag)!r}"
        )
    return result


def resolve_asset_path(
    root: ElementTree.Element, path: str
) -> ElementTree.Element:
    current = root
    for component in path.split("."):
        current = immediate_named_child(current, component)
    return current


def code_data_properties(
    page: ElementTree.Element,
) -> dict[str, tuple[str, str]]:
    code_data = next(
        (
            child
            for child in page
            if child.tag == "Data"
            and child.attrib.get("Name", "").casefold() == "codedata"
        ),
        None,
    )
    if code_data is None:
        raise AssertionError(
            f"{page.attrib.get('Name', page.tag)!r} has no immediate CodeData"
        )
    return {
        name.casefold(): (name, value)
        for name, value in code_data.attrib.items()
        if name.casefold() != "name"
    }


class PrecuSelectedSkillSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = SKILLS_CPP.read_text(encoding="utf-8")
        cls.header = SKILLS_HEADER.read_text(encoding="utf-8")

    def test_source_binds_the_nested_publish14_selected_skill_contract(self) -> None:
        constructor = function_body(
            self.source, "SwgCuiSkills::SwgCuiSkills(UIPage & page)"
        )
        self.assertIn('GetObjectFromPath("CodeData", TUIData)', constructor)
        for name, binding in SELECTED_SKILL_BINDINGS.items():
            with self.subTest(binding=name):
                source_type = "TUI" + binding.ui_type
                self.assertIn(
                    f'{source_type}, m_{name[0].lower() + name[1:]}, "{name}"',
                    constructor,
                )

    def test_selected_skill_drives_points_recovery_and_experience(self) -> None:
        populate = function_body(
            self.source, "void SwgCuiSkills::populateSelectedSkill()"
        )
        for contract in (
            "selectedSkill->getSkillPointsRequired()",
            "selectedSkill->getPrerequisiteExperience()",
            "m_pageLearningCurrent",
            "m_pageLearningCost",
            "m_pageLearningRecover",
            "m_textAcquire",
            "m_textSurrender",
            "m_textExpRequired",
            "m_barExp",
            "CuiStringIdsSkill::acquire_skill_points_prose",
            "CuiStringIdsSkill::surrender_prose",
            "CuiStringIdsSkill::acquire_exp_prose",
            "CuiStringIdsSkill::exp_prose",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, populate)

        self.assertIn("showAcquisition && selectedSkillCost > 0", populate)
        self.assertIn("ownsSelectedSkill && selectedSkillCost > 0", populate)
        self.assertIn("hasAllPrerequisiteSkills(*player, *selectedSkill)", populate)
        self.assertIn("showAcquisition && hasPrerequisites", populate)

        prerequisite_gate = function_body(
            self.source, "bool hasAllPrerequisiteSkills("
        )
        self.assertIn("skill.getPrerequisiteSkills()", prerequisite_gate)
        self.assertIn("findOwnedSkill(player, prerequisite->getSkillName())", prerequisite_gate)
        self.assertIn("return false", prerequisite_gate)

    def test_all_selected_skill_bar_geometry_is_bounded(self) -> None:
        proportional = function_body(
            self.source, "UIScalar calculateProportionalWidth("
        )
        self.assertGreaterEqual(proportional.count("std::max"), 2)
        self.assertGreaterEqual(proportional.count("std::min"), 2)
        self.assertIn("maximum <= 0", proportional)

        bar_range = function_body(self.source, "void setHorizontalBarRange(")
        for bounded_value in (
            "boundedFullWidth",
            "boundedStart",
            "requestedEnd",
            "boundedEnd",
            "boundedWidth",
        ):
            self.assertIn(bounded_value, bar_range)
        self.assertIn("page->SetWidth(boundedWidth)", bar_range)
        self.assertIn("location.x = boundedStart", bar_range)
        self.assertIn("boundedEnd - boundedStart", bar_range)

        def bounded_range(start: int, width: int, full_width: int) -> tuple[int, int]:
            bounded_start = max(0, min(full_width, start))
            bounded_end = max(0, min(full_width, start + max(0, width)))
            return bounded_start, max(0, bounded_end - bounded_start)

        self.assertEqual((0, 5), bounded_range(-5, 10, 100))
        self.assertEqual((95, 5), bounded_range(95, 10, 100))

        populate = function_body(
            self.source, "void SwgCuiSkills::populateSelectedSkill()"
        )
        self.assertEqual(4, populate.count("setHorizontalBarRange("))
        self.assertIn(
            "learningBarWidth, availableSkillPoints, k_skillPointCap", populate
        )
        self.assertIn(
            "m_pageLearningCost, currentWidth, selectedCostWidth", populate
        )
        self.assertIn(
            "m_pageLearningRecover, currentWidth - selectedCostWidth", populate
        )

    def test_schematic_groups_expand_to_localized_drafts_not_commands(self) -> None:
        populate = function_body(
            self.source, "void SwgCuiSkills::populateSelectedSkill()"
        )
        for contract in (
            "selectedSkill->getStatisticModifiers()",
            "selectedSkill->getCommandsProvided()",
            "selectedSkill->getSchematicsGranted()",
            "isPrivateName(modifierName)",
            "isPrivateName(cmdKey)",
            "commandDefinition.m_visibleToClients",
            "schematicGroupNames.find(cmdGrantKey)",
            "DraftSchematicGroupManager::getSchematicsForGroup",
            "DraftSchematicManager::cacheDraftSchematic",
            "info->getLocalizedName()",
            '"/styles.icon.misc.granted"',
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, populate)
        self.assertLess(
            populate.index("schematicGroupNames.find(cmdGrantKey)"),
            populate.index("CuiSkillManager::localizeCmdName(cmdGrantKey"),
        )

    def test_parameterized_commands_use_base_policy_and_icon_keys(self) -> None:
        populate = function_body(
            self.source, "void SwgCuiSkills::populateSelectedSkill()"
        )
        for contract in (
            "cmdGrantKey.find('+')",
            "cmdGrantKey.substr(0, argumentSeparator)",
            "cmdGrantKey.substr(argumentSeparator + 1)",
            "isPrivateName(cmdKey)",
            "Crc::normalizeAndCalculate(cmdKey.c_str())",
            "cmdKey.compare(0, 5, \"cert_\")",
            'std::string("/styles.icon.command.") + cmdKey',
            "CuiSkillManager::localizeCmdName(cmdGrantKey, localizedName)",
            "CuiSkillManager::localizeCmdName(cmdKey, localizedName)",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, populate)

        def split_grant(grant: str) -> tuple[str, str]:
            base, separator, argument = grant.casefold().partition("+")
            return base, argument if separator else ""

        self.assertEqual(("flourish", "1"), split_grant("flourish+1"))
        self.assertEqual(
            ("startdance", "basic"), split_grant("startDance+basic")
        )

        self.assertNotIn("k_skillBoxMods", populate)
        self.assertNotIn("k_skillBoxCommands", populate)
        self.assertNotIn("SwgCuiSkillBoxData.h", self.source)
        self.assertNotIn("MarcJoyce", populate)
        self.assertNotIn("stock TRE skills.iff", populate)

    def test_source_has_no_custom_per_box_xpbar_or_respec_dependency(self) -> None:
        self.assertNotIn("applySkillBoxXp", self.source)
        self.assertNotIn("applySkillBoxXp", self.header)
        self.assertNotIn('".xpbar"', self.source.casefold())
        self.assertNotIn("skillsrespec", self.source.casefold())
        self.assertNotIn("skillsrespec", self.header.casefold())


class PrecuSelectedSkillAssetContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not SKILLS_ASSET.is_file():
            message = (
                f"Publish 14 skills asset not found at {SKILLS_ASSET}. Set "
                f"{ASSET_ENVIRONMENT_VARIABLE} to its extracted path."
            )
            if ASSET_OVERRIDE:
                raise AssertionError(message)
            raise unittest.SkipTest(message)

        cls.asset_bytes = SKILLS_ASSET.read_bytes()
        cls.root = ElementTree.fromstring(cls.asset_bytes)
        cls.skills_page = immediate_named_child(cls.root, "skills")
        cls.skills_properties = code_data_properties(cls.skills_page)
        cls.right_page = resolve_asset_path(
            cls.skills_page, cls.skills_properties["pageprofession"][1]
        )
        cls.right_properties = code_data_properties(cls.right_page)

    def test_asset_is_the_locked_patch13_reference(self) -> None:
        self.assertEqual(EXPECTED_ASSET_SIZE, len(self.asset_bytes))
        self.assertEqual(
            EXPECTED_ASSET_SHA256,
            hashlib.sha256(self.asset_bytes).hexdigest(),
        )
        self.assertEqual("Page", self.root.tag)
        self.assertEqual("Skill", self.root.attrib.get("Name"))
        self.assertEqual("skills", self.skills_page.attrib.get("Name"))

    def test_selected_skill_codedata_paths_resolve_to_expected_types(self) -> None:
        self.assertEqual("both.right", self.skills_properties["pageprofession"][1])
        for name, binding in SELECTED_SKILL_BINDINGS.items():
            with self.subTest(binding=name):
                property_name, asset_path = self.right_properties[name.casefold()]
                self.assertEqual(name.casefold(), property_name.casefold())
                self.assertEqual(binding.asset_path.casefold(), asset_path.casefold())
                target = resolve_asset_path(self.right_page, asset_path)
                self.assertEqual(binding.ui_type, target.tag)

    def test_asset_needs_no_custom_per_box_xpbar_or_skills_respec_page(self) -> None:
        element_names = {
            element.attrib.get("Name", "").casefold()
            for element in self.root.iter()
        }
        self.assertNotIn("xpbar", element_names)
        self.assertNotIn("skillsrespec", element_names)
        self.assertNotIn(b"skillsrespec", self.asset_bytes.lower())


if __name__ == "__main__":
    unittest.main()
