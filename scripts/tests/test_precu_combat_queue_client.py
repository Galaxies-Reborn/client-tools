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
GROUND_HUD_MANAGER_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiHudWindowManagerGround.cpp"
)
TARGETS_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiTargets.cpp"
)
STATUS_GROUND_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiStatusGround.cpp"
)
HUD_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHud.cpp"
)
RADIAL_CPP = ROOT / (
    "src/engine/client/library/clientUserInterface/src/shared/core/"
    "CuiRadialMenuManager.cpp"
)
GROUND_COMBAT_CPP = ROOT / (
    "src/engine/client/library/clientGame/src/shared/combat/"
    "GroundCombatActionManager.cpp"
)
COMMAND_CHECKS_CPP = ROOT / (
    "src/engine/client/library/clientGame/src/shared/command/"
    "ClientCommandChecks.cpp"
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
        cls.ground_hud_manager_cpp = GROUND_HUD_MANAGER_CPP.read_text(
            encoding="utf-8"
        )
        cls.targets_cpp = TARGETS_CPP.read_text(encoding="utf-8")
        cls.status_ground_cpp = STATUS_GROUND_CPP.read_text(encoding="utf-8")
        cls.hud_cpp = HUD_CPP.read_text(encoding="utf-8")
        cls.radial_cpp = RADIAL_CPP.read_text(encoding="utf-8")
        cls.ground_combat_cpp = GROUND_COMBAT_CPP.read_text(encoding="utf-8")
        cls.command_checks_cpp = COMMAND_CHECKS_CPP.read_text(encoding="utf-8")

    def test_clear_uses_the_authoritative_all_combat_sentinel(self) -> None:
        clear_body = function_body(
            self.queue_cpp, "void SwgCuiCombatQueue::clearCombatQueue()"
        )
        self.assertIn("ClientCommandQueue::clear()", clear_body)
        self.assertNotIn("getCombatCommandsFromQueue(sequenceIds)", clear_body)

        clear_queue = function_body(
            self.command_queue_cpp, "void ClientCommandQueue::clear()"
        )
        self.assertIn("sendCommandQueueRemove(0)", clear_queue)
        self.assertIn("ms_commandQueue.clear()", clear_queue)
        self.assertNotIn("ms_commandQueue.insert(firstValue)", clear_queue)

    def test_peace_button_queues_the_immediate_server_command(self) -> None:
        pressed = function_body(
            self.queue_cpp, "void SwgCuiCombatQueue::OnButtonPressed("
        )
        self.assertIn('enqueueCommand("peace"', pressed)

    def test_publish14_target_and_target_of_target_pages_are_both_owned(self) -> None:
        constructor = function_body(
            self.ground_hud_manager_cpp,
            "SwgCuiHudWindowManagerGround::SwgCuiHudWindowManagerGround",
        )
        update = function_body(self.targets_cpp, "void SwgCuiTargets::update (")

        self.assertIn('"TargetsPage"', constructor)
        self.assertIn('"SecondaryTargetsPage"', constructor)
        self.assertIn("primaryTargetPage->DuplicateObject()", constructor)
        self.assertIn('SetName("SecondaryTarget")', constructor)
        self.assertIn("new SwgCuiTargets(*mediatorPage, SwgCuiTargets::TR_primary)", constructor)
        self.assertIn(
            "new SwgCuiTargets(*mediatorPage, SwgCuiTargets::TR_targetOfTarget)",
            constructor,
        )
        self.assertIn('getCodeDataObject (TUIPage, statusPage, "pagestatus")', self.targets_cpp)
        self.assertIn("groundStatus->setLookAtTarget(true)", self.targets_cpp)
        self.assertIn(
            "intendedTarget.isValid() ? intendedTarget : lookAtTarget", update
        )
        self.assertIn("m_targetRole == TR_targetOfTarget", update)
        self.assertIn("target = primaryTargetObject->getIntendedTarget()", update)
        self.assertIn("target = primaryTargetObject->getLookAtTarget()", update)
        self.assertIn("getPage ().SetVisible (true)", update)
        self.assertIn("getPage ().SetVisible (false)", update)

    def test_target_creatures_keep_the_three_pool_ham_presentation(self) -> None:
        update_ham = function_body(
            self.status_ground_cpp,
            "bool SwgCuiStatusGround::updateTargetHam(CreatureObject const & creature",
        )
        self.assertIn("if(m_isLookAtTarget ||", update_ham)
        self.assertIn("pageStyle = PS_ham", update_ham)

    def test_radial_attack_and_double_click_share_the_basic_attack_path(self) -> None:
        combat_attack = function_body(
            self.radial_cpp,
            "bool CuiRadialMenuManager::performCombatAttack(Object const & object)",
        )
        menu_action = function_body(
            self.radial_cpp, "void CuiRadialMenuManager::performMenuAction ("
        )
        double_click = function_body(
            self.radial_cpp,
            "bool CuiRadialMenuManager::performDefaultDoubleClickAction(",
        )
        hud_message = function_body(
            self.hud_cpp, "bool SwgCuiHud::OnMessage("
        )
        modeless_double_click = hud_message[
            hud_message.index(
                "if (msg.Type == UIMessage::LeftMouseDoubleClick &&"
            ) : hud_message.index("if (msg.Type == UIMessage::MouseWheel)")
        ]

        self.assertIn("tangible->isAttackable()", combat_attack)
        self.assertIn("CuiPreferences::setAutoAimToggle(true)", combat_attack)
        self.assertIn("player->setLookAtAndIntendedTarget(targetId)", combat_attack)
        self.assertIn("commandsAreNowFromToolbar(true)", combat_attack)
        self.assertIn("ClientCommandQueue::enqueueCommand(", combat_attack)
        self.assertIn("player->getCurrentPrimaryActionName()", combat_attack)
        self.assertIn("commandsAreNowFromToolbar(false)", combat_attack)
        self.assertIn("sequenceId == 0", combat_attack)
        self.assertNotIn("AT_primaryAttack", combat_attack)
        self.assertIn("AT_toggleRepeatPrimaryAttack", combat_attack)
        self.assertLess(
            combat_attack.index("ClientCommandQueue::enqueueCommand("),
            combat_attack.index("AT_toggleRepeatPrimaryAttack"),
        )
        self.assertIn("performCombatAttack(*object)", menu_action)
        self.assertIn("performCombatAttack(object)", double_click)
        self.assertIn(
            "CuiPreferences::getUseModelessInterface()", modeless_double_click
        )
        self.assertIn(
            "CuiRadialMenuManager::performDefaultAction(*selectedObject)",
            modeless_double_click,
        )
        self.assertNotIn(
            "CuiRadialMenuManager::performDefaultDoubleClickAction(",
            modeless_double_click,
        )
        self.assertNotIn(
            "findDefaultAction(*m_lastSelectedObject.getPointer())", hud_message
        )
        self.assertNotIn(
            "LeftMouseDoubleClick && CuiPreferences::getAutoAimToggle()",
            hud_message,
        )

    def test_bare_hands_are_treated_as_the_implicit_unarmed_weapon(self) -> None:
        weapon_check = function_body(
            self.command_checks_cpp,
            "bool ClientCommandChecks::doesWeaponInvalidateCommand(",
        )
        update = function_body(
            self.ground_combat_cpp,
            "void GroundCombatActionManager::update(",
        )

        self.assertIn("WeaponObject::WT_unarmed", weapon_check)
        self.assertIn("implicitUnarmedWeapon", update)
        self.assertIn("canUsePrimaryActionWithoutWeapon", update)
        self.assertIn("std::max(0.1f, commandSpacingTime)", update)

    def test_inventory_default_and_radial_actions_transfer_items_directly(self) -> None:
        menu_action = function_body(
            self.radial_cpp, "void CuiRadialMenuManager::performMenuAction ("
        )

        self.assertIn("sel == ITEM_EQUIP", menu_action)
        self.assertIn("CuiInventoryManager::equipObject", menu_action)
        self.assertIn("sel == ITEM_UNEQUIP", menu_action)
        self.assertIn("CuiInventoryManager::unequipObject", menu_action)
        self.assertIn("sel == ITEM_EQUIP_APPEARANCE", menu_action)
        self.assertIn("CuiInventoryManager::equipAppearanceItem", menu_action)

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
