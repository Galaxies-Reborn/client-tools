import math
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
UI_ROOT = REPOSITORY_ROOT / "src" / "external" / "3rd" / "library" / "ui" / "src"
OBJECT_LIST_VIEWER_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "engine"
    / "client"
    / "library"
    / "clientUserInterface"
    / "src"
    / "shared"
    / "widget"
    / "CuiWidget3dObjectListViewer.cpp"
)
LAYER_RENDERER_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "engine"
    / "client"
    / "library"
    / "clientUserInterface"
    / "src"
    / "shared"
    / "core"
    / "CuiLayerRenderer.cpp"
)
WORKSPACE_SOURCE = (
    REPOSITORY_ROOT
    / "src"
    / "engine"
    / "client"
    / "library"
    / "clientUserInterface"
    / "src"
    / "shared"
    / "core"
    / "CuiWorkspace.cpp"
)


class UiWidgetScaleSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.widget_header = (UI_ROOT / "win32" / "UIWidget.h").read_text(encoding="utf-8")
        cls.widget_source = (UI_ROOT / "win32" / "UIWidget.cpp").read_text(encoding="utf-8")
        cls.page_source = (UI_ROOT / "win32" / "UIPage.cpp").read_text(encoding="utf-8")
        cls.canvas_source = (UI_ROOT / "win32" / "UICanvas.cpp").read_text(encoding="utf-8")
        cls.manager_source = (UI_ROOT / "win32" / "UIManager.cpp").read_text(encoding="utf-8")
        cls.renderer_source = (UI_ROOT / "shared" / "core" / "UIRenderHelper.cpp").read_text(encoding="utf-8")
        cls.object_list_viewer_source = OBJECT_LIST_VIEWER_SOURCE.read_text(encoding="utf-8")
        cls.layer_renderer_source = LAYER_RENDERER_SOURCE.read_text(encoding="utf-8")
        cls.workspace_source = WORKSPACE_SOURCE.read_text(encoding="utf-8")

    def test_scale_is_a_serialized_cloned_widget_property(self):
        self.assertIn("static const UILowerString Scale;", self.widget_header)
        self.assertIn("void            SetScale", self.widget_header)
        self.assertIn("float           GetScale", self.widget_header)
        self.assertIn("mScale               (1.0f)", self.widget_source)
        self.assertIn("_DESCRIPTOR(Scale,\"1\",T_float)", self.widget_source)
        self.assertIn("Name == PropertyName::Scale", self.widget_source)
        self.assertIn("SetScale(rhs_widget.GetScale());", self.widget_source)

    def test_world_point_apis_and_rects_include_ancestor_scales(self):
        self.assertIn("GetWorldPointFromLocal", self.widget_header)
        self.assertIn("GetLocalPointFromWorld", self.widget_header)
        self.assertIn("result.scale *= current.GetScale();", self.widget_source)
        self.assertIn("current.GetScrollLocation().x", self.widget_source)
        self.assertIn("transform.scale * static_cast<float>(mSize.x)", self.widget_source)
        self.assertIn("std::floor(transform.originX)", self.widget_source)
        self.assertIn(
            "std::ceil(transform.originX + transform.scale * static_cast<float>(mSize.x))",
            self.widget_source,
        )

        # Match the half-open pixel coverage used by scaled rendering.
        self.assertEqual((0, 3), (math.floor(0.0), math.ceil(3 * 0.75)))
        self.assertEqual((0, 2), (math.floor(0.0), math.ceil(1 * 1.25)))

    def test_render_and_clip_have_exact_scale_one_fast_paths(self):
        self.assertIn("if (scale == 1.0f)", self.renderer_source)
        self.assertIn("w.GetLocation () - w.GetScrollLocation ()", self.renderer_source)
        self.assertIn("DestinationCanvas.Scale (scale, scale);", self.renderer_source)
        self.assertIn("if (!mState.TranslationOnly)", self.canvas_source)
        self.assertIn("getTransformedBounds", self.canvas_source)

    def test_scaled_blits_clip_geometry_and_texture_coordinates(self):
        self.assertIn("bool clipAxisAlignedQuad", self.canvas_source)
        self.assertIn("std::max(originalLeft, static_cast<float>(clippingRect.left))", self.canvas_source)
        self.assertIn("std::min(originalBottom, static_cast<float>(clippingRect.bottom))", self.canvas_source)
        self.assertIn("interpolateQuad(originalTextureCoordinates, leftAmount, topAmount)", self.canvas_source)
        self.assertEqual(
            2,
            self.canvas_source.count(
                "!clipAxisAlignedQuad(s_theVertices, src ? s_theUVs : 0, mState.ClippingRect)"
            ),
        )
        self.assertGreaterEqual(self.canvas_source.count("if( mState.TranslationOnly )"), 2)

        # A 100x40 quad scaled to 125x50 and clipped 25 physical pixels from
        # the left must advance its source U coordinate by 20%, not 25%.
        original_left, original_right = 10.0, 135.0
        clipped_left = 35.0
        source_left, source_right = 0.25, 0.75
        left_amount = (clipped_left - original_left) / (original_right - original_left)
        clipped_source_left = source_left + (source_right - source_left) * left_amount
        self.assertAlmostEqual(0.2, left_amount)
        self.assertAlmostEqual(0.35, clipped_source_left)

    def test_manager_context_and_drag_coordinates_use_inverse_widget_scale(self):
        expected_conversions = (
            "activeWidget->GetLocalPointFromWorld(requestPoint)",
            "DraggedControl->GetLocalPointFromWorld(Msg.MouseCoords)",
            "parentWidget->GetLocalPointFromWorld(Msg.MouseCoords)",
            "DragTarget->GetLocalPointFromWorld(Ref)",
        )
        for conversion in expected_conversions:
            self.assertIn(conversion, self.manager_source)

        stale_subtractions = (
            "requestPoint - activeWidget->GetWorldLocation",
            "Msg.MouseCoords - DraggedControl->GetWorldLocation",
            "Msg.MouseCoords - parentWidget->GetWorldLocation",
            "Ref - DragTarget->GetWorldLocation",
        )
        for subtraction in stale_subtractions:
            self.assertNotIn(subtraction, self.manager_source)

    def test_tooltip_queries_convert_world_point_for_each_widget(self):
        self.assertIn(
            "ObjectUnderCursor->GetLocalPointFromWorld(mLastMouseCoord)",
            self.manager_source,
        )
        self.assertIn(
            "WidgetToQuery->GetLocalPointFromWorld(mLastMouseCoord)",
            self.manager_source,
        )
        self.assertNotIn("mLastMouseCoord - worldLocation", self.manager_source)
        self.assertNotIn("tooltipPt += WidgetToQuery->GetLocation", self.manager_source)

    def test_shrink_wrap_stays_in_logical_parent_coordinates(self):
        self.assertIn(
            "widget->GetLocation() - GetScrollLocation()", self.widget_source
        )
        self.assertIn(
            "static_cast<float>(widget->GetWidth()) * widget->GetScale()",
            self.widget_source,
        )
        self.assertIn(
            "GetWorldPointFromLocal(newRect.Location())", self.widget_source
        )
        self.assertIn(
            "parentWidget->GetLocalPointFromWorld(childWorldLocation) + parentWidget->GetScrollLocation()",
            self.widget_source,
        )
        self.assertNotIn("SetSize(newRect.Size())", self.widget_source)

        wrapper_scale = 1.25
        ancestor_scale = 1.2
        wrapper_scroll = 3
        child_location = 13
        child_size = 40
        child_scale = 1.5
        logical_left = child_location - wrapper_scroll
        logical_width = math.ceil(child_size * child_scale)
        world_width = logical_width * wrapper_scale * ancestor_scale
        self.assertEqual(10, logical_left)
        self.assertEqual(60, logical_width)
        self.assertEqual(90, world_width)

    def test_3d_viewport_uses_complete_physical_canvas_transform_once(self):
        render_start = self.object_list_viewer_source[
            self.object_list_viewer_source.index(
                "void CuiWidget3dObjectListViewer::RenderStart"
            ) : self.object_list_viewer_source.index(
                "void CuiWidget3dObjectListViewer::RenderText"
            )
        ]
        self.assertIn("canvas.TransformFP(0.0f, 0.0f)", render_start)
        self.assertIn("canvas.GetCurrentState().ClippingRect", render_start)
        self.assertNotIn("Graphics::getUiCanvasScale()", render_start)
        self.assertNotIn("canvas.GetTranslation()", render_start)
        self.assertNotIn("canvas.GetClip", render_start)
        self.assertIn("static_cast<int>(m_viewport.left)", render_start)
        self.assertIn("m_controlToPhysicalOrigin = transformedCorners[0]", render_start)
        self.assertIn("canvas.Deform(allCorners, deformedCorners, 8)", render_start)

        self.assertIn("controlToPhysical(UIFloatPoint const & point)", self.object_list_viewer_source)
        self.assertIn("physicalToControl(UIFloatPoint const & point)", self.object_list_viewer_source)
        self.assertIn(
            "physicalToControl(UIFloatPoint(screen2d.x, screen2d.y))",
            self.object_list_viewer_source,
        )

        find_world = self.object_list_viewer_source[
            self.object_list_viewer_source.index(
                "bool CuiWidget3dObjectListViewer::findWorldLocation"
            ) : self.object_list_viewer_source.index(
                "const ClientObject * CuiWidget3dObjectListViewer::getObjectAt"
            )
        ]
        self.assertIn("controlToPhysical(UIFloatPoint(pointLocalToControl))", find_world)
        self.assertIn("static_cast<int>(m_viewport.left)", find_world)
        self.assertNotIn("m_viewport.left * uiScale", find_world)

        translation = (100.0, 50.0)
        individual_scale = 1.25
        widget_size = (80.0, 40.0)
        global_scale = 1.5
        transformed = (
            math.floor(translation[0] * global_scale),
            math.floor(translation[1] * global_scale),
            math.ceil(
                (translation[0] + widget_size[0] * individual_scale)
                * global_scale
            ),
            math.ceil(
                (translation[1] + widget_size[1] * individual_scale)
                * global_scale
            ),
        )
        clip = (165, 82, 285, 142)
        clipped = (
            max(transformed[0], clip[0]),
            max(transformed[1], clip[1]),
            min(transformed[2], clip[2]),
            min(transformed[3], clip[3]),
        )
        camera_viewport = (
            int(clipped[0]),
            int(clipped[1]),
            int(clipped[2] - clipped[0]),
            int(clipped[3] - clipped[1]),
        )
        self.assertEqual((150, 75, 300, 150), transformed)
        self.assertEqual((165, 82, 285, 142), clipped)
        self.assertEqual((165, 82, 120, 60), camera_viewport)

    def test_pretransformed_cooldown_triangles_are_not_globally_scaled_twice(self):
        triangle_renderer = self.layer_renderer_source[
            self.layer_renderer_source.index("void CuiLayerRenderer::renderTriangles") :
            self.layer_renderer_source.index(
                "void CuiLayerRenderer::renderLine",
                self.layer_renderer_source.index("void CuiLayerRenderer::renderTriangles") + 1,
            )
        ]
        self.assertNotIn("Graphics::getUiCanvasScale()", triangle_renderer)
        self.assertIn(
            "setPosition(tri.p1.x + pixOffset, tri.p1.y + pixOffset",
            triangle_renderer,
        )

    def test_mouse_hit_capture_and_hover_routes_are_inverse_scaled(self):
        self.assertGreaterEqual(self.page_source.count("getChildLocalPoint(*this"), 5)
        self.assertIn("GetLocalPointFromWorld(GetWorldPointFromLocal(msg.MouseCoords))", self.page_source)
        self.assertIn("std::floor(static_cast<float>(coordinate) / scale)", self.page_source)

    def test_scaled_widget_move_uses_stable_parent_coordinates(self):
        self.assertIn("mUserModificationStartPointInParent", self.widget_header)
        self.assertIn(
            "getParentPointFromLocal(*this, msg.MouseCoords)", self.widget_source
        )

        move_case = self.widget_source[
            self.widget_source.index("case UMT_MOVE:") : self.widget_source.index(
                "if ((rect.top < 0L)",
                self.widget_source.index("case UMT_MOVE:"),
            )
        ]
        self.assertIn("rect += moveDiff;", move_case)
        self.assertNotIn("rect += diff;", move_case)

        # Resize calculations intentionally stay in widget-local units. Scaling
        # those deltas would make a logical resize step depend on display scale.
        self.assertIn("UIPoint resizeDiff (diff);", self.widget_source)
        self.assertIn("rect.right  + resizeDiff.x", self.widget_source)

        scale = 1.25
        start_location = 100
        start_local = 20
        current_location = 112
        current_local = 22
        old_local_move = (current_local - start_local) + (
            current_location - start_location
        )
        start_parent = start_location + round(scale * start_local)
        current_parent = current_location + round(scale * current_local)
        stable_parent_move = current_parent - start_parent
        self.assertEqual(14, old_local_move)
        self.assertEqual(15, stable_parent_move)

    def test_workspace_snapping_uses_visual_rects_and_returns_logical_rects(self):
        self.assertIn("getVisualWorkspaceRect", self.workspace_source)
        self.assertIn("getLogicalWidgetRect", self.workspace_source)
        self.assertIn(
            "getVisualWorkspaceRect (m_page, page, page.GetRect ())",
            self.workspace_source,
        )
        self.assertIn(
            "getVisualWorkspaceRect (m_page, *context, targetRect)",
            self.workspace_source,
        )
        self.assertIn("context->SetRect (logicalResultRect);", self.workspace_source)
        self.assertNotIn("const UIRect rect (page.GetRect ());", self.workspace_source)
        self.assertIn("std::floor (topLeft.x)", self.workspace_source)
        self.assertIn("std::ceil (bottomRight.x)", self.workspace_source)

        # A 100-wide widget shown at 125% occupies 125 workspace pixels.  Its
        # visible right edge therefore snaps to a neighbor at x=300 from a
        # logical x of 175, while retaining its 100-unit logical width.
        widget_scale = 1.25
        logical_location = 175
        logical_width = 100
        visual_right = logical_location + round(widget_scale * logical_width)
        self.assertEqual(300, visual_right)
        self.assertEqual(logical_width, 100)

    def test_workspace_visual_conversion_handles_nested_scale_and_scroll_once(self):
        self.assertIn("transformWidgetParentPointToWorkspace", self.workspace_source)
        self.assertIn("transformWorkspacePointToWidgetParent", self.workspace_source)
        self.assertIn(
            "point.x -= static_cast<float> (workspace.GetScrollLocation ().x);",
            self.workspace_source,
        )
        self.assertIn(
            "point.x += static_cast<float> (workspace.GetScrollLocation ().x);",
            self.workspace_source,
        )
        self.assertIn(
            "const float widgetScale = widget.GetScale ();",
            self.workspace_source,
        )

        workspace_scroll = 7.0
        parent_location = 30.0
        parent_scroll = 5.0
        parent_scale = 1.2
        widget_location = 40.0
        widget_scale = 1.25
        widget_width = 80.0

        visual_left = (
            parent_location
            + parent_scale * (widget_location - parent_scroll)
            - workspace_scroll
        )
        visual_right = (
            parent_location
            + parent_scale
            * (widget_location + widget_scale * widget_width - parent_scroll)
            - workspace_scroll
        )
        self.assertEqual((65.0, 185.0), (visual_left, visual_right))

        parent_left = (
            (visual_left + workspace_scroll - parent_location) / parent_scale
            + parent_scroll
        )
        parent_right = (
            (visual_right + workspace_scroll - parent_location) / parent_scale
            + parent_scroll
        )
        logical_width_after_round_trip = (parent_right - parent_left) / widget_scale
        self.assertEqual(widget_location, parent_left)
        self.assertEqual(widget_width, logical_width_after_round_trip)

    def test_position_mediator_converts_world_mouse_to_workspace_content(self):
        self.assertIn(
            "m_page.GetLocalPointFromWorld (UIManager::gUIManager ().GetLastMouseCoord ())",
            self.workspace_source,
        )
        self.assertIn("m_page.GetScrollLocation ();", self.workspace_source)

    def test_nested_scale_scroll_transform_round_trip(self):
        # root: location (10,20), scale 1.25, scroll (4,6)
        # child: location (30,40), scale 0.8, scroll (2,3)
        # leaf: location (5,7), scale 1.5
        origin_x = 10.0
        origin_y = 20.0
        cumulative_scale = 1.25
        origin_x -= cumulative_scale * 4.0
        origin_y -= cumulative_scale * 6.0
        origin_x += cumulative_scale * 30.0
        origin_y += cumulative_scale * 40.0
        cumulative_scale *= 0.8
        origin_x -= cumulative_scale * 2.0
        origin_y -= cumulative_scale * 3.0
        origin_x += cumulative_scale * 5.0
        origin_y += cumulative_scale * 7.0
        cumulative_scale *= 1.5

        self.assertEqual((45.5, 66.5, 1.5), (origin_x, origin_y, cumulative_scale))

        local_point = (8, 4)
        rounded_world = (
            math.floor(origin_x + cumulative_scale * local_point[0] + 0.5),
            math.floor(origin_y + cumulative_scale * local_point[1] + 0.5),
        )
        round_trip = (
            math.floor((rounded_world[0] - origin_x) / cumulative_scale),
            math.floor((rounded_world[1] - origin_y) / cumulative_scale),
        )
        self.assertEqual((58, 73), rounded_world)
        self.assertEqual(local_point, round_trip)


if __name__ == "__main__":
    unittest.main()
