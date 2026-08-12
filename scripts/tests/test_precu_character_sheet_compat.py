from __future__ import annotations

import hashlib
import os
import re
import unittest
import xml.etree.ElementTree as ElementTree
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CHARACTER_SHEET_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiCharacterSheet.cpp"
)

ASSET_ENVIRONMENT_VARIABLE = "PRECU_CHARACTER_SHEET_ASSET"
ASSET_OVERRIDE = os.environ.get(ASSET_ENVIRONMENT_VARIABLE)
DEFAULT_CHARACTER_SHEET_ASSET = ROOT.parents[1] / (
    "MCP/SWGEmu/extracted/precu-patch-12-00/ui/ui_pda_char_sheet.inc"
)
CHARACTER_SHEET_ASSET = (
    Path(ASSET_OVERRIDE).expanduser()
    if ASSET_OVERRIDE
    else DEFAULT_CHARACTER_SHEET_ASSET
)

EXPECTED_ASSET_SIZE = 82_811
EXPECTED_ASSET_SHA256 = (
    "1d6eae700cb6c57fe684daafa054963d2bc8b26df5ab9bd6c0c298f0eb5aea05"
)


@dataclass(frozen=True)
class Binding:
    ui_type: str
    asset_path: str


REQUIRED_BINDINGS = {
    "tabs": Binding("TUITabbedPane", "tabs"),
    "textCharacterName": Binding("TUIText", "bg.capt.charactername"),
    "rank": Binding("TUIText", "bg.capt.rank"),
    "factionPvPStatusText": Binding(
        "TUIText", "target.status.comp.top.top.pageFactions.pvpStatus"
    ),
    "factionRebelText": Binding(
        "TUIText", "target.status.comp.top.top.pageFactions.rebelPercentage"
    ),
    "factionImperialText": Binding(
        "TUIText", "target.status.comp.top.top.pageFactions.imperialPercentage"
    ),
    "tableFactions": Binding("TUITable", "target.factions.tablefactions.table"),
    "pageAttributes": Binding("TUIPage", "target.status.comp.middle"),
    "textShockWounds": Binding(
        "TUIText", "target.status.comp.top.top.shockwounds.wounds"
    ),
    "borndate": Binding("TUIText", "target.personal.comp.top.born"),
    "species": Binding("TUIText", "target.personal.comp.top.species"),
    "playedTime": Binding("TUIText", "target.personal.comp.top.played"),
    "home": Binding("TUIText", "target.personal.comp.top.home"),
    "married": Binding("TUIText", "target.personal.comp.top.married"),
    "bindLocation": Binding("TUIText", "target.personal.comp.top.bind"),
    "bankLocation": Binding("TUIText", "target.personal.comp.top.bank"),
    "lotsAvailable": Binding("TUIText", "target.personal.comp.top.lots"),
    "guild": Binding("TUIText", "target.personal.comp.top.guild.guild"),
    "guildAbbreviation": Binding("TUIText", "target.personal.comp.top.guild.abbr"),
    "title": Binding("TUIText", "target.personal.comp.top.guild.title"),
    "badges": Binding("TUIText", "target.personal.comp.pageMedals.text"),
    "bio": Binding("TUIText", "target.personal.comp.pageBio.text"),
}

OPTIONAL_BINDINGS = {
    "imageFoodBar": Binding(
        "TUIImage", "target.status.comp.top.top.pageStomach.food_bar.filler"
    ),
    "imageFoodBarBack": Binding(
        "TUIImage", "target.status.comp.top.top.pageStomach.food_bar.back_bar"
    ),
    "imageDrinkBar": Binding(
        "TUIImage", "target.status.comp.top.top.pageStomach.drink_bar.filler"
    ),
    "imageDrinkBarBack": Binding(
        "TUIImage", "target.status.comp.top.top.pageStomach.drink_bar.back_bar"
    ),
    "forcepowerbar": Binding("TUIImage", "target.status.comp.force.inner.bar.value"),
    "forcepowertext": Binding(
        "TUIText", "target.status.comp.force.inner.textback.text"
    ),
    "buttonStatMigration": Binding("TUIButton", "buttonStatMigration"),
}

FORBIDDEN_BINDING_NAMES = {
    "attrAcidValue",
    "attrActionBar",
    "attrActionBarBack",
    "attrActionValue",
    "attrAgilityValue",
    "attrColdValue",
    "attrConstitutionValue",
    "attrElectricityValue",
    "attrEnergyValue",
    "attrHealthBar",
    "attrHealthBarBack",
    "attrHealthValue",
    "attrHeatValue",
    "attrKineticValue",
    "attrLuckValue",
    "attrPrecisionValue",
    "attrStaminaValue",
    "attrStrengthValue",
    "buttonCollections",
    "buttonCybernetics",
    "characterAttributes",
    "chronicleRating",
    "chronicleUpdate",
    "gcwBackImperial",
    "gcwBackRebel",
    "gcwHighImpLabel",
    "gcwHighRankImp",
    "gcwHighRankReb",
    "gcwHighRebLabel",
    "gcwImperialProgBar",
    "gcwLifetime",
    "gcwPercent",
    "gcwPercentSign",
    "gcwPoints",
    "gcwProgressBarback",
    "gcwProgressText",
    "gcwPvPKills",
    "gcwPvPLifetime",
    "gcwRank",
    "gcwRebelProgBar",
    "gcwTimer",
    "gcwTimerText",
    "levelValue",
    "paperDoll",
    "personalTop",
    "skillsText",
    "textPvPStatus",
}

EXPECTED_TABS = (
    ("Status", "target.status"),
    ("Personal", "target.personal"),
    ("Factions", "target.factions"),
)

EXPECTED_HAM_ROWS = (
    "health",
    "strength",
    "constitution",
    "action",
    "quickness",
    "stamina",
    "mind",
    "focus",
    "willpower",
)

EXPECTED_HAM_COLUMNS = {
    "attribute": "text",
    "value": "text",
    "bar": "widget",
    "wounds": "integer",
    "buff": "integer",
    "encumbrance": "integer",
}


@dataclass(frozen=True)
class SourceBindingCall:
    ui_type: str
    name: str
    optional: bool


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


def source_binding_calls(source: str) -> list[SourceBindingCall]:
    calls = []
    pattern = re.compile(
        r"getCodeDataObject\s*\(\s*"
        r"(?P<type>TUI\w+)\s*,\s*"
        r"[^,]+?\s*,\s*"
        r'"(?P<name>[^"]+)"\s*'
        r"(?P<optional>,\s*true\s*)?\)",
        re.DOTALL,
    )
    for match in pattern.finditer(source):
        calls.append(
            SourceBindingCall(
                ui_type=match.group("type"),
                name=match.group("name"),
                optional=match.group("optional") is not None,
            )
        )
    return calls


def immediate_named_child(parent: ElementTree.Element, name: str) -> ElementTree.Element:
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


def resolve_asset_path(root: ElementTree.Element, path: str) -> ElementTree.Element:
    current = root
    for component in path.split("."):
        current = immediate_named_child(current, component)
    return current


def code_data_properties(page: ElementTree.Element) -> dict[str, tuple[str, str]]:
    code_data = next(
        (
            child
            for child in page
            if child.tag == "Data" and child.attrib.get("Name") == "CodeData"
        ),
        None,
    )
    if code_data is None:
        raise AssertionError("CharacterSheet has no immediate CodeData block")
    return {
        name.casefold(): (name, value)
        for name, value in code_data.attrib.items()
        if name != "Name"
    }


class PrecuCharacterSheetSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = CHARACTER_SHEET_CPP.read_text(encoding="utf-8")
        cls.calls = source_binding_calls(cls.source)
        cls.calls_by_name = {
            name.casefold(): [call for call in cls.calls if call.name.casefold() == name.casefold()]
            for name in {call.name for call in cls.calls}
        }

    def test_factory_keeps_the_publish14_pda_page(self) -> None:
        create_into = function_body(
            self.source, "SwgCuiCharacterSheet* SwgCuiCharacterSheet::createInto"
        )
        self.assertIn('UIPage::DuplicateInto(parent, "/PDA.CharacterSheet")', create_into)

    def test_source_binds_the_required_publish14_contract(self) -> None:
        for name, binding in REQUIRED_BINDINGS.items():
            with self.subTest(binding=name):
                calls = self.calls_by_name.get(name.casefold(), [])
                self.assertTrue(calls, f"missing required CodeData binding {name!r}")
                self.assertTrue(
                    any(call.ui_type == binding.ui_type and not call.optional for call in calls),
                    f"{name!r} must be a required {binding.ui_type} binding",
                )

    def test_conditional_publish14_features_are_optional_bindings(self) -> None:
        for name, binding in OPTIONAL_BINDINGS.items():
            with self.subTest(binding=name):
                calls = self.calls_by_name.get(name.casefold(), [])
                self.assertTrue(calls, f"missing optional CodeData binding {name!r}")
                self.assertTrue(
                    any(call.ui_type == binding.ui_type and call.optional for call in calls),
                    f"{name!r} must be an optional {binding.ui_type} binding",
                )

    def test_source_extends_the_three_publish14_tabs_with_appearance(self) -> None:
        enum_match = re.search(
            r"enum\s+TabPages\s*\{(?P<body>.*?)\}\s*;", self.source, re.DOTALL
        )
        self.assertIsNotNone(enum_match, "missing TabPages enum")
        identifiers = re.findall(r"\bTAB_[A-Za-z0-9_]+\b", enum_match.group("body"))
        self.assertEqual(
            [
                "TAB_status",
                "TAB_personal",
                "TAB_factions",
                "TAB_appearance",
                "TAB_numTabPages",
            ],
            identifiers,
        )

    def test_character_sheet_response_populates_precu_personal_fields(self) -> None:
        receive_message = function_body(
            self.source, "void SwgCuiCharacterSheet::receiveMessage"
        )
        for getter in (
            "getBornDate",
            "getPlayed",
            "getBankLoc",
            "getBankPlanet",
        ):
            with self.subTest(getter=getter):
                self.assertIsNotNone(
                    re.search(rf"\.{getter}\s*\(", receive_message),
                    f"receiveMessage must consume CharacterSheetResponseMessage::{getter}()",
                )

    def test_self_only_responses_cannot_leak_into_remote_examine_mode(self) -> None:
        receive_message = function_body(
            self.source, "void SwgCuiCharacterSheet::receiveMessage"
        )
        self.assertGreaterEqual(
            receive_message.count("if (!isExaminingSelf())"),
            3,
            "faction, CharacterSheetResponse, and residence responses must be self-only",
        )

    def test_target_switch_clears_asynchronous_subject_data(self) -> None:
        examine_mode = function_body(
            self.source, "void SwgCuiCharacterSheet::setExamineMode"
        )
        for member in (
            "m_guild",
            "m_guildAbbreviation",
            "m_guildTitle",
            "m_badgeWindow",
            "m_bio",
        ):
            with self.subTest(member=member):
                self.assertRegex(examine_mode, rf"{member}->Clear\s*\(\s*\)")

    def test_missing_later_badge_catalog_does_not_claim_every_badge(self) -> None:
        badge_window = function_body(
            self.source, "void SwgCuiCharacterSheet::refreshBadgeWindow"
        )
        guard_start = badge_window.index("if (allBadges.empty())")
        unearned_start = badge_window.index(
            "CuiStringIdsCharacterSheet::badges_unearned"
        )
        self.assertLess(guard_start, unearned_start)
        guard = badge_window[guard_start:unearned_start]
        self.assertIn("m_badgeWindow->SetLocalText(current);", guard)
        self.assertIn("return;", guard)

    def test_stomach_and_force_status_are_self_only(self) -> None:
        status_bars = function_body(
            self.source, "void SwgCuiCharacterSheet::updateStatusBars"
        )
        self.assertRegex(
            status_bars,
            r"isExaminingSelf\s*\(\s*\)\s*\?\s*"
            r"m_playerObjectWatcher->getPointer\s*\(\s*\)\s*:\s*0",
        )

    def test_later_character_sheet_bindings_and_generators_are_absent(self) -> None:
        bound_names = {call.name.casefold() for call in self.calls}
        forbidden = sorted(
            name for name in FORBIDDEN_BINDING_NAMES if name.casefold() in bound_names
        )
        self.assertEqual([], forbidden)

        for forbidden_source in (
            "gcwRebelRankIcon",
            "gcwImperialRankIcon",
            "datatables/expertise/skill_mod_listing.iff",
            "TAB_skillMods",
            "TAB_gcw",
        ):
            with self.subTest(forbidden_source=forbidden_source):
                self.assertNotIn(forbidden_source, self.source)


class PrecuCharacterSheetAssetContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not CHARACTER_SHEET_ASSET.is_file():
            message = (
                f"Publish 14 Character Sheet asset not found at "
                f"{CHARACTER_SHEET_ASSET}. Set {ASSET_ENVIRONMENT_VARIABLE} to its "
                "extracted path."
            )
            if ASSET_OVERRIDE:
                raise AssertionError(message)
            raise unittest.SkipTest(message)

        cls.asset_bytes = CHARACTER_SHEET_ASSET.read_bytes()
        cls.page = ElementTree.fromstring(cls.asset_bytes)
        cls.properties = code_data_properties(cls.page)

    def test_asset_is_the_locked_patch12_winner(self) -> None:
        self.assertEqual(EXPECTED_ASSET_SIZE, len(self.asset_bytes))
        self.assertEqual(
            EXPECTED_ASSET_SHA256,
            hashlib.sha256(self.asset_bytes).hexdigest(),
        )
        self.assertEqual("Page", self.page.tag)
        self.assertEqual("CharacterSheet", self.page.attrib.get("Name"))

    def test_required_and_optional_codedata_paths_resolve_to_expected_types(self) -> None:
        for name, binding in {**REQUIRED_BINDINGS, **OPTIONAL_BINDINGS}.items():
            with self.subTest(binding=name):
                property_name, asset_path = self.properties[name.casefold()]
                self.assertEqual(name.casefold(), property_name.casefold())
                self.assertEqual(binding.asset_path.casefold(), asset_path.casefold())
                target = resolve_asset_path(self.page, asset_path)
                self.assertEqual(binding.ui_type.removeprefix("TUI"), target.tag)

    def test_asset_has_exactly_the_three_publish14_tabs(self) -> None:
        tabbed_pane = resolve_asset_path(self.page, self.properties["tabs"][1])
        self.assertEqual("TabData", tabbed_pane.attrib.get("DataSource"))
        self.assertEqual("target", tabbed_pane.attrib.get("TargetPage"))

        tab_data = immediate_named_child(self.page, "TabData")
        tabs = tuple(
            (child.attrib.get("Name"), child.attrib.get("Target"))
            for child in tab_data
            if child.tag == "Data"
        )
        self.assertEqual(EXPECTED_TABS, tabs)

    def test_asset_has_the_publish14_nine_row_six_column_ham_table(self) -> None:
        attributes_page = resolve_asset_path(
            self.page, self.properties["pageattributes"][1]
        )
        nested_properties = code_data_properties(attributes_page)
        self.assertEqual("table", nested_properties["table"][1].casefold())
        self.assertEqual("textbg.text", nested_properties["text"][1].casefold())

        table = resolve_asset_path(attributes_page, nested_properties["table"][1])
        text = resolve_asset_path(attributes_page, nested_properties["text"][1])
        self.assertEqual("Table", table.tag)
        self.assertEqual("Text", text.tag)

        container = immediate_named_child(attributes_page, "containerall")
        columns = [child for child in container if child.tag == "DataSource"]
        self.assertEqual(6, len(columns))
        self.assertEqual(set(EXPECTED_HAM_COLUMNS), {column.attrib["Name"] for column in columns})

        for column in columns:
            name = column.attrib["Name"]
            with self.subTest(column=name):
                self.assertEqual(EXPECTED_HAM_COLUMNS[name], column.attrib.get("Type"))
                rows = tuple(
                    child.attrib.get("Name", "").casefold()
                    for child in column
                    if child.tag == "Data"
                )
                self.assertEqual(EXPECTED_HAM_ROWS, rows)


if __name__ == "__main__":
    unittest.main()
