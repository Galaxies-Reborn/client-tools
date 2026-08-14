from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OPT_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOpt.cpp"
OPT_PAGE_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiOptGalaxiesReborn.cpp"
STATUS_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiMfdStatusBar.cpp"
STATUS_GROUND_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiStatusGround.cpp"
GROUP_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiGroup.cpp"
COMPOSITE_CPP = ROOT / "src/external/3rd/library/ui/src/shared/UIComposite.cpp"
HUD_MANAGER_CPP = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudWindowManagerGround.cpp"
PREFERENCES_CPP = ROOT / "src/engine/client/library/clientUserInterface/src/shared/core/CuiPreferences.cpp"
PROJECT = ROOT / "src/game/client/library/swgClientUserInterface/build/win32/swgClientUserInterface.vcxproj"


class GalaxiesRebornHamEnhanceTests(unittest.TestCase):
    def test_options_page_is_compiled_and_binds_the_saved_preference(self) -> None:
        project = PROJECT.read_text(encoding="utf-8")
        page = OPT_PAGE_CPP.read_text(encoding="utf-8")
        preferences = PREFERENCES_CPP.read_text(encoding="utf-8")

        self.assertIn("SwgCuiOptGalaxiesReborn.cpp", project)
        self.assertIn('"checkHamEnhance"', page)
        self.assertIn("if (checkbox)", page)
        self.assertIn("CuiPreferences::setHamEnhance", page)
        self.assertIn("CuiPreferences::getHamEnhance", page)
        self.assertIn("REGISTER_OPTION_USER(hamEnhance);", preferences)
        self.assertIn("bool        ms_hamEnhance                       = true;", preferences)
        self.assertIn("ms_hamEnhance = true;", preferences)
        self.assertIn("UNREF(enabled);", preferences)
        self.assertIn("return true;", preferences)
        self.assertIn("checkbox->SetChecked(true, false);", page)
        self.assertIn("checkbox->SetEnabled(false);", page)

    def test_keymap_remains_a_standalone_controls_route(self) -> None:
        options = OPT_CPP.read_text(encoding="utf-8")
        self.assertIn('narrowPath == "target.keymap"', options)
        self.assertIn("m_standaloneKeymap->activate();", options)
        self.assertIn("m_tabs->SetActiveTab(9);", options)

    def test_live_status_bars_apply_and_restore_the_reviewed_layout(self) -> None:
        bar = STATUS_CPP.read_text(encoding="utf-8")
        status = STATUS_GROUND_CPP.read_text(encoding="utf-8")
        group = GROUP_CPP.read_text(encoding="utf-8")
        composite = COMPOSITE_CPP.read_text(encoding="utf-8")

        self.assertIn('UILowerString("HamEnhanceEligible")', bar)
        self.assertIn("m_valueText->SetVisible(enabled)", bar)
        self.assertNotIn("SetSize(", bar.split("updateHamEnhanceVisibility", 1)[1].split("performActivate", 1)[0])
        self.assertNotIn("compactOverheadBar", bar)
        self.assertNotIn("collectHamEnhanceWidgets", bar)

        self.assertIn('s_hamEnhanceLayoutRoot("HamEnhanceLayoutRoot")', status)
        self.assertIn('UILowerString("HamStandardSize")', status)
        self.assertIn('UILowerString("HamEnhancedSize")', status)
        self.assertIn("collectHamEnhanceLayoutWidgets", status)
        self.assertIn("isLayoutApplied", status)
        self.assertIn("m_layoutProbes", status)
        self.assertIn("isRuntimeSizedHamWidget", status)
        self.assertIn("m_root->SetDoNotPackChildren(true)", status)
        self.assertIn("boundaryPage->SetDoNotPackChildren(true)", status)
        self.assertIn("actual != expected", status)
        self.assertIn("m_root->ForcePackChildren();", status)
        self.assertNotIn("enhancedHeight", status)
        self.assertNotIn("compactOverheadBar", status)
        self.assertIn("if (GetDoNotPackChildren())", composite)
        self.assertLess(
            composite.index("if (GetDoNotPackChildren())"),
            composite.index("switch(mSpacingType)"),
        )
        self.assertIn("pageStyle = PS_ham;", status)
        self.assertIn("isFactionalCombatCreature", status)
        self.assertIn("PvpData::isImperialFactionId", status)
        self.assertIn("PvpData::isRebelFactionId", status)
        self.assertNotIn("creature.getAttribute(Attributes::Action) > 0", status)
        self.assertIn("m_compositePage && !m_hamEnhanceState", status)

        all_targets = (
            ROOT
            / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiAllTargets.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("if (!statusPage.GetDoNotPackChildren())", all_targets)

        for source in (status, group):
            self.assertIn("SetMinimumSize(UISize(0, 0))", source)
            self.assertLess(
                source.index("PropertyName::MaximumSize, maximumSize"),
                source.index("PropertyName::MinimumSize, minimumSize"),
            )

        # UIWidget::SetLocation() refreshes far/center anchor metadata using the
        # current dimensions. The layout transaction must therefore set Size
        # before Location on every target and runtime-duplicated target cycle.
        property_pairs = status.split(
            "const HamEnhancePropertyPair s_hamEnhancePropertyPairs[]", 1
        )[1].split("};", 1)[0]
        self.assertLess(
            property_pairs.index("UIWidget::PropertyName::Size"),
            property_pairs.index("UIWidget::PropertyName::Location"),
        )
        self.assertIn("boundary && boundary != m_root", status)
        self.assertIn('s_hamEnhanceHideStates("HamEnhanceHideStates")', status)
        self.assertIn(
            "GetPropertyBoolean(s_hamEnhanceHideStates, suppressInlineStates)",
            status,
        )
        self.assertIn(
            "pageSetVisible(m_volumeStates, hasStatusIcons && !suppressInlineStates)",
            status,
        )

        self.assertIn('UILowerString("HamStandardSizeIncrement")', group)
        self.assertIn('UILowerString("HamEnhancedSizeIncrement")', group)
        self.assertIn("collectGroupHamEnhanceLayoutWidgets", group)
        self.assertIn("std::sort(memberPages.begin(), memberPages.end()", group)
        self.assertIn("widget->SetSizeIncrement(UISize(1, 1))", group)
        self.assertIn("propertyPairCount - 1", group)
        self.assertIn("s_groupWindowChromeHeight = 5", group)
        self.assertIn("sizeIncrement.y * windowSize + s_groupWindowChromeHeight", group)
        self.assertIn("getPage().SetSizeIncrement(UISize(1, 1))", group)
        self.assertIn("getPage().SetSizeIncrement(sizeIncrement)", group)
        self.assertIn("resizeGroupWindow(static_cast<int>(memberPages.size()))", group)

        bar_construction = status.index("for(i = 0; i < SwgCuiStatusGround::HAMBarPageCount; ++i)")
        state_construction = status.index("new SwgCuiStatusGroundHamEnhanceState")
        self.assertLess(bar_construction, state_construction)

        constructor_tail = status.split(
            "new SwgCuiStatusGroundHamEnhanceState", 1
        )[1].split("SwgCuiStatusGround::~SwgCuiStatusGround", 1)[0]
        self.assertNotIn("updateHamEnhanceLayout();", constructor_tail)

        update_body = status.split(
            "void SwgCuiStatusGround::update(float deltaTimeSecs)", 1
        )[1].split("void SwgCuiStatusGround::updateHamEnhanceLayout()", 1)[0]
        self.assertLess(
            update_body.index("m_compositePage->WrapChildren();"),
            update_body.index("updateHamEnhanceLayout();"),
        )
        self.assertLess(
            update_body.index("updateHamEnhanceLayout();"),
            update_body.index("individualUiScaleRoot->SetScale(uiScale);"),
        )
        update_layout = status.split(
            "void SwgCuiStatusGround::updateHamEnhanceLayout()", 1
        )[1].split("UIPage * SwgCuiStatusGround::getIndividualUiScaleRoot", 1)[0]
        self.assertIn(
            "enabled == m_hamEnhanceApplied && m_hamEnhanceState->isLayoutApplied(enabled)",
            update_layout,
        )

    def test_fixed_ham_heights_are_not_overwritten_by_saved_workspace_sizes(self) -> None:
        manager = HUD_MANAGER_CPP.read_text(encoding="utf-8")
        for owner in (
            "m_targetStatusPage",
            "m_secondaryTargetStatusPage",
            "m_playerStatusPage",
        ):
            self.assertIn(
                f"{owner}->setSettingsAutoSizeLocation(false, true)", manager
            )
        self.assertNotIn(
            "m_targetStatusPage->setSettingsAutoSizeLocation(true, true)", manager
        )
        self.assertNotIn(
            "m_secondaryTargetStatusPage->setSettingsAutoSizeLocation(true, true)",
            manager,
        )
        self.assertNotIn(
            "m_playerStatusPage->setSettingsAutoSizeLocation(true, true)", manager
        )


if __name__ == "__main__":
    unittest.main()
