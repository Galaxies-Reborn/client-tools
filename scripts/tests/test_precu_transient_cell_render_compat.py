import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
GROUND_SCENE_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp"
)


class Publish14TransientCellRenderCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = GROUND_SCENE_SOURCE.read_text(encoding="utf-8")
        match = re.search(
            r"CellProperty const \* playerCell = getPlayer\(\)->getParentCell\(\);"
            r".*?WorldSnapshot::update\(playerCell, playerPosition\);",
            cls.source,
            flags=re.DOTALL,
        )
        if match is None:
            raise AssertionError("GroundScene player-cell snapshot block was not found")
        cls.snapshot_block = match.group(0)

    def test_unbound_interior_uses_world_snapshot_temporarily(self):
        self.assertIn(
            "if (playerCell->getPortalProperty() == NULL)",
            self.snapshot_block,
        )
        self.assertIn(
            "playerCell = CellProperty::getWorldCellProperty();",
            self.snapshot_block,
        )

    def test_fallback_preserves_authoritative_player_containment(self):
        self.assertNotIn("getPlayer()->setParentCell", self.snapshot_block)
        self.assertIn(
            "without changing containment",
            self.snapshot_block,
        )


if __name__ == "__main__":
    unittest.main()
