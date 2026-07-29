from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
QUEUE_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiCombatQueue.cpp"
)
QUEUE_HEADER = QUEUE_CPP.with_suffix(".h")
COMMAND_QUEUE_CPP = ROOT / (
    "src/engine/client/library/clientGame/src/shared/command/"
    "ClientCommandQueue.cpp"
)
PLAYER_CONTROLLER_CPP = ROOT / (
    "src/engine/client/library/clientGame/src/shared/controller/"
    "PlayerCreatureController.cpp"
)
HUD_MANAGER_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiHudWindowManager.cpp"
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


class PrecuCombatQueueClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.queue_cpp = QUEUE_CPP.read_text(encoding="utf-8")
        cls.queue_header = QUEUE_HEADER.read_text(encoding="utf-8")
        cls.command_queue_cpp = COMMAND_QUEUE_CPP.read_text(encoding="utf-8")
        cls.player_controller_cpp = PLAYER_CONTROLLER_CPP.read_text(encoding="utf-8")
        cls.hud_manager_cpp = HUD_MANAGER_CPP.read_text(encoding="utf-8")

    def test_clear_is_combat_only_and_sequence_specific(self) -> None:
        clear_body = function_body(
            self.queue_cpp, "void SwgCuiCombatQueue::clearCombatQueue()"
        )
        self.assertIn("getCombatCommandsFromQueue(sequenceIds)", clear_body)
        self.assertIn("ClientCommandQueue::removeCommand(*it)", clear_body)
        self.assertNotIn("ClientCommandQueue::clear()", clear_body)

        remove_body = function_body(
            self.command_queue_cpp,
            "bool ClientCommandQueue::removeCommand(uint32 sequenceId)",
        )
        self.assertIn("sequenceId == 0", remove_body)
        self.assertIn("sendCommandQueueRemove(sequenceId)", remove_body)
        self.assertNotIn("handleCommandRemoved(sequenceId", remove_body)

        self.assertIn("case CM_commandQueueRemove:", self.player_controller_cpp)
        self.assertIn(
            "ClientCommandQueue::handleCommandRemoved(msg->getSequenceId(), msg->getWaitTime()",
            self.player_controller_cpp,
        )

        broad_clear = function_body(
            self.command_queue_cpp, "void ClientCommandQueue::clear()"
        )
        self.assertIn("sendCommandQueueRemove(0)", broad_clear)

    def test_queue_subscriptions_outlive_activation(self) -> None:
        constructor = function_body(
            self.queue_cpp, "SwgCuiCombatQueue::SwgCuiCombatQueue(UIPage & page)"
        )
        destructor = function_body(
            self.queue_cpp, "SwgCuiCombatQueue::~SwgCuiCombatQueue()"
        )
        activate = function_body(
            self.queue_cpp, "void SwgCuiCombatQueue::performActivate()"
        )
        deactivate = function_body(
            self.queue_cpp, "void SwgCuiCombatQueue::performDeactivate()"
        )

        for message in ("Added", "Removing", "StatesChanged"):
            self.assertIn(message, constructor)
            self.assertIn(message, destructor)
            self.assertNotIn(message, activate)
            self.assertNotIn(message, deactivate)

    def test_removal_copies_only_stable_payload_data(self) -> None:
        removal = function_body(
            self.queue_cpp, "void SwgCuiCombatQueue::onCommandRemoving("
        )
        self.assertIn("payload.sequenceId", removal)
        self.assertIn("payload.waitTime", removal)
        self.assertNotIn("payload.commandEntry", removal)
        self.assertNotIn("ClientCommandQueue::Entry *", self.queue_header)

    def test_hud_manager_directly_owns_queue(self) -> None:
        self.assertIn('hud.getCodeDataObject(TUIPage, mediatorPage, "CombatQueue")', self.hud_manager_cpp)
        self.assertIn("new SwgCuiCombatQueue(*mediatorPage)", self.hud_manager_cpp)
        self.assertIn("m_workspace->addMediator(*m_combatQueueMediator)", self.hud_manager_cpp)
        self.assertIn("m_workspace->removeMediator(*m_combatQueueMediator)", self.hud_manager_cpp)
        self.assertIn("m_combatQueueMediator->release()", self.hud_manager_cpp)

    def test_retail_bindings_and_actions_are_present(self) -> None:
        for binding in (
            "VolumePage",
            "SampleItem",
            "ClearButton",
            "PeaceAttackButton",
            "TargetNameText",
        ):
            self.assertIn(f'"{binding}"', self.queue_cpp)

        for action in (
            "combatQueueCollapse",
            "combatQueueExpand",
            "clearCombatQueue",
        ):
            self.assertIn(action, self.queue_cpp)

    def test_target_label_retries_unresolved_objects(self) -> None:
        constructor = function_body(
            self.queue_cpp, "SwgCuiCombatQueue::SwgCuiCombatQueue(UIPage & page)"
        )
        update_target = function_body(
            self.queue_cpp, "void SwgCuiCombatQueue::updateTarget()"
        )

        self.assertIn("m_targetNameText->Clear()", constructor)
        self.assertIn("target.isValid() && !object", update_target)
        self.assertIn(
            "m_lastCombatTarget = CachedNetworkId::cms_cachedInvalid",
            update_target,
        )

    def test_invisible_publish14_death_blow_gets_a_real_sequence(self) -> None:
        enqueue = function_body(
            self.command_queue_cpp,
            "uint32 ClientCommandQueue::enqueueCommand(Command const &command",
        )
        self.assertIn('Crc::normalizeAndCalculate("coupDeGrace")', enqueue)
        self.assertIn('Crc::normalizeAndCalculate("deathBlow")', enqueue)
        self.assertIn("command.m_addToCombatQueue", enqueue)
        self.assertIn(
            "command.m_visibleToClients || trackPrecuDeathBlow",
            enqueue,
        )
        self.assertIn("sequenceId = nextSequenceId()", enqueue)


if __name__ == "__main__":
    unittest.main()
