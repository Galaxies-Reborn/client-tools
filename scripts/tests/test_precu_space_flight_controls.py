from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
INPUT_SCHEME = ROOT / "src/engine/client/library/clientGame/src/shared/core/InputScheme.cpp"
GAME = ROOT / "src/engine/client/library/clientGame/src/shared/core/Game.cpp"
GROUND_SCENE = ROOT / "src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp"
CREATURE_OBJECT = ROOT / "src/engine/client/library/clientGame/src/shared/object/CreatureObject.cpp"
HUD_FACTORY = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/page/SwgCuiHudFactory.cpp"
CUI_MANAGER = ROOT / "src/game/client/library/swgClientUserInterface/src/shared/core/SwgCuiManager.cpp"
COMMANDS = ROOT / "src/engine/client/library/clientGame/src/shared/command/CommandCppFuncs.cpp"
SPACE_TRANSITION = (
    ROOT.parent
    / "pre-cu-reborn-server-x64"
    / "dsrc/sku.0/sys.server/compiled/game/script/library/space_transition.java"
)


class PreCuSpaceFlightControlsTests(unittest.TestCase):
    def test_space_hud_and_bindings_follow_the_live_ship_station(self) -> None:
        game = GAME.read_text(encoding="utf-8")
        input_scheme = INPUT_SCHEME.read_text(encoding="utf-8")
        hud = HUD_FACTORY.read_text(encoding="utf-8")

        self.assertIn("creature->getShipStation() != ShipStation::ShipStation_None", game)
        self.assertIn('"input/spaceinputmap_default.iff"', input_scheme)
        self.assertIn('"input/spaceinputmap_ja101.iff"', input_scheme)
        self.assertIn('s_dataMap[Game::ST_space].insert', input_scheme)
        self.assertIn("SwgCuiHudSpace::createFreshHud", hud)
        self.assertIn('CuiSettings::setPrefixString ("space_")', hud)
        self.assertIn("groundScene->loadInputMap()", hud)

    def test_atmospheric_pilot_transition_selects_cockpit_before_hud_reset(self) -> None:
        manager = CUI_MANAGER.read_text(encoding="utf-8")
        scene = GROUND_SCENE.read_text(encoding="utf-8")

        cockpit = manager.index("groundScene->setView(GroundScene::CI_cockpit);")
        scene_change = manager.index("Game::emitSceneChange();", cockpit)
        self.assertLess(cockpit, scene_change)
        self.assertIn("station == ShipStation::ShipStation_Pilot", manager)
        self.assertIn("groundScene->setView(GroundScene::CI_freeChase);", manager)
        self.assertIn("m_cockpitCamera = new CockpitCamera();", scene)
        self.assertIn("return m_cockpitCamera->isFirstPerson();", scene)

    def test_cross_scene_pilot_containment_restores_controller_and_station_edge(self) -> None:
        scene = GROUND_SCENE.read_text(encoding="utf-8")
        creature = CREATURE_OBJECT.read_text(encoding="utf-8")

        self.assertIn("localPilot == Game::getPlayerCreature()", scene)
        self.assertIn("dynamic_cast<PlayerShipController *>(pilotedShip->getController())", scene)
        self.assertIn("pilotedShip->onShipPilotMounted(localPilot);", scene)
        self.assertIn("setView(GroundScene::CI_cockpit);", scene)

        containment = creature[
            creature.index("void CreatureObject::containedByModified") :
            creature.index("void CreatureObject::arrangementModified")
        ]
        self.assertIn("if (shipStationNow == ShipStation::ShipStation_Pilot)", containment)
        self.assertIn("onEnteredPilotStation();", containment)

    def test_waypoint_autopilot_requires_and_uses_the_containing_ship(self) -> None:
        commands = COMMANDS.read_text(encoding="utf-8")
        self.assertIn("Game::getPlayerPilotedShip()", commands)
        self.assertIn("engageAutopilotToLocation", commands)
        self.assertIn("waypointAutopilot", commands)

    def test_cross_zone_launch_retains_ship_identity_until_pilot_handoff(self) -> None:
        transition = SPACE_TRANSITION.read_text(encoding="utf-8")
        scene_change = transition[
            transition.index("public static void handlePotentialSceneChange") :
            transition.index("public static boolean shouldSendToGroundOnLogout")
        ]

        self.assertIn("MAX_LAUNCH_PILOT_RETRIES = 40", transition)
        self.assertIn("if (isIdValid(launchedShip))", scene_change)
        self.assertIn("boolean shipLoaded = launchedShip.isLoaded();", scene_change)
        self.assertIn('messageTo(player, "retrySpaceLaunchPilot", null, 0.5f, false);', scene_change)
        retry = scene_change.index("if (isIdValid(launchedShip))", scene_change.index("if (isIdValid(launchedShip) && launchedShip.isLoaded())") + 1)
        final_cleanup = scene_change.index("clearLaunchPilotHandoff(player);", retry)
        self.assertLess(retry, final_cleanup)
        self.assertNotIn(
            'removeObjVar(player, "space.launch.ship");\n                removeObjVar(player, "space.launch.startIndex");',
            scene_change[:retry],
        )


if __name__ == "__main__":
    unittest.main()
