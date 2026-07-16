from __future__ import annotations

import hashlib
import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
INVENTORY_INFO_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiInventoryInfo.cpp"
)

ASSET_ENVIRONMENT_VARIABLE = "PRECU_EXAMINE_ASSET"
ASSET_OVERRIDE = os.environ.get(ASSET_ENVIRONMENT_VARIABLE)
DEFAULT_EXAMINE_ASSET = ROOT.parents[1] / (
    "MCP/SWGEmu/extracted/precu-patch-12-00/ui/ui_pda_examine.inc"
)
EXAMINE_ASSET = (
    Path(ASSET_OVERRIDE).expanduser() if ASSET_OVERRIDE else DEFAULT_EXAMINE_ASSET
)

EXPECTED_ASSET_SIZE = 13_999
EXPECTED_ASSET_SHA256 = (
    "17f9ae411281c17e2442336e29c30b47ed8e35ffb5f25cdae59e31050d2716e1"
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


class Publish14ExamineSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = INVENTORY_INFO_CPP.read_text(encoding="utf-8")
        cls.constructor = function_body(
            cls.source, "SwgCuiInventoryInfo::SwgCuiInventoryInfo"
        )

    def test_later_appearance_checkbox_is_optional(self) -> None:
        self.assertEqual(
            1,
            self.constructor.count(
                'getCodeDataObject (TUICheckbox,     m_hideAppearanceItems, '
                '"checkHideAppearance", true);'
            ),
        )

    def test_optional_checkbox_is_guarded_before_registration(self) -> None:
        binding = self.constructor.index('"checkHideAppearance", true)')
        registration = self.constructor.index(
            "registerMediatorObject(*m_hideAppearanceItems, true);", binding
        )
        between = self.constructor[binding:registration]
        self.assertRegex(
            between,
            r"if\s*\(\s*m_hideAppearanceItems\s*\)",
        )


class Publish14ExamineAssetContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not EXAMINE_ASSET.is_file():
            message = (
                f"Publish 14 Examine asset not found at {EXAMINE_ASSET}. Set "
                f"{ASSET_ENVIRONMENT_VARIABLE} to its extracted path."
            )
            if ASSET_OVERRIDE:
                raise AssertionError(message)
            raise unittest.SkipTest(message)

        cls.asset_bytes = EXAMINE_ASSET.read_bytes()
        cls.asset = cls.asset_bytes.decode("utf-8")

    def test_asset_is_the_locked_patch12_winner(self) -> None:
        self.assertEqual(EXPECTED_ASSET_SIZE, len(self.asset_bytes))
        self.assertEqual(
            EXPECTED_ASSET_SHA256,
            hashlib.sha256(self.asset_bytes).hexdigest(),
        )

    def test_publish14_info_codedata_has_no_later_checkbox(self) -> None:
        info_page = re.search(
            r"<Page\s+.*?Name='info'.*?</Page>", self.asset, flags=re.DOTALL
        )
        self.assertIsNotNone(info_page)
        self.assertNotIn("checkHideAppearance", info_page.group(0))
        for required in ("content=", "Label=", "textAttribs=", "textDesc=", "viewer="):
            with self.subTest(required=required):
                self.assertIn(required, info_page.group(0))


if __name__ == "__main__":
    unittest.main()
