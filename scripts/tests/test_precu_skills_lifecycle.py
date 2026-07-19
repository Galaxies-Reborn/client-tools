from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
SKILLS_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiSkills.cpp"
)
SKILLS_HEADER = SKILLS_CPP.with_suffix(".h")
FACTORY_SETUP_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/core/"
    "SwgCuiMediatorFactorySetup.cpp"
)
CONFIRMATION_CPP = ROOT / (
    "src/engine/client/library/clientUserInterface/src/shared/page/"
    "CuiDeleteSkillConfirmation.cpp"
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


class PrecuSkillsLifecycleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.skills_cpp = SKILLS_CPP.read_text(encoding="utf-8")
        cls.skills_header = SKILLS_HEADER.read_text(encoding="utf-8")
        cls.factory_setup_cpp = FACTORY_SETUP_CPP.read_text(encoding="utf-8")
        cls.confirmation_cpp = CONFIRMATION_CPP.read_text(encoding="utf-8")

    def test_live_refresh_subscriptions_are_activation_scoped(self) -> None:
        activate = function_body(
            self.skills_cpp, "void SwgCuiSkills::performActivate()"
        )
        deactivate = function_body(
            self.skills_cpp, "void SwgCuiSkills::performDeactivate()"
        )

        for message in (
            "CreatureObject::Messages::SkillsChanged",
            "PlayerObject::Messages::ExperienceChanged",
            "CreatureObject::Messages::SkillModsChanged",
        ):
            self.assertIn(message, activate)
            self.assertIn(message, deactivate)

        constructor = function_body(
            self.skills_cpp, "SwgCuiSkills::SwgCuiSkills(UIPage & page)"
        )
        destructor = function_body(
            self.skills_cpp, "SwgCuiSkills::~SwgCuiSkills()"
        )
        self.assertIn("ClientCommandQueue::Messages::Removing", constructor)
        self.assertIn("ClientCommandQueue::Messages::Removing", destructor)
        self.assertIn("Game::Messages::SceneChanged", constructor)
        self.assertIn("Game::Messages::SceneChanged", destructor)
        self.assertIn(
            "CuiDeleteSkillConfirmation::Message::DeleteSkillConfirmation",
            constructor,
        )
        self.assertIn(
            "CuiDeleteSkillConfirmation::Message::DeleteSkillConfirmation",
            destructor,
        )
        self.assertNotIn(
            "CuiDeleteSkillConfirmation::Message::DeleteSkillConfirmation",
            deactivate,
        )
        self.assertNotIn("m_confirmationSkill.clear()", deactivate)

    def test_refresh_handlers_filter_player_payloads_and_update_exact_views(self) -> None:
        skills = function_body(
            self.skills_cpp, "void SwgCuiSkills::onSkillsChanged("
        )
        self.assertIn("&creature != Game::getPlayerCreature()", skills)
        for refresh in (
            "populateProfessionList()",
            "populateSelectedProfession()",
            "updateSkillPointsDisplay()",
        ):
            self.assertIn(refresh, skills)

        experience = function_body(
            self.skills_cpp, "void SwgCuiSkills::onExperienceChanged("
        )
        self.assertIn("&player != Game::getConstPlayerObject()", experience)
        self.assertIn("populateExperience()", experience)
        self.assertIn("populateSelectedProfession()", experience)

        skill_mods = function_body(
            self.skills_cpp, "void SwgCuiSkills::onSkillModsChanged("
        )
        self.assertIn("&creature == Game::getPlayerCreature()", skill_mods)
        self.assertIn("populateSkillMods()", skill_mods)

    def test_live_rebuild_deduplicates_widgets_and_preserves_selected_box(self) -> None:
        self.assertGreaterEqual(
            self.skills_cpp.count("if (!isRegisteredMediatorObject("), 3
        )
        graph = function_body(
            self.skills_cpp, "bool SwgCuiSkills::tryPopulateGraph4x4("
        )
        self.assertIn("selectionStillVisible", graph)
        self.assertIn("it->second == m_selectedSkill", graph)
        self.assertIn("if (!selectionStillVisible)", graph)
        profession_list = function_body(
            self.skills_cpp, "void SwgCuiSkills::populateProfessionList()"
        )
        self.assertIn("synchronizeProfessionTreeSelection()", profession_list)
        synchronize = function_body(
            self.skills_cpp,
            "void SwgCuiSkills::synchronizeProfessionTreeSelection()",
        )
        self.assertIn("GetDataSourceContainerAtRow(row)", synchronize)
        self.assertIn("m_treeProf->SelectRow(selectedRow)", synchronize)
        fallback = function_body(
            self.skills_cpp, "void SwgCuiSkills::populateSelectedProfession()"
        )
        fallback = fallback[fallback.index("// Non-4x4 fallback") :]
        self.assertIn("m_selectedSkill.clear()", fallback)
        self.assertIn("populateSelectedSkill()", fallback)

    def test_surrender_click_validates_and_never_enqueues_directly(self) -> None:
        button = function_body(
            self.skills_cpp, "void SwgCuiSkills::OnButtonPressed("
        )
        self.assertIn("findOwnedSkill(*player, m_selectedSkill)", button)
        self.assertIn("findLearnedDependentSkills", button)
        self.assertIn("showSurrenderDependencies", button)
        self.assertIn("m_confirmationSkill = m_selectedSkill", button)
        self.assertIn("CuiMediatorTypes::DeleteSkillConfirmation", button)
        self.assertIn("setSelectedSkill(m_confirmationSkill)", button)
        self.assertNotIn("enqueueCommand", button)

    def test_confirmation_revalidates_and_enqueues_exactly_once(self) -> None:
        confirmation = function_body(
            self.skills_cpp, "void SwgCuiSkills::onDeleteSkillConfirmation("
        )
        self.assertIn("skillName != m_confirmationSkill", confirmation)
        self.assertIn("clearConfirmationSnapshot()", confirmation)
        self.assertIn("player->getNetworkId() == confirmationPlayerId", confirmation)
        self.assertIn("findOwnedSkill(*player, skillName)", confirmation)
        self.assertIn("findLearnedDependentSkills", confirmation)
        self.assertIn('Crc::normalizeAndCalculate("surrenderSkill")', confirmation)
        self.assertIn("surrenderCommand.isNull()", confirmation)
        self.assertIn("!surrenderCommand.m_visibleToClients", confirmation)
        self.assertIn("m_pendingSurrenderSkill = skillName", confirmation)
        self.assertIn("m_pendingSurrenderPlayerId = player->getNetworkId()", confirmation)
        self.assertEqual(confirmation.count("ClientCommandQueue::enqueueCommand("), 1)
        self.assertIn("surrenderCommand, NetworkId::cms_invalid", confirmation)
        self.assertNotIn("surrenderCommand, player->getNetworkId()", confirmation)
        self.assertIn("targetType=none", confirmation)
        self.assertIn("Unicode::narrowToWide(skillName)", confirmation)
        self.assertLess(
            confirmation.index("surrenderCommand.isNull()"),
            confirmation.index("m_pendingSurrenderSkill = skillName"),
        )
        self.assertLess(
            confirmation.index("m_pendingSurrenderSkill = skillName"),
            confirmation.index("ClientCommandQueue::enqueueCommand("),
        )
        self.assertIn("m_surrenderSequenceId == 0", confirmation)

    def test_pending_guard_reconciles_completion_delta_queue_and_scene(self) -> None:
        removing = function_body(
            self.skills_cpp, "void SwgCuiSkills::onCommandRemoving("
        )
        self.assertIn("payload.sequenceId != m_surrenderSequenceId", removing)
        self.assertIn("payload.status != Command::CEC_Success", removing)
        self.assertIn("clearPendingSurrender()", removing)

        scene = function_body(
            self.skills_cpp, "void SwgCuiSkills::onSceneChanged("
        )
        self.assertIn("clearConfirmationSnapshot()", scene)
        self.assertIn("clearPendingSurrender()", scene)
        self.assertIn("CuiMediatorTypes::DeleteSkillConfirmation", scene)

        skills = function_body(
            self.skills_cpp, "void SwgCuiSkills::onSkillsChanged("
        )
        self.assertIn("reconcilePendingSurrender()", skills)
        reconcile = function_body(
            self.skills_cpp, "void SwgCuiSkills::reconcilePendingSurrender()"
        )
        self.assertIn("player->getNetworkId() != m_pendingSurrenderPlayerId", reconcile)
        self.assertIn("ClientCommandQueue::findEntry(m_surrenderSequenceId) == 0", reconcile)
        self.assertIn("clearPendingSurrender()", reconcile)
        update = function_body(self.skills_cpp, "void SwgCuiSkills::update(")
        self.assertIn("reconcilePendingSurrender()", update)
        button_state = function_body(
            self.skills_cpp, "void SwgCuiSkills::updateSurrenderButton()"
        )
        self.assertIn("m_pendingSurrenderSkill.empty()", button_state)

    def test_retained_typed_confirmation_contract_is_registered(self) -> None:
        self.assertIn(
            "CuiMediatorTypes::DeleteSkillConfirmation", self.factory_setup_cpp
        )
        self.assertIn('new CuiMediatorFactory::Constructor <CuiDeleteSkillConfirmation>("/confirm")', self.factory_setup_cpp)
        for key in (
            "skill_delete_you_requested",
            "skill_delete_skill",
            "skill_delete_confirm",
        ):
            self.assertIn(key, self.confirmation_cpp)
        self.assertIn("s_deleteSkillConfirmation.emitMessage", self.confirmation_cpp)


if __name__ == "__main__":
    unittest.main()
