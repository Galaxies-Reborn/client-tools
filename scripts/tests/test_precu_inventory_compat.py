from __future__ import annotations

import hashlib
import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
INVENTORY_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiInventoryEquipment.cpp"
)
INVENTORY_HEADER = INVENTORY_CPP.with_suffix(".h")

ASSET_ENVIRONMENT_VARIABLE = "PRECU_INVENTORY_ASSET"
ASSET_OVERRIDE = os.environ.get(ASSET_ENVIRONMENT_VARIABLE)
DEFAULT_INVENTORY_ASSET = ROOT.parents[1] / (
    "MCP/SWGEmu/extracted/precu-patch-12-00/ui/ui_pda_inventory.inc"
)
INVENTORY_ASSET = (
    Path(ASSET_OVERRIDE).expanduser()
    if ASSET_OVERRIDE
    else DEFAULT_INVENTORY_ASSET
)

EXPECTED_ASSET_SIZE = 52_884
EXPECTED_ASSET_SHA256 = (
    "3f6b6e34b65a4e3846551e8697679804e7f39e68e7e500b0e29964b7aff06aff"
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


class Publish14InventorySourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = INVENTORY_CPP.read_text(encoding="utf-8")
        cls.header = INVENTORY_HEADER.read_text(encoding="utf-8")
        cls.constructor = function_body(
            cls.source,
            "SwgCuiInventoryEquipment::SwgCuiInventoryEquipment",
        )
        cls.activate = function_body(
            cls.source, "void SwgCuiInventoryEquipment::performActivate"
        )
        cls.deactivate = function_body(
            cls.source, "void SwgCuiInventoryEquipment::performDeactivate"
        )
        cls.setup = function_body(
            cls.source, "void SwgCuiInventoryEquipment::setupCharacterViewer"
        )

    def test_constructor_accepts_both_inventory_viewer_generations(self) -> None:
        self.assertIn(
            "dynamic_cast<CuiWidget3dObjectListViewer *>(widget)",
            self.constructor,
        )
        self.assertIn(
            "dynamic_cast<CuiWidget3dPaperdoll *>(widget)",
            self.constructor,
        )
        self.assertNotIn(
            "NON_NULL (dynamic_cast<CuiWidget3dObjectListViewer *>(widget))",
            self.constructor,
        )
        self.assertIn("CuiWidget3dPaperdoll *", self.header)

    def test_registration_uses_the_resolved_base_widget(self) -> None:
        self.assertRegex(
            self.constructor,
            r"if\s*\(widget\)\s*registerMediatorObject\s*\(\*widget, true\)",
        )
        self.assertNotIn(
            "registerMediatorObject (*m_characterViewer, true)",
            self.constructor,
        )

    def test_lifecycle_guards_both_viewer_types(self) -> None:
        for body in (self.activate, self.deactivate):
            self.assertIn("if (m_characterViewer)", body)
            self.assertIn("else if (m_characterPaperdoll)", body)

    def test_publish14_paperdoll_receives_the_player_object(self) -> None:
        self.assertIn("if (m_characterViewer)", self.setup)
        self.assertIn("else if (m_characterPaperdoll)", self.setup)
        self.assertIn(
            "m_characterPaperdoll->setAutoComputeMinimumVectorFromExtent (true)",
            self.setup,
        )
        self.assertIn(
            "m_characterPaperdoll->setObject (object, Vector (), cameraMaximum)",
            self.setup,
        )


class Publish14InventoryAssetContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not INVENTORY_ASSET.is_file():
            message = (
                f"Publish 14 inventory asset not found at {INVENTORY_ASSET}. Set "
                f"{ASSET_ENVIRONMENT_VARIABLE} to its extracted path."
            )
            if ASSET_OVERRIDE:
                raise AssertionError(message)
            raise unittest.SkipTest(message)

        cls.asset_bytes = INVENTORY_ASSET.read_bytes()
        cls.asset = cls.asset_bytes.decode("utf-8")

    def test_asset_is_the_locked_publish14_inventory_layout(self) -> None:
        self.assertEqual(EXPECTED_ASSET_SIZE, len(self.asset_bytes))
        self.assertEqual(
            EXPECTED_ASSET_SHA256,
            hashlib.sha256(self.asset_bytes).hexdigest(),
        )

    def test_character_viewer_is_a_publish14_paperdoll(self) -> None:
        code_data = re.search(
            r"<Data\s+.*?characterviewer='([^']+)'.*?Name='CodeData'.*?/>",
            self.asset,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(code_data)
        self.assertEqual("inner.viewerpage.viewer", code_data.group(1))
        paperdoll = re.search(
            r"<CuiWidget3dPaperdoll\s+.*?Name='Viewer'.*?/>",
            self.asset,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(paperdoll)


if __name__ == "__main__":
    unittest.main()
