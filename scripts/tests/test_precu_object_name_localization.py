from __future__ import annotations

import os
import re
import struct
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CLIENT_OBJECT = ROOT / (
    "src/engine/client/library/clientGame/src/shared/object/ClientObject.cpp"
)

WORKSPACE = ROOT.parents[1]
P12_OBJECT_NAMES = Path(
    os.environ.get(
        "PRECU_P12_OBJ_N",
        WORKSPACE / "Diagnostics/p14-patch12-full/string/en/obj_n.stf",
    )
)
P14_OBJECT_NAMES = Path(
    os.environ.get(
        "PRECU_P14_OBJ_N",
        WORKSPACE / "Diagnostics/p14-patch14-full/string/en/obj_n.stf",
    )
)

ALIASES = {
    "obj_bandfill_classic": "obj_bandfill",
    "obj_chidinkalu_horn_classic": "obj_chidinkalu_horn",
    "obj_fanfar_classic": "obj_fanfar",
    "obj_fizzz_classic": "obj_fizzz",
    "obj_kloo_horn_classic": "obj_kloo_horn",
    "obj_mandoviol_classic": "obj_mandoviol",
    "obj_nalargon_classic": "obj_nalargon",
    "obj_ommni_box_classic": "obj_ommni_box",
    "obj_slitherhorn_classic": "obj_slitherhorn",
    "obj_traz_classic": "obj_traz",
}


def read_stf(path: Path) -> dict[str, str]:
    data = path.read_bytes()
    magic, = struct.unpack_from("<I", data, 0)
    if magic != 0xABCD:
        raise AssertionError(f"unexpected STF magic in {path}: {magic:#x}")

    entry_count, = struct.unpack_from("<I", data, 9)
    offset = 13
    values: dict[int, str] = {}
    for _ in range(entry_count):
        string_id, _source_crc, character_count = struct.unpack_from(
            "<III", data, offset
        )
        offset += 12
        byte_count = character_count * 2
        values[string_id] = data[offset : offset + byte_count].decode("utf-16-le")
        offset += byte_count

    strings: dict[str, str] = {}
    for _ in range(entry_count):
        string_id, name_length = struct.unpack_from("<II", data, offset)
        offset += 8
        name = data[offset : offset + name_length].decode("latin-1")
        offset += name_length
        strings[name] = values[string_id]

    if offset != len(data):
        raise AssertionError(f"STF parser left {len(data) - offset} bytes in {path}")
    return strings


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


class Publish14ObjectNameLocalizationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = CLIENT_OBJECT.read_text(encoding="utf-8")
        cls.compatibility = function_body(
            cls.source, "StringId getPreCuCompatibleObjectNameStringId"
        )
        cls.update_name = function_body(
            cls.source, "void ClientObject::updateLocalizedName"
        )

    def test_later_instrument_keys_route_to_exact_publish14_keys(self) -> None:
        mapping_block = re.search(
            r"s_preCuObjectNameAliases\[\]\s*=\s*\{(?P<body>.*?)\n\s*\};",
            self.source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(mapping_block)
        actual = dict(
            re.findall(
                r'\{\s*"([^"]+)",\s*"([^"]+)"\s*\}',
                mapping_block.group("body"),
            )
        )
        self.assertEqual(ALIASES, actual)
        self.assertIn('source.getTable() == "obj_n"', self.compatibility)

    def test_object_localization_uses_the_compatible_id_for_both_locales(self) -> None:
        self.assertIn(
            "getPreCuCompatibleObjectNameStringId(m_nameStringId.get())",
            self.update_name,
        )
        self.assertEqual(3, self.update_name.count("compatibleNameStringId"))
        self.assertNotIn(
            "getLocalizedStringValue (m_nameStringId.get ()", self.update_name
        )

    def test_alias_targets_are_authentic_in_both_precu_tables(self) -> None:
        missing = [
            str(path)
            for path in (P12_OBJECT_NAMES, P14_OBJECT_NAMES)
            if not path.is_file()
        ]
        if missing:
            self.skipTest("authoritative PRE-CU STF fixture unavailable: " + ", ".join(missing))

        for path in (P12_OBJECT_NAMES, P14_OBJECT_NAMES):
            strings = read_stf(path)
            for later_key, publish14_key in ALIASES.items():
                self.assertNotIn(later_key, strings, f"{path}:{later_key}")
                self.assertIn(publish14_key, strings, f"{path}:{publish14_key}")


if __name__ == "__main__":
    unittest.main()
