import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
GROUND_SCENE_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp"
)


class Publish14GroundInputMapCompatibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = GROUND_SCENE_SOURCE.read_text(encoding="utf-8")
        match = re.search(
            r"void\s+GroundScene::init\b.*?\n\}",
            cls.source,
            flags=re.DOTALL,
        )
        if match is None:
            raise AssertionError("GroundScene::init was not found")
        cls.init = match.group(0)

    def test_all_auxiliary_cameras_use_publish14_ground_input_map(self):
        expected_assignments = (
            'm_freeCameraInputMap = new InputMap ("input/groundinputmap_swg.iff", 0, 0);',
            'm_debugPortalCameraInputMap = new InputMap ("input/groundinputmap_swg.iff", 0, 0);',
            'm_structurePlacementCameraInputMap = new InputMap ("input/groundinputmap_swg.iff", 0, 0);',
        )
        for assignment in expected_assignments:
            with self.subTest(assignment=assignment):
                self.assertEqual(1, self.init.count(assignment))
        self.assertEqual(
            3,
            self.init.count(
                'new InputMap ("input/groundinputmap_swg.iff", 0, 0);'
            ),
        )

    def test_each_auxiliary_map_keeps_its_camera_controller_queue(self):
        queue_assignments = (
            "m_freeCameraInputMap->setMessageQueue (freeCameraController->getMessageQueue ());",
            "m_debugPortalCameraInputMap->setMessageQueue (debugPortalCameraController->getMessageQueue ());",
            "m_structurePlacementCameraInputMap->setMessageQueue (structurePlacementCameraController->getMessageQueue ());",
        )
        for assignment in queue_assignments:
            with self.subTest(assignment=assignment):
                self.assertEqual(1, self.init.count(assignment))

    def test_later_missing_input_map_paths_are_not_requested(self):
        self.assertNotIn("input/groundinputmap_freecamera.iff", self.init)
        self.assertNotIn("input/groundinputmap_debugportalcamera.iff", self.init)


if __name__ == "__main__":
    unittest.main()
