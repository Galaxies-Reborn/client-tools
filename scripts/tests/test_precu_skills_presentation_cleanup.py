from __future__ import annotations

import os
import unittest
import xml.etree.ElementTree as ElementTree
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SKILLS_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiSkills.cpp"
)
SKILLS_HEADER = SKILLS_CPP.with_suffix(".h")

ASSET_OVERRIDE = os.environ.get("PRECU_SKILLS_ASSET")
SKILLS_ASSET = (
    Path(ASSET_OVERRIDE).expanduser()
    if ASSET_OVERRIDE
    else ROOT.parents[1] / "Staging/m2-authentic-ui/ui/ui_skill.inc"
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


def named_child(
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


def resolve_path(
    parent: ElementTree.Element, path: str
) -> ElementTree.Element:
    current = parent
    for component in path.split("."):
        current = named_child(current, component)
    return current


class PrecuSkillsPresentationCleanupSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = SKILLS_CPP.read_text(encoding="utf-8")
        cls.header = SKILLS_HEADER.read_text(encoding="utf-8")

    def test_all_publish14_graph_placeholder_slots_are_cleared(self) -> None:
        graph = function_body(
            self.source, "bool SwgCuiSkills::tryPopulateGraph4x4("
        )
        self.assertIn("for (int slot = 0; slot < 6; ++slot)", graph)
        self.assertIn('"graph.disciplineNext.%d.%d"', graph)
        self.assertIn("SetLocalText(Unicode::emptyString)", graph)
        self.assertGreaterEqual(graph.count("linkText->SetVisible(false)"), 2)
        self.assertIn("for (int slot = 0; slot < 4; ++slot)", graph)
        self.assertIn('"graph.next.%d"', graph)
        self.assertGreaterEqual(graph.count("linkText->SetVisible(true)"), 2)
        self.assertIn('"graph.prev.%d"', graph)
        self.assertIn("text->SetVisible(false)", graph)
        self.assertIn("t->SetVisible(true)", graph)

        clear_branch = graph.index("slot < 6")
        clear_master = graph.index("slot < 4")
        populate_branch = graph.index("def->branchLinks[col][slot]")
        self.assertLess(clear_branch, populate_branch)
        self.assertLess(clear_master, populate_branch)
        self.assertLess(
            graph.index("linkText->SetVisible(false)"),
            graph.index("linkText->SetVisible(true)"),
        )
        prev_reset = graph.index('"graph.prev.%d"')
        self.assertLess(
            graph.index("text->SetVisible(false)", prev_reset),
            graph.index("t->SetVisible(true)", prev_reset),
        )

    def test_master_links_derive_from_runtime_novice_prerequisites(self) -> None:
        graph = function_body(
            self.source, "bool SwgCuiSkills::tryPopulateGraph4x4("
        )
        for contract in (
            "candidateNovice->getPrerequisiteSkills()",
            "(*prerequisite)->getSkillName() == masterName",
            'snprintf(path, sizeof(path), "graph.next.%d", nextSlot)',
            'Unicode::String label = Unicode::narrowToWide("To: ")',
            "m_linkSkills[linkText] = candidate.noviceSkill",
            "registerMediatorObject(*linkText, true)",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, graph)
        self.assertIn("nextSlot < 4", graph)
        self.assertLess(
            graph.index("slot < 4"),
            graph.index("candidateNovice->getPrerequisiteSkills()"),
        )

    def test_certification_table_is_rebuilt_from_localized_player_commands(self) -> None:
        constructor = function_body(
            self.source, "SwgCuiSkills::SwgCuiSkills(UIPage & page)"
        )
        self.assertIn(
            'GetObjectFromPath("comp.TableCerts.containerall.name",  '
            "TUIDataSource)",
            constructor,
        )
        self.assertIn("m_dsCertsName", self.header)

        populate = function_body(
            self.source, "void SwgCuiSkills::populateCertifications()"
        )
        for contract in (
            "m_dsCertsName->Clear()",
            "player->getCommands()",
            'commandName.compare(0, 5, "cert_")',
            "CuiSkillManager::localizeCmdName(Unicode::toLower(commandName)",
            "appendGrantedDetailRow(m_dsCertsName, 0, localizedName",
        ):
            with self.subTest(contract=contract):
                self.assertIn(contract, populate)
        self.assertLess(
            populate.index("m_dsCertsName->Clear()"),
            populate.index("Game::getPlayerCreature()"),
        )

    def test_command_delta_refresh_is_activation_scoped_and_player_filtered(self) -> None:
        activate = function_body(
            self.source, "void SwgCuiSkills::performActivate()"
        )
        deactivate = function_body(
            self.source, "void SwgCuiSkills::performDeactivate()"
        )
        self.assertIn("CreatureObject::Messages::CommandsChanged", activate)
        self.assertIn("CreatureObject::Messages::CommandsChanged", deactivate)
        self.assertIn("populateCertifications()", activate)

        handler = function_body(
            self.source, "void SwgCuiSkills::onCommandsChanged("
        )
        self.assertIn("&creature == Game::getPlayerCreature()", handler)
        self.assertIn("populateCertifications()", handler)


class PrecuSkillsPresentationCleanupAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not SKILLS_ASSET.is_file():
            raise unittest.SkipTest(
                f"Publish 14 skills asset not found at {SKILLS_ASSET}"
            )
        cls.root = ElementTree.fromstring(SKILLS_ASSET.read_bytes())

    def test_asset_exposes_the_complete_graph_text_contract(self) -> None:
        graph = next(
            element
            for element in self.root.iter()
            if element.attrib.get("Name", "").casefold() == "graph4x4"
        )
        code_data = next(
            element
            for element in graph
            if element.tag == "Data"
            and element.attrib.get("Name", "").casefold() == "codedata"
        )

        for column in range(4):
            for slot in range(6):
                property_name = f"textDisciplineNext_{column}_{slot}"
                with self.subTest(property_name=property_name):
                    target = resolve_path(graph, code_data.attrib[property_name])
                    self.assertEqual("Text", target.tag)
                    self.assertIn("specialist", target.attrib["LocalText"])

        for slot in range(4):
            property_name = f"textNext_{slot}"
            with self.subTest(property_name=property_name):
                target = resolve_path(graph, code_data.attrib[property_name])
                self.assertEqual("Text", target.tag)
                self.assertIn("specialist", target.attrib["LocalText"])

        for slot in range(4):
            property_name = f"textPrev_{slot}"
            with self.subTest(property_name=property_name):
                target = resolve_path(graph, code_data.attrib[property_name])
                self.assertEqual("Text", target.tag)
                self.assertIn("specialist", target.attrib["LocalText"])

    def test_asset_certification_rows_are_editor_samples_not_localization(self) -> None:
        table_certs = next(
            element
            for element in self.root.iter()
            if element.attrib.get("Name", "").casefold() == "tablecerts"
        )
        names = resolve_path(table_certs, "containerall.name")
        self.assertEqual(
            ["1 one", "1 one", "2 two", "3 three"],
            [child.attrib.get("Value") for child in names],
        )


if __name__ == "__main__":
    unittest.main()
