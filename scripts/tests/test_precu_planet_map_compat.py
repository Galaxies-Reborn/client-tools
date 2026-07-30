from __future__ import annotations

import hashlib
import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PLANET_MAP_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiPlanetMap.cpp"
)

ASSET_ENVIRONMENT_VARIABLE = "PRECU_PLANET_MAP_ASSET"
ASSET_OVERRIDE = os.environ.get(ASSET_ENVIRONMENT_VARIABLE)
DEFAULT_PLANET_MAP_ASSET = ROOT.parents[1] / (
    "MCP/SWGEmu/extracted/precu-patch-12-00/ui/ui_planet_map.inc"
)
PLANET_MAP_ASSET = (
    Path(ASSET_OVERRIDE).expanduser()
    if ASSET_OVERRIDE
    else DEFAULT_PLANET_MAP_ASSET
)

EXPECTED_ASSET_SIZE = 46_924
EXPECTED_ASSET_SHA256 = (
    "eaf38e54ee2e2b312897dc838404384386de9cc66f68aa351d1e1c82a4c9dc37"
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


class Publish14PlanetMapSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = PLANET_MAP_CPP.read_text(encoding="utf-8")
        cls.constructor = function_body(
            cls.source, "SwgCuiPlanetMap::SwgCuiPlanetMap"
        )
        cls.setup_current_zone = function_body(
            cls.source, "void SwgCuiPlanetMap::setupCurrentZone"
        )

    def test_later_zoom_button_is_optional(self) -> None:
        self.assertIn(
            'getCodeDataObject (TUIButton,         m_buttonZoom,            '
            '"buttonZoom",            true)',
            self.constructor,
        )
        self.assertRegex(
            self.setup_current_zone,
            r"if\s*\(m_buttonZoom\s*&&\s*"
            r"PlanetMapManager::sceneHasSupermap\(zoneName\)\)",
        )
        self.assertIn("else if(m_buttonZoom)", self.setup_current_zone)
        self.assertIn(
            "else if(m_zoomLevel == ZL_Planet && m_buttonZoom)",
            self.setup_current_zone,
        )


class Publish14PlanetMapAssetContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not PLANET_MAP_ASSET.is_file():
            message = (
                f"Publish 14 planet-map asset not found at {PLANET_MAP_ASSET}. Set "
                f"{ASSET_ENVIRONMENT_VARIABLE} to its extracted path."
            )
            if ASSET_OVERRIDE:
                raise AssertionError(message)
            raise unittest.SkipTest(message)

        cls.asset_bytes = PLANET_MAP_ASSET.read_bytes()
        cls.asset = cls.asset_bytes.decode("utf-8")

    def test_asset_is_the_locked_publish14_planet_map_layout(self) -> None:
        self.assertEqual(EXPECTED_ASSET_SIZE, len(self.asset_bytes))
        self.assertEqual(
            EXPECTED_ASSET_SHA256,
            hashlib.sha256(self.asset_bytes).hexdigest(),
        )

    def test_publish14_contract_has_map_but_no_zone_zoom_button(self) -> None:
        code_data = re.search(
            r"<Data\s+(.*?Name='CodeData'.*?)/>",
            self.asset,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(code_data)
        properties = code_data.group(1)
        self.assertIn("pageMaps='comp.left.planets.maps'", properties)
        self.assertIn("sliderZoom='comp.left.slider.slider'", properties)
        self.assertIn("buttonRefresh='comp.left.buttonRefresh'", properties)
        self.assertNotRegex(properties, r"(?i)buttonzoom=")


if __name__ == "__main__":
    unittest.main()
