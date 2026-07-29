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
IMAGE_DESIGNER_CPP = UI_ROOT / "src/shared/page/SwgCuiImageDesignerDesigner.cpp"
IMAGE_DESIGNER_H = UI_ROOT / "src/shared/page/SwgCuiImageDesignerDesigner.h"
FACTORY_CPP = UI_ROOT / "src/shared/core/SwgCuiMediatorFactorySetup.cpp"
PROJECT = UI_ROOT / "build/win32/swgClientUserInterface.vcxproj"
SHARED_IMAGE_DESIGNER_CPP = REPOSITORY_ROOT / (
    "src/engine/shared/library/sharedGame/src/shared/core/"
    "SharedImageDesignerManager.cpp"
)
IMAGE_DESIGN_CHANGE_CPP = REPOSITORY_ROOT / (
    "src/engine/shared/library/sharedNetworkMessages/src/shared/"
    "clientGameServer/ImageDesignChangeMessage.cpp"
)


class Publish14StatMigrationRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = MEDIATOR_CPP.read_text(encoding="utf-8")
        cls.header = MEDIATOR_H.read_text(encoding="utf-8")
        cls.character_sheet = CHARACTER_SHEET_CPP.read_text(encoding="utf-8")
        cls.image_designer = IMAGE_DESIGNER_CPP.read_text(encoding="utf-8")
        cls.image_designer_header = IMAGE_DESIGNER_H.read_text(encoding="utf-8")
        cls.factory = FACTORY_CPP.read_text(encoding="utf-8")
        cls.project = PROJECT.read_text(encoding="utf-8")
        cls.shared_image_designer = SHARED_IMAGE_DESIGNER_CPP.read_text(
            encoding="utf-8"
        )
        cls.image_design_change = IMAGE_DESIGN_CHANGE_CPP.read_text(encoding="utf-8")

    def test_authentic_widget_contract_is_bound_in_wire_order(self):
        match = re.search(
            r"s_statPagePaths\[s_attributeCount\]\s*=\s*\{(?P<body>.*?)\};",
            self.source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(match)
        names = re.findall(r'"([^"]+)"', match.group("body"))
        self.assertEqual(
            names,
            [
                "stats.comp.health",
                "stats.comp.strength",
                "stats.comp.constitution",
                "stats.comp.action",
                "stats.comp.quickness",
                "stats.comp.stamina",
                "stats.comp.mind",
                "stats.comp.focus",
                "stats.comp.willpower",
            ],
        )
        for binding in ("buttonCancel", "buttonOk", "stats.points.points"):
            self.assertIn(f'"{binding}"', self.source)
        for nested in ("stat", "current", "target"):
            self.assertIn(f'"{nested}"', self.source)

    def test_retail_visual_paths_do_not_depend_on_duplicated_uidata(self):
        self.assertIn('page.GetObjectFromPath("buttonCancel", TUIButton)', self.source)
        self.assertIn('page.GetObjectFromPath("buttonOk", TUIButton)', self.source)
        self.assertIn(
            'page.GetObjectFromPath("stats.points.points", TUIText)', self.source
        )
        self.assertNotIn('m_statPages[i]->GetChild("CodeData")', self.source)

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
        format_matches = re.findall(
            r'_snprintf\(buffer, sizeof\(buffer\), "(?P<format>[^"]+)"',
            self.source,
        )
        self.assertIn(10, [format_string.count("%d") for format_string in format_matches])
        self.assertIn('enqueueCommand("requestSetStatMigrationData"', self.source)
        self.assertIn('enqueueCommand("requestStatMigrationData"', self.source)

    def test_required_retail_commands_target_the_player(self):
        self.assertIn(
            'enqueueCommand("requestStatMigrationData", player->getNetworkId()',
            self.source,
        )
        self.assertIn(
            'enqueueCommand("requestSetStatMigrationData", player->getNetworkId()',
            self.source,
        )
        self.assertNotIn(
            'enqueueCommand("requestStatMigrationData", NetworkId::cms_invalid',
            self.source,
        )

    def test_retail_numeric_fields_do_not_repeat_the_xml_labels(self):
        self.assertIn("Unicode::String formatStatValue(int value)", self.source)
        self.assertIn("SetLocalText(formatStatValue(m_pointsLeft))", self.source)
        self.assertIn("SetLocalText(formatStatValue(m_current[index]))", self.source)
        self.assertIn("SetLocalText(formatStatValue(m_targets[index]))", self.source)
        self.assertNotIn('formatStatText("stat_current"', self.source)

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

    def test_retail_image_designer_checkbox_is_bound_and_registered(self):
        self.assertIn("UICheckbox * m_doStatMigration", self.image_designer_header)
        self.assertIn(
            'getCodeDataObject (TUICheckbox, m_doStatMigration, "checkboxDoStatMigration")',
            self.image_designer,
        )
        self.assertIn(
            "registerMediatorObject (*m_doStatMigration, true)",
            self.image_designer,
        )

    def test_checkbox_drives_retained_stat_migration_design_type(self):
        self.assertIn("if(context == m_doStatMigration)", self.image_designer)
        self.assertIn(
            "session.designType = ImageDesignChangeMessage::DT_STAT_MIGRATION",
            self.image_designer,
        )
        self.assertIn(
            "session.designType = ImageDesignChangeMessage::DT_COSMETIC",
            self.image_designer,
        )
        self.assertNotIn('enqueueCommand("requestStatMigrationStart"', self.image_designer)
        self.assertNotIn('enqueueCommand("requestStatMigrationStop"', self.image_designer)

    def test_checkbox_is_overlaid_without_a_non_self_salon_session(self):
        self.assertIn(
            "m_terminalId != NetworkId::cms_invalid && m_recipientId != player->getNetworkId()",
            self.image_designer,
        )
        self.assertIn('statMigrationParent->GetChild("Overlay")', self.image_designer)
        self.assertIn("overlay->SetVisible(!canMigrateStats)", self.image_designer)

    def test_retail_image_designer_timer_and_design_type_are_preserved(self):
        self.assertIn(
            "ConfigSharedGame::getImageDesignerStatMigrationSessionTimeSeconds()",
            self.shared_image_designer,
        )
        self.assertIn(
            "bool const statMigrationRequested = session.designType == ImageDesignChangeMessage::DT_STAT_MIGRATION",
            self.shared_image_designer,
        )
        self.assertIn(
            "statMigrationRequested ? ImageDesignChangeMessage::DT_STAT_MIGRATION",
            self.shared_image_designer,
        )

    def test_image_designer_start_time_keeps_the_32_bit_retail_wire_width(self):
        self.assertIn(
            "int const startingTimeWire = static_cast<int>(msg->getStartingTime())",
            self.image_design_change,
        )
        self.assertIn("Archive::put(target, startingTimeWire)", self.image_design_change)
        self.assertIn("int tempTimeWire = 0", self.image_design_change)
        self.assertIn(
            "msg->setStartingTime(static_cast<time_t>(tempTimeWire))",
            self.image_design_change,
        )
        self.assertNotIn(
            "Archive::put(target, msg->getStartingTime())", self.image_design_change
        )


if __name__ == "__main__":
    unittest.main()
