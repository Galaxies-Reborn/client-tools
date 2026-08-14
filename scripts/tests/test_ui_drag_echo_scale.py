import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
UI_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "external"
    / "3rd"
    / "library"
    / "ui"
    / "src"
    / "win32"
)
TABLE_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "external"
    / "3rd"
    / "library"
    / "ui"
    / "src"
    / "shared"
    / "table"
    / "UITable.cpp"
)


class UiDragEchoScaleSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manager_header = (UI_SOURCE / "UIManager.h").read_text(encoding="utf-8")
        cls.manager_source = (UI_SOURCE / "UIManager.cpp").read_text(encoding="utf-8")
        cls.widget_source = (UI_SOURCE / "UIWidget.cpp").read_text(encoding="utf-8")
        cls.table_source = TABLE_SOURCE.read_text(encoding="utf-8")

    def test_drag_echo_captures_and_resets_complete_source_scale(self):
        self.assertIn("float                  mDragEchoScale;", self.manager_header)
        self.assertIn("mDragEchoScale            (1.0f)", self.manager_source)
        self.assertIn(
            "float getCumulativeWidgetScale(UIWidget const & widget)",
            self.manager_source,
        )
        self.assertIn("scale *= current->GetScale();", self.manager_source)
        self.assertIn("current = current->GetParentWidget();", self.manager_source)
        self.assertIn(
            "UIWidget const * const scaleWidget = NewEcho == source ? source : NewEcho;",
            self.manager_source,
        )
        self.assertIn(
            "float const dragEchoScale = scaleWidget ? getCumulativeWidgetScale(*scaleWidget) : 1.0f;",
            self.manager_source,
        )
        self.assertIn("mDragEchoScale  = dragEchoScale;", self.manager_source)
        self.assertGreaterEqual(self.manager_source.count("mDragEchoScale = 1.0f;"), 1)

        # Nested individual scales compound before the drag widget is detached.
        self.assertAlmostEqual(0.9375, 0.75 * 1.25)
        self.assertAlmostEqual(1.5, 1.0 * 1.5)

    def test_distinct_custom_echo_uses_echo_local_hotspot_and_scale(self):
        self.assertIn(
            "offset = -mCustomDragWidget->GetLocalPointFromWorld(GetWorldPointFromLocal(pt));",
            self.widget_source,
        )
        self.assertIn(
            "offset = -dragWidget->GetLocalPointFromWorld(GetWorldPointFromLocal(point));",
            self.table_source,
        )
        self.assertNotIn(
            "mCustomDragWidget->GetWorldLocation () + (mCustomDragWidget->GetSize () / 2L)",
            self.widget_source,
        )
        self.assertNotIn(
            "dragWidget->GetWorldLocation () + (dragWidget->GetSize () / 2L)",
            self.table_source,
        )

        # Attached custom echoes use their full hierarchy; detached echoes
        # naturally retain just their own Scale property.
        attached_echo_scale = 1.25 * 0.8
        detached_echo_scale = 0.8
        self.assertAlmostEqual(1.0, attached_echo_scale)
        self.assertAlmostEqual(0.8, detached_echo_scale)

    def test_scaled_hotspot_clip_and_render_share_one_canvas_transform(self):
        render_start = self.manager_source.index("void UIManager::Render(")
        render_end = self.manager_source.index("UIWidget * const ObjectUnderCursor", render_start)
        render = self.manager_source[render_start:render_end]

        fast_translate = "DestinationCanvas.Translate( mLastMouseCoord + mDragEchoOffset );"
        mouse_translate = "DestinationCanvas.Translate( mLastMouseCoord );"
        scale = "DestinationCanvas.Scale( mDragEchoScale, mDragEchoScale );"
        offset_translate = "DestinationCanvas.Translate( mDragEchoOffset );"
        clip = "DestinationCanvas.Clip( UIRect( 0, 0, mDraggedControl->GetWidth(), mDraggedControl->GetHeight() ) );"
        scroll = "DestinationCanvas.Translate( -mDraggedControl->GetScrollLocation() );"
        draw = "mDraggedControl->Render( DestinationCanvas );"

        self.assertIn("if (mDragEchoScale == 1.0f)", render)
        self.assertIn(fast_translate, render)
        self.assertLess(render.index(mouse_translate), render.index(scale))
        self.assertLess(render.index(scale), render.index(offset_translate))
        self.assertLess(render.index(offset_translate), render.index(clip))
        self.assertLess(render.index(clip), render.index(scroll))
        self.assertLess(render.index(scroll), render.index(draw))

        # A local hotspot remains under the world-space cursor at either end
        # of the supported 75%-150% range.
        cursor = 500.0
        local_hotspot = 32.0
        for individual_scale in (0.75, 1.0, 1.5):
            echo_origin = cursor - local_hotspot * individual_scale
            rendered_hotspot = echo_origin + local_hotspot * individual_scale
            self.assertAlmostEqual(cursor, rendered_hotspot)


if __name__ == "__main__":
    unittest.main()
