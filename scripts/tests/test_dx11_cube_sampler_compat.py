from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SHADER_SOURCE = (
    ROOT
    / "src/engine/client/application/Direct3d11/src/win32/Direct3d11_ShaderSource.cpp"
)
CUBE_PROGRAMS = (
    "pixel_program/a_alpha_envmask_ps20.psh",
    "pixel_program/a_envmask_specmap_ps20.psh",
    "pixel_program/h_color2_envmask_specmap_ps20.psh",
)


def shader_source(payload: bytes) -> bytes:
    offset = payload.find(b"PSRC")
    if offset < 0:
        raise AssertionError("shader has no PSRC chunk")
    length = int.from_bytes(payload[offset + 4 : offset + 8], "big")
    return payload[offset + 8 : offset + 8 + length]


class Dx11CubeSamplerCompatibilityTests(unittest.TestCase):
    def test_generic_d3d9_cube_samplers_are_specialized_before_compile(self) -> None:
        source = SHADER_SOURCE.read_text(encoding="utf-8")

        self.assertIn("patchCubeSamplerDeclarations", source)
        self.assertIn('char const texCube[] = "texCUBE";', source)
        self.assertIn('memcpy(destination, "samplerCUBE"', source)
        self.assertIn("if (!isVertexProgram)", source)
        invocation = source.index("patchCubeSamplerDeclarations(scanSource")
        wrapper = source.index("Direct3d11_ShaderSignature::wrapPixelProgram", invocation)
        self.assertLess(invocation, wrapper)

    def test_patch_is_identifier_scoped_not_shader_name_scoped(self) -> None:
        source = SHADER_SOURCE.read_text(encoding="utf-8")

        helper = source[
            source.index("char *patchCubeSamplerDeclarations") :
            source.index("// ------------------------------------------------------------------", source.index("char *patchCubeSamplerDeclarations"))
        ]
        self.assertIn("isCubeSampledIdentifier", helper)
        self.assertIn("isIdentifierCharacter", helper)
        self.assertNotIn("a_envmask_specmap_ps20", helper)
        self.assertNotIn("a_alpha_envmask_ps20", helper)

    def test_retail_cube_program_overrides_have_explicit_cube_samplers(self) -> None:
        converted = ROOT / "scripts/asm2hlsl/converted"
        for relative_path in CUBE_PROGRAMS:
            with self.subTest(relative_path=relative_path):
                source = shader_source((converted / relative_path).read_bytes())
                self.assertIn(b"samplerCUBE envMap", source.replace(b"\t", b" "))
                self.assertNotRegex(source, rb"(?m)^sampler[ \t]+envMap")
                self.assertIn(
                    b"float3 grPrecuCalculateHemisphericLighting(", source
                )
                self.assertIn(
                    b"grPrecuCalculateHemisphericLighting("
                    b"dot3LightDirection, normal_o, vertexDiffuse)",
                    source,
                )
                self.assertNotRegex(
                    source,
                    rb"(?<![A-Za-z0-9_])calculateHemisphericLighting\(",
                )


if __name__ == "__main__":
    unittest.main()
