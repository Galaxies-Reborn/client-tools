from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
LAYER_RENDERER = ROOT / (
    "src/engine/client/library/clientUserInterface/src/shared/core/"
    "CuiLayerRenderer.cpp"
)


class PrecuReleaseUiDiagnosticTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = LAYER_RENDERER.read_text(encoding="utf-8")

    def test_flush_profiler_is_compiled_only_into_debug_clients(self) -> None:
        warning = self.source.index('"UiFlush:')
        debug_guard = self.source.rfind("#if defined(_DEBUG)", 0, warning)
        guard_end = self.source.index("#endif", warning)

        self.assertGreaterEqual(debug_guard, 0)
        self.assertLess(debug_guard, warning)
        self.assertLess(warning, guard_end)
        self.assertIn('"UiFlushShaders:', self.source[warning:guard_end])

    def test_release_helpers_do_not_collect_flush_profile_state(self) -> None:
        self.assertIn("inline void noteFlushShader (void const *)", self.source)
        self.assertIn("inline void noteFlushReason (bool, bool, bool)", self.source)


if __name__ == "__main__":
    unittest.main()
