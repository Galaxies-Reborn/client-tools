from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
OVERHEAD_MAP = ROOT / (
    "src/engine/client/library/clientGame/src/shared/scene/OverheadMap.cpp"
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


class PrecuOverheadMapDx11Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = OVERHEAD_MAP.read_text(encoding="utf-8")

    def test_triangle_buffers_live_through_the_draw(self) -> None:
        body = function_body(
            self.source,
            "void OverheadMapNamespace::renderIndexedTriangleList",
        )
        draw = body.index("Graphics::drawIndexedTriangleList ()")
        self.assertLess(body.index("DynamicVertexBuffer vertexBuffer"), draw)
        self.assertLess(body.index("DynamicIndexBuffer indexBuffer"), draw)
        self.assertNotIn("Graphics::setVertexBuffer (vertexBuffer);\n\t}", body)
        self.assertNotIn("Graphics::setIndexBuffer (indexBuffer);\n\t}", body)

    def test_vertex_color_shader_input_is_complete(self) -> None:
        for signature in (
            "void OverheadMapNamespace::renderIndexedTriangleList",
            "void OverheadMapNamespace::renderLineList",
            "void OverheadMapNamespace::renderLineStrip",
        ):
            with self.subTest(signature=signature):
                body = function_body(self.source, signature)
                self.assertIn("format.setPosition ();", body)
                self.assertIn("format.setNormal ();", body)
                self.assertIn("format.setColor0 ();", body)
                self.assertIn("v.setNormal (Vector::unitY);", body)

    def test_line_buffers_live_through_their_draws(self) -> None:
        for signature, draw_call in (
            ("void OverheadMapNamespace::renderLineList", "Graphics::drawLineList ()"),
            ("void OverheadMapNamespace::renderLineStrip", "Graphics::drawLineStrip ()"),
        ):
            with self.subTest(signature=signature):
                body = function_body(self.source, signature)
                self.assertLess(
                    body.index("DynamicVertexBuffer vertexBuffer"),
                    body.index(draw_call),
                )
                self.assertNotIn(
                    "Graphics::setVertexBuffer (vertexBuffer);\n\t}", body
                )


if __name__ == "__main__":
    unittest.main()
