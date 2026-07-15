import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
UI_ROOT = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface"
)
MEDIATOR_CPP = UI_ROOT / "src/shared/page/SwgCuiStatMigration.cpp"
MEDIATOR_H = UI_ROOT / "src/shared/page/SwgCuiStatMigration.h"
CHARACTER_SHEET_CPP = UI_ROOT / "src/shared/page/SwgCuiCharacterSheet.cpp"
FACTORY_CPP = UI_ROOT / "src/shared/core/SwgCuiMediatorFactorySetup.cpp"
PROJECT = UI_ROOT / "build/win32/swgClientUserInterface.vcxproj"


class Publish14StatMigrationRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = MEDIATOR_CPP.read_text(encoding="utf-8")
        cls.header = MEDIATOR_H.read_text(encoding="utf-8")
        cls.character_sheet = CHARACTER_SHEET_CPP.read_text(encoding="utf-8")
        cls.factory = FACTORY_CPP.read_text(encoding="utf-8")
        cls.project = PROJECT.read_text(encoding="utf-8")

    def test_authentic_widget_contract_is_bound_in_wire_order(self):
        match = re.search(
            r"s_statPageCodeDataNames\[s_attributeCount\]\s*=\s*\{(?P<body>.*?)\};",
            self.source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(match)
        names = re.findall(r'"([^"]+)"', match.group("body"))
        self.assertEqual(
            names,
            [
                "statsHealth",
                "statsStrength",
                "statsConstitution",
                "statsAction",
                "statsQuickness",
                "statsStamina",
                "statsMind",
                "statsFocus",
                "statsWillPower",
            ],
        )
        for binding in ("buttonCancel", "buttonOk", "pointsLeft"):
            self.assertIn(f'"{binding}"', self.source)
        for nested in ("stat", "current", "target"):
            self.assertIn(f'"{nested}"', self.source)

    def test_server_owned_targets_gate_the_controls(self):
        self.assertIn("setMigrationControlsEnabled(false);", self.source)
        self.assertIn("connectToMessage(StatMigrationTargetsMessage::cms_name)", self.source)
        self.assertIn("StatMigrationTargetsMessage const targetsMessage(reader)", self.source)
        self.assertIn("m_receivedTargets = true;", self.source)
        self.assertIn("setMigrationControlsEnabled(true);", self.source)

    def test_racial_bounds_and_unmodified_maxima_drive_each_slider(self):
        self.assertIn("PlayerCreationManager::getRacialMinMaxes", self.source)
        self.assertIn("getUnmodifiedMaxAttribute", self.source)
        self.assertIn("SetLowerLimit(m_minMaxes[i].first)", self.source)
        self.assertIn("SetUpperLimit(m_minMaxes[i].second)", self.source)

    def test_slider_delta_cannot_overspend_server_points(self):
        self.assertIn("if (delta > m_pointsLeft)", self.source)
        self.assertIn("requestedTarget = oldTarget + delta", self.source)
        self.assertIn("if (m_pointsLeft - delta < 0)", self.source)
        self.assertIn("m_pointsLeft -= delta", self.source)

    def test_submit_requires_zero_points_and_sends_legacy_ten_integers(self):
        self.assertIn("if (m_pointsLeft != 0)", self.source)
        self.assertIn("statmig_usealltpoints.localize()", self.source)
        format_match = re.search(
            r'_snprintf\(buffer, sizeof\(buffer\), "(?P<format>[^"]+)"',
            self.source,
        )
        self.assertIsNotNone(format_match)
        self.assertEqual(format_match.group("format").count("%d"), 10)
        self.assertIn('enqueueCommand("requestSetStatMigrationData"', self.source)
        self.assertIn('enqueueCommand("requestStatMigrationData"', self.source)

    def test_character_sheet_exposes_only_the_self_migration_entry_point(self):
        self.assertIn("registerMediatorObject(*m_statMigrationButton, true)", self.character_sheet)
        self.assertIn("m_statMigrationButton->SetVisible(examiningSelf)", self.character_sheet)
        self.assertIn("context == m_statMigrationButton && isExaminingSelf()", self.character_sheet)
        self.assertIn(
            "CuiMediatorFactory::activateInWorkspace(CuiMediatorTypes::WS_StatMigration)",
            self.character_sheet,
        )

    def test_factory_and_x64_project_include_the_mediator(self):
        self.assertIn('#include "swgClientUserInterface/SwgCuiStatMigration.h"', self.factory)
        self.assertIn(
            'MAKE_SWG_CTOR_WS (StatMigration,                  "/pda.StatMigration")',
            self.factory,
        )
        self.assertIn("SwgCuiStatMigration.cpp", self.project)
        self.assertIn("SwgCuiStatMigration.h", self.project)


if __name__ == "__main__":
    unittest.main()
