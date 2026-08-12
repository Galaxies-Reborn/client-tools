from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
HUD_SOURCE = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHud.cpp"
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


class PrecuGroundReticleCursorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = HUD_SOURCE.read_text(encoding="utf-8")
        cls.update = function_body(cls.source, "void SwgCuiHud::update (")

    def test_mouse_look_starts_each_frame_with_publish14_default_reticle(self) -> None:
        default_cursor = "UICursor * theCursor = m_reticleDefault;"
        contextual_cursor = (
            "Cui::MenuInfoTypes::findDefaultCursor (*clientObject)"
        )

        self.assertIn(default_cursor, self.update)
        self.assertIn(contextual_cursor, self.update)
        self.assertLess(
            self.update.index(default_cursor),
            self.update.index(contextual_cursor),
        )

    def test_later_intended_attack_cursor_is_not_used_for_ground_reticle(self) -> None:
        self.assertNotIn("getIntendedAttackCursor", self.update)
        self.assertNotIn("getIntendedAttackInactiveCursor", self.update)

    def test_contextual_cursor_only_replaces_default_when_resolved(self) -> None:
        contextual_cursor = self.update.index(
            "Cui::MenuInfoTypes::findDefaultCursor (*clientObject)"
        )
        resolved_guard = self.update.index("if (tmpCursor)", contextual_cursor)
        assignment = self.update.index("theCursor = tmpCursor;", resolved_guard)

        self.assertLess(contextual_cursor, resolved_guard)
        self.assertLess(resolved_guard, assignment)


if __name__ == "__main__":
    unittest.main()
