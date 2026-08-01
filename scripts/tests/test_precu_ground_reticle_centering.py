from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
CUI_IO_WIN = ROOT / (
    "src/engine/client/library/clientUserInterface/src/shared/core/CuiIoWin.cpp"
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


class PrecuGroundReticleCenteringTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = CUI_IO_WIN.read_text(encoding="utf-8")

    def test_ground_mouse_look_has_no_movable_dead_zone(self) -> None:
        reset = function_body(self.source, "void CuiIoWin::resetDeadZone ()")
        configured = function_body(self.source, "void CuiIoWin::setDeadZoneSize")

        for body in (reset, configured):
            self.assertIn("Game::isHudSceneTypeSpace()", body)
            self.assertIn("ms_reticleDeadZoneSizeUsable = 0;", body)
            self.assertIn("ms_minimumSpaceDeadZone", body)

    def test_ground_center_uses_logical_ui_canvas_coordinates(self) -> None:
        center = function_body(self.source, "void CuiIoWin::getScreenCenter (")
        self.assertIn("Graphics::getUiCanvasWidth() / 2", center)
        self.assertIn("Graphics::getUiCanvasHeight() / 2", center)


if __name__ == "__main__":
    unittest.main()
