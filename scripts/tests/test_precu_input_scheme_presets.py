from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
INPUT_SCHEME_CPP = ROOT / (
    "src/engine/client/library/clientGame/src/shared/core/InputScheme.cpp"
)

EXPECTED_GROUND_PRESETS = {
    "fps": (
        "input/groundinputmap_fps.iff",
        {"F_modalChat", "F_chaseCam"},
    ),
    "iso": (
        "input/groundinputmap_iso.iff",
        {"F_mouseMode"},
    ),
    "ja101": (
        "input/groundinputmap_ja101.iff",
        set(),
    ),
    "mmo": (
        "input/groundinputmap_mmorpg.iff",
        {"F_modalChat", "F_mouseMode", "F_chaseCam", "F_turnStrafes"},
    ),
    "mmo2": (
        "input/groundinputmap_mmorpg2.iff",
        {
            "F_modalChat",
            "F_mouseMode",
            "F_chaseCam",
            "F_turnStrafes",
            "F_modeless",
        },
    ),
    "swg": (
        "input/groundinputmap_swg.iff",
        set(),
    ),
    "swg2": (
        "input/groundinputmap_swg2.iff",
        {"F_mouseMode", "F_chaseCam", "F_modeless", "F_swgMouseMap"},
    ),
}

EXPECTED_LABELS = {
    "fps": "First Person Shooter",
    "iso": "Isometric (UO Style)",
    "ja101": "Japanese w/English(101-key) Keyboard",
    "mmo": "MMORPG (EQ style)",
    "mmo2": "MMORPG Modeless",
    "swg": "Star Wars Galaxies",
    "swg2": "Star Wars Galaxies Modeless",
}


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


class Publish14InputSchemePresetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = INPUT_SCHEME_CPP.read_text(encoding="utf-8")
        cls.install = function_body(cls.source, "void install ()")
        cls.fetch = function_body(cls.source, "InputMap * InputScheme::fetchGroundInputMap")

    def test_registry_is_the_exact_publish14_ground_preset_set(self) -> None:
        declarations = {}
        for match in re.finditer(
            r"const Data\s+(?P<variable>\w+)\s*\("
            r"Game::ST_ground\s*,\s*\"(?P<path>[^\"]+)\"\s*,"
            r"(?P<flags>.*?)\);",
            self.install,
            flags=re.DOTALL,
        ):
            declarations[match.group("variable")] = (
                match.group("path"),
                set(re.findall(r"InputScheme::(F_\w+)", match.group("flags"))),
            )

        registrations = dict(
            re.findall(
                r's_dataMap\[Game::ST_ground\]\.insert\s*\('
                r'DataMap::value_type\s*\(\"([^\"]+)\"\s*,\s*(\w+)\)\);',
                self.install,
            )
        )

        actual = {
            preset: declarations[variable]
            for preset, variable in registrations.items()
        }
        self.assertEqual(EXPECTED_GROUND_PRESETS, actual)
        self.assertEqual(
            ["fps", "iso", "ja101", "mmo", "mmo2", "swg", "swg2"],
            sorted(actual),
        )

    def test_registry_identifiers_have_the_publish14_ui_labels(self) -> None:
        self.assertEqual(set(EXPECTED_GROUND_PRESETS), set(EXPECTED_LABELS))
        self.assertIn(
            'StringId ("ui", std::string ("inputscheme_") + type + "_n")',
            self.source,
        )

    def test_default_and_saved_later_presets_use_the_publish14_fallback(self) -> None:
        self.assertIn(
            'const std::string s_defaultGroundInputSchemeType = "swg";',
            self.source,
        )
        self.assertRegex(
            self.source,
            r"std::string\s+s_lastInputSchemeTypes\[Game::ST_numTypes\]\s*=\s*"
            r"\{\s*\"\"\s*,\s*s_defaultSpaceInputSchemeType\s*\};",
        )
        self.assertIn(
            "bool const needsReset = !data;",
            self.fetch,
        )
        self.assertIn(
            "data = NON_NULL (findData (s_defaultGroundInputSchemeType));",
            self.fetch,
        )
        for later_identifier in ("swg3", "minor1", "minor2"):
            self.assertNotIn(later_identifier, self.source)


if __name__ == "__main__":
    unittest.main()
