from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
TOOLBAR_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiToolbar.cpp"
)
QUEUE_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiCombatQueue.cpp"
)
INPUT_SCHEME_CPP = ROOT / (
    "src/engine/client/library/clientGame/src/shared/core/InputScheme.cpp"
)
CLIENT_QUEUE_CPP = ROOT / (
    "src/engine/client/library/clientGame/src/shared/command/ClientCommandQueue.cpp"
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


class PrecuHotbarQueueExecutionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.toolbar = TOOLBAR_CPP.read_text(encoding="utf-8")
        cls.queue = QUEUE_CPP.read_text(encoding="utf-8")
        cls.input_scheme = INPUT_SCHEME_CPP.read_text(encoding="utf-8")
        cls.client_queue = CLIENT_QUEUE_CPP.read_text(encoding="utf-8")

    def test_explicit_slot_activation_fires_even_with_publish14_flags(self) -> None:
        set_default = function_body(
            self.toolbar, "void SwgCuiToolbar::setDefaultAction("
        )
        reset_scheme = function_body(
            self.input_scheme, "bool InputScheme::resetFromType ("
        )

        self.assertIn("if(activateActionIfNeeded)", set_default)
        self.assertIn("AT_secondaryAttackFromToolbar", set_default)
        self.assertNotIn("getCanFireSecondariesFromToolbar", set_default)
        self.assertIn("setDefaultAction(slot, false)", self.toolbar)
        self.assertIn("F_canFireSecondariesFromToolbar", reset_scheme)

    def test_queue_template_is_linked_before_real_presentation(self) -> None:
        add_command = function_body(
            self.queue, "void SwgCuiCombatQueue::addCommand("
        )
        link = add_command.index("commandPage->Link()")
        self.assertLess(link, add_command.index("icon->SetStyle(imageStyle)"))
        self.assertLess(link, add_command.index("nameText->SetLocalText(localizedCommandName)"))
        self.assertIn("Unicode::narrowToWide(entry.m_command->m_commandName)", add_command)

    def test_normal_combat_commands_are_not_limited_by_the_spam_cutoff(self) -> None:
        enqueue = function_body(
            self.client_queue,
            "uint32 ClientCommandQueue::enqueueCommand(Command const &command",
        )
        self.assertIn("command.m_defaultPriority == Command::CP_Immediate", enqueue)
        self.assertIn("ms_cutoffTime += 1/MAX_QUEUED_COMMANDS", enqueue)
        self.assertNotIn("ms_commandQueue.size() >=", enqueue)


if __name__ == "__main__":
    unittest.main()
