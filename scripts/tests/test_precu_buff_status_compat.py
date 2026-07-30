import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
STATUS_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiStatusGround.cpp"
)
BUFF_UTILS_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/core/"
    "SwgCuiBuffUtils.cpp"
)
BUFF_MANAGER_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientGame/src/shared/core/ClientBuffManager.cpp"
)
BUFF_DISPLAY_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiBuffDisplay.cpp"
)
HUD_MANAGER_SOURCE = REPOSITORY_ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiHudWindowManagerGround.cpp"
)


class Publish14BuffStatusCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.status_source = STATUS_SOURCE.read_text(encoding="utf-8")
        cls.buff_utils_source = BUFF_UTILS_SOURCE.read_text(encoding="utf-8")
        cls.buff_manager_source = BUFF_MANAGER_SOURCE.read_text(encoding="utf-8")
        cls.buff_display_source = BUFF_DISPLAY_SOURCE.read_text(encoding="utf-8")
        cls.hud_manager_source = HUD_MANAGER_SOURCE.read_text(encoding="utf-8")

    def test_publish14_combined_status_volume_is_supported(self):
        self.assertIn("if(m_volumeStates && m_sampleStateIcon)", self.status_source)
        self.assertNotIn(
            "if(m_volumeStates && m_debuffStates && m_sampleStateIcon)",
            self.status_source,
        )
        self.assertIn(
            "UIVolumePage & debuffPage = m_debuffStates ? *m_debuffStates : "
            "*m_volumeStates;",
            self.status_source,
        )
        self.assertIn("if (!m_debuffStates)", self.status_source)

    def test_player_uses_attribmod_without_collapsing_ham_width(self):
        self.assertIn(
            "if (m_statusType == ST_player && !m_debuffStates)",
            self.status_source,
        )
        self.assertIn(
            "SwgCuiBuffUtils::clearBuffIcons(*m_volumeStates);",
            self.status_source,
        )
        self.assertIn(
            "pageSetVisible(m_volumeStates, false)",
            self.status_source,
        )

    def test_authored_placeholders_are_removed_before_population(self):
        self.assertIn("void removePlaceholderIcons(UIVolumePage & page)", self.buff_utils_source)
        self.assertIn(
            "if (!object->GetPropertyInteger(BUFF_ID_PROPERTY, buffId))",
            self.buff_utils_source,
        )
        self.assertIn("removePlaceholderIcons(buffPage);", self.buff_utils_source)

    def test_dynamic_icons_are_visible_and_only_report_successful_creation(self):
        self.assertGreaterEqual(
            self.buff_utils_source.count("SetOpacity(1.0f);"),
            2,
        )
        new_buff_section = self.buff_utils_source.split(
            "// add any new buffs to the display", 1
        )[1]
        before_success = new_buff_section.split("if (buffIcon != NULL)", 1)[0]
        self.assertNotIn("returnValue |= UBRT_has", before_success)
        self.assertIn(
            "returnValue |= (ClientBuffManager::getBuffIsDebuff",
            new_buff_section,
        )

    def test_short_lived_buffs_do_not_blink_for_their_entire_lifetime(self):
        self.assertEqual(
            2,
            self.buff_utils_source.count(
                "buff.m_duration > CREATURE_BUFF_BLINK_TIME"
            ),
        )

    def test_shared_volume_is_enumerated_and_cleared_only_once(self):
        self.assertIn(
            "bool const sharedStatusPage = (&buffPage == &debuffPage);",
            self.buff_utils_source,
        )
        self.assertEqual(
            2,
            len(re.findall(r"if \(!sharedStatusPage\)", self.buff_utils_source)),
        )
        self.assertIn(
            "if (!sharedStatusPage && debuffPage.GetChildCount() > 0)",
            self.buff_utils_source,
        )

    def test_missing_later_icon_styles_use_publish14_polarity_fallbacks(self):
        self.assertIn('"/Styles.Icon.buffs.healthBuff"', self.buff_manager_source)
        self.assertIn('"/Styles.Icon.buffs.healthDebuff"', self.buff_manager_source)
        self.assertLess(
            self.buff_manager_source.index("compatibleFallback"),
            self.buff_manager_source.index("CuiIconManager::getFallback", 0),
        )

    def test_catalog_audit_checks_every_visible_status_icon(self):
        self.assertIn(
            "void ClientBuffManager::auditVisibleBuffCatalog",
            self.buff_manager_source,
        )
        audit = self.buff_manager_source.split(
            "void ClientBuffManager::auditVisibleBuffCatalog", 1
        )[1]
        self.assertIn("for (std::unordered_map<uint32, BuffRecord>", audit)
        self.assertIn("if (!record.visible)", audit)
        self.assertIn("++result.positiveCount", audit)
        self.assertIn("++result.debuffCount", audit)
        self.assertIn("++result.authoredIconMissCount", audit)
        self.assertIn("++result.unresolvedIconCount", audit)

    def test_internal_effect_parameters_do_not_truncate_tooltips(self):
        unknown_effect = self.buff_manager_source.split(
            'WARNING(true, ("Unknown effect crc', 1
        )[1].split("EffectRecord const &effectRecord", 1)[0]
        self.assertIn("continue;", unknown_effect)
        self.assertNotIn("return;", unknown_effect)

    def test_existing_icons_refresh_duration_description_and_timestamp(self):
        existing = self.buff_utils_source.split(
            "// the buff is on the creature's list", 1
        )[1].split("buffs.erase(found);", 1)[0]
        self.assertGreaterEqual(
            existing.count("ClientBuffManager::getBuffDescription(buff, tooltipStr)"),
            2,
        )
        self.assertGreaterEqual(
            existing.count("SetPropertyInteger(BUFF_TIMESTAMP_PROPERTY, buff.m_timestamp)"),
            2,
        )
        self.assertIn(
            "SetPropertyInteger(BUFF_LENGTH_PROPERTY, static_cast<int>(buff.m_duration))",
            existing,
        )

    def test_each_buff_uses_its_own_celestial_or_played_time_base(self):
        self.assertGreaterEqual(
            self.buff_utils_source.count(
                "uint32 const currentTime = ClientBuffManager::getBuffIsCelestial"
            ),
            2,
        )

    def test_optional_debuff_volume_is_guarded_for_pointer_input(self):
        right_mouse = re.search(
            r"if \(msg\.Type == UIMessage::RightMouseUp\).*?"
            r"const CreatureObject \* const player",
            self.status_source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(right_mouse)
        self.assertIn("if (m_debuffStates)", right_mouse.group(0))

    def test_publish14_attribute_modifier_panel_is_bound_to_real_buffs(self):
        self.assertIn('GetObjectFromPath("AttribMod", TUIPage)', self.hud_manager_source)
        self.assertIn("new SwgCuiBuffDisplay(*mediatorPage)", self.hud_manager_source)
        self.assertIn('"VolumePage"', self.buff_display_source)
        self.assertIn('"sampleIcon"', self.buff_display_source)
        self.assertIn("m_volume->Clear();", self.buff_display_source)
        self.assertIn(
            "SwgCuiBuffUtils::updateBuffs(",
            self.buff_display_source,
        )

    def test_attribute_modifier_panel_hides_when_empty_and_honors_close(self):
        self.assertIn(
            "getPage().SetVisible(hasActiveEffects && !m_userClosed);",
            self.buff_display_source,
        )
        self.assertIn("if (context == m_closeButton)", self.buff_display_source)
        self.assertIn("getPage().SetVisible(false);", self.buff_display_source)


if __name__ == "__main__":
    unittest.main()
