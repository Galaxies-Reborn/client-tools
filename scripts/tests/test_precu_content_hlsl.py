import csv
import hashlib
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SHADER_ROOT = ROOT / "scripts" / "asm2hlsl"
MANIFEST = SHADER_ROOT / "precu-content-hlsl.tsv"
CONVERTED = SHADER_ROOT / "converted"


class PrecuContentHlslTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with MANIFEST.open(encoding="utf-8", newline="") as handle:
            header = handle.readline().removeprefix("# ").rstrip("\r\n").split("\t")
            cls.rows = list(csv.DictReader(handle, fieldnames=header, delimiter="\t"))

    def test_manifest_is_the_bounded_nge_content_shader_surface(self):
        self.assertEqual(len(self.rows), 29)
        self.assertEqual(len({row["path"] for row in self.rows}), 29)
        self.assertTrue(all("\r" not in row["source_tre"] for row in self.rows))
        self.assertTrue(
            all(
                row["path"].startswith(("pixel_program/", "vertex_program/"))
                for row in self.rows
            )
        )

    def test_tracked_payloads_match_clean_source_hashes(self):
        for row in self.rows:
            payload = (CONVERTED / Path(*row["path"].split("/"))).read_bytes()
            self.assertEqual(len(payload), int(row["output_size"]), row["path"])
            self.assertEqual(
                hashlib.sha256(payload).hexdigest().upper(),
                row["output_sha256"],
                row["path"],
            )

    def test_every_payload_is_dx11_compilable_hlsl_source(self):
        for row in self.rows:
            payload = (CONVERTED / Path(*row["path"].split("/"))).read_bytes()
            if row["path"].startswith("vertex_program/"):
                self.assertTrue(payload.lstrip().startswith(b"//hlsl"), row["path"])
                self.assertNotRegex(payload, rb":\s*register\(v\d+\)")
                self.assertIn(
                    b'#include "vertex_program/include/asm_constants.inc"',
                    payload,
                )
                self.assertNotIn(b"vertex_shader_constants.inc", payload)
                self.assertNotIn(b"vertex_program/include/functions.inc", payload)
            elif row["path"].endswith(".inc"):
                self.assertIn(
                    b"float3 grPrecuCalculateHemisphericLightingVertexColor(",
                    payload,
                    row["path"],
                )
                self.assertEqual(
                    payload.count(b"grPrecuCalculateHemisphericLightingVertexColor("),
                    4,
                    row["path"],
                )
            else:
                self.assertTrue(payload.startswith(b"FORM"), row["path"])
                self.assertIn(b"//hlsl", payload, row["path"])


if __name__ == "__main__":
    unittest.main()
