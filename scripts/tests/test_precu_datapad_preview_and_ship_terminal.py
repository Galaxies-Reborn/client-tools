from pathlib import Path
import unittest


TOOLS_ROOT = Path(__file__).resolve().parents[2]
SERVER_ROOT = TOOLS_ROOT.parent / "pre-cu-reborn-server-x64" / "src"


def read_tools(path: str) -> str:
    return (TOOLS_ROOT / path).read_text(encoding="utf-8")


def read_server(path: str) -> str:
    return (SERVER_ROOT / path).read_text(encoding="utf-8")


class DatapadPreviewAndShipTerminalTests(unittest.TestCase):
    def test_ship_terminal_mediator_precedes_request(self) -> None:
        source = read_tools(
            "src/game/client/library/swgClientUserInterface/src/shared/page/"
            "SwgCuiHudActionGround.cpp"
        )
        block = source[source.index("if(closestSpaceTerminal)") : source.index("else\n", source.index("if(closestSpaceTerminal)"))]
        self.assertLess(block.index("spawnShipChoose"), block.index("CM_spaceTerminalRequest"))

        chooser = read_tools(
            "src/game/client/library/swgClientUserInterface/src/shared/page/"
            "SwgCuiShipChoose.cpp"
        )
        self.assertIn("m_parkingDataReceived = false;", chooser)
        self.assertIn("shipParkingLocation == terminalLocation", chooser)
        self.assertIn("CuiStringIdsShipChoose::here.localize()", chooser)
        self.assertIn("CuiStringIdsShipChoose::not_parked_here.localize()", chooser)
        self.assertIn("selectButton->SetEnabled(canSelect)", chooser)

    def test_server_response_always_starts_with_terminal(self) -> None:
        source = read_server(
            "engine/server/library/serverGame/src/shared/controller/"
            "PlayerCreatureController.cpp"
        )
        block = source[source.index("case CM_spaceTerminalRequest:") : source.index("case CM_droidCommandProgramming:")]
        terminal_entry = "outData.push_back(std::make_pair(terminal->getNetworkId(), terminalParkingLocation));"
        self.assertEqual(block.count(terminal_entry), 1)
        self.assertLess(block.index(terminal_entry), block.index("owner->getAllShipsInDatapad(ships)"))
        self.assertNotIn("if (building) {", block)

    def test_viewer_separates_logical_and_render_objects(self) -> None:
        header = read_tools(
            "src/engine/client/library/clientUserInterface/src/shared/widget/"
            "CuiWidget3dObjectListViewer.h"
        )
        source = read_tools(
            "src/engine/client/library/clientUserInterface/src/shared/widget/"
            "CuiWidget3dObjectListViewer.cpp"
        )
        self.assertIn("addObject                (Object & logicalObject, Object & renderObject)", header)
        self.assertIn("ObjectPair(Watcher<Object>(&logicalObject), Watcher<Object>(&renderObject))", source)
        self.assertIn("renderObject.alter", source)

    def test_ship_preview_uses_real_ship_and_persists_its_link(self) -> None:
        source = read_tools(
            "src/engine/client/library/clientUserInterface/src/shared/core/"
            "CuiIconManager.cpp"
        )
        header = read_tools(
            "src/engine/client/library/clientUserInterface/src/shared/core/"
            "CuiIconManager.h"
        )
        info = read_tools(
            "src/game/client/library/swgClientUserInterface/src/shared/page/"
            "SwgCuiInventoryInfo.cpp"
        )
        server = read_server(
            "engine/server/library/serverGame/src/shared/object/"
            "IntangibleObject.cpp"
        )
        self.assertIn("GOT_data_ship_control_device", source)
        self.assertIn("ContainerInterface::getContainer(logicalObject)", source)
        self.assertIn("containedClientObject->asShipObject()", source)
        self.assertIn("gr_internal_ship_preview_id", source)
        self.assertIn("NetworkIdManager::getObjectById(NetworkId(shipIdString))", source)
        self.assertIn("s_datapadShipPreviewCache", source)
        self.assertIn("s_datapadShipFallbackCache", source)
        self.assertIn("ObjectTemplateList::createObject(templateCrc)", source)
        self.assertIn("customizationData->loadLocalDataFromString(customization)", source)
        self.assertIn("Object * const persistentFallback", source)
        self.assertIn("linkedObject->isInWorld() || ContainerInterface::getContainedByObject(*linkedObject)", source)
        self.assertIn("cachedObject->isInWorld() || ContainerInterface::getContainedByObject(*cachedObject)", source)
        self.assertLess(
            source.index("Object * const persistentFallback"),
            source.index("Container const * const container = ContainerInterface::getContainer(logicalObject)"),
        )
        self.assertLess(
            source.index("Container const * const container = ContainerInterface::getContainer(logicalObject)"),
            source.index("if (persistentFallback)"),
        )
        self.assertIn("clearDatapadShipFallbackCache();", source)
        self.assertIn("viewer->addObject(obj, *renderObject)", source)
        self.assertIn("dragWidget->addObject(obj, *renderObject)", source)
        self.assertIn("getObjectPreviewRenderObject(Object & logicalObject)", header)
        self.assertIn("CuiIconManager::getObjectPreviewRenderObject(*object)", info)
        self.assertIn("CuiIconManager::getObjectPreviewRenderObject(*clientObject)", info)
        self.assertIn("OBJVAR_ATMOSPHERIC_SHIP_LINK", server)
        self.assertIn("gr_internal_ship_preview_id", server)
        self.assertIn("ship->getClientSharedTemplateName()", server)
        self.assertIn("ship->getAppearanceData()", server)
        self.assertIn("OBJVAR_SHIP_PREVIEW_TEMPLATE", server)
        self.assertIn("OBJVAR_SHIP_PREVIEW_CUSTOMIZATION", server)
        self.assertIn("void IntangibleObject::onContainerGainItem", server)
        self.assertIn("void IntangibleObject::onContainerLostItem", server)

    def test_ground_call_does_not_force_space_hyperspace_arrival(self) -> None:
        scene = read_tools(
            "src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp"
        )
        start = scene.index("ShipObject * const shipObject = clientObject->asShipObject();")
        end = scene.index("else\n", start)
        creation = scene[start:end]
        self.assertIn("Game::isSpace()", creation)
        self.assertIn("sharedShipObjectTemplate->getPlayerControlled()", creation)
        self.assertIn("shipObject->onEnterByHyperspace();", creation)
        self.assertLess(creation.index("Game::isSpace()"), creation.index("hyperspace = true;"))

    def test_ground_call_recreates_cached_ship_and_descends_vertically(self) -> None:
        scene = read_tools(
            "src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp"
        )
        controller = read_tools(
            "src/engine/client/library/clientGame/src/shared/controller/"
            "RemoteShipController.cpp"
        )
        controller_header = read_tools(
            "src/engine/client/library/clientGame/src/shared/controller/"
            "RemoteShipController.h"
        )
        ship = read_tools(
            "src/engine/client/library/clientGame/src/shared/object/ShipObject.cpp"
        )
        server_controller = read_server(
            "engine/server/library/serverGame/src/shared/controller/"
            "PlayerShipController.cpp"
        )
        server_control_device = read_server(
            "engine/server/library/serverGame/src/shared/object/"
            "IntangibleObject.cpp"
        )

        recreate = scene[
            scene.index("bool const recreateAtmosphericShip") :
            scene.index("else if (clientObject->isClientCached", scene.index("bool const recreateAtmosphericShip"))
        ]
        self.assertIn("!Game::isSpace()", recreate)
        self.assertIn("hyperspace", recreate)
        self.assertIn("!clientObject->isInWorld()", recreate)
        self.assertIn("ContainerInterface::getContainedByObject(*clientObject) == 0", recreate)
        self.assertIn("delete clientObject;", recreate)
        self.assertIn("existingObject = 0;", recreate)
        self.assertIn("shipObject->onEnterByAtmosphere();", scene)
        self.assertIn("void ShipObject::onEnterByAtmosphere()", ship)
        self.assertIn("void enterByAtmosphere();", controller_header)

        arrival = controller[
            controller.index("void RemoteShipController::enterByAtmosphere()") :
        ]
        self.assertIn("cms_atmosphericArrivalHeight = 160.0f", controller)
        self.assertIn("cms_atmosphericArrivalTimeSeconds = 4.0f", controller)
        self.assertIn("arrivalPosition.y += cms_atmosphericArrivalHeight", arrival)
        self.assertNotIn("move_o", arrival)
        self.assertIn("fraction * fraction * (3.0f - 2.0f * fraction)", controller)
        self.assertIn("arrivalPosition.y += cms_atmosphericArrivalHeight * (1.0f - easedFraction)", controller)

        heights = []
        for elapsed in (0.0, 1.0, 2.0, 3.0, 4.0):
            fraction = elapsed / 4.0
            eased = fraction * fraction * (3.0 - 2.0 * fraction)
            heights.append(160.0 * (1.0 - eased))
        self.assertEqual(heights[0], 160.0)
        self.assertEqual(heights[-1], 0.0)
        self.assertTrue(all(a > b for a, b in zip(heights, heights[1:])))

        unpack = server_controller[
            server_controller.index("void PlayerShipController::teleport") :
            server_controller.index("void PlayerShipController::handleMessage")
        ]
        self.assertLess(
            unpack.index("ship->setHyperspaceOnCreate(true);"),
            unpack.index("ContainerInterface::transferItemToWorld"),
        )
        self.assertIn("ship->setHyperspaceOnCreate(false);", unpack)
        gain = server_control_device[
            server_control_device.index("void IntangibleObject::onContainerGainItem") :
            server_control_device.index("void IntangibleObject::onContainerLostItem")
        ]
        self.assertIn("ship->setHyperspaceOnCreate(false);", gain)

    def test_atmospheric_ship_call_explicitly_unpacks_player_ship(self) -> None:
        controller = read_server(
            "engine/server/library/serverGame/src/shared/controller/"
            "PlayerShipController.cpp"
        )
        teleport = controller[
            controller.index("void PlayerShipController::teleport") :
            controller.index("void PlayerShipController::handleMessage")
        ]
        self.assertIn("galaxiesReborn.atmosphericShip.worldVisible", controller)
        self.assertIn("ContainerInterface::getContainedByObject(*ship)", teleport)
        self.assertIn("SharedObjectTemplate::GOT_data_ship_control_device", teleport)
        self.assertIn("ContainerInterface::transferItemToWorld(*ship, goal, nullptr, error)", teleport)
        self.assertLess(
            teleport.index("ContainerInterface::transferItemToWorld"),
            teleport.index("ShipController::teleport(goal, goalObj)"),
        )

        scene = read_tools(
            "src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp"
        )
        containment_start = scene.index('else if(message.isType("UpdateContainmentMessage"))')
        containment_end = scene.index("else if(message.isType", containment_start + 1)
        containment = scene[containment_start:containment_end]
        self.assertIn("target->asShipObject()", containment)
        self.assertIn("o.getContainerId() == NetworkId::cms_invalid", containment)
        self.assertIn("awaiting authoritative SceneCreate", containment)
        self.assertNotIn("RenderWorld::addObjectNotifications(*target);", containment)
        self.assertNotIn("target->addToWorld();", containment)
        self.assertIn("atmosphericShipAwaitingAuthoritativeCreate", containment)
        self.assertLess(
            containment.index("target->removeFromWorld();"),
            containment.index("target->updateContainment(o.getContainerId(), o.getSlotArrangement());"),
        )
        self.assertGreaterEqual(containment.count("target->removeFromWorld();"), 2)
        self.assertLess(
            containment.index("target->updateContainment(o.getContainerId(), o.getSlotArrangement());"),
            containment.rindex("target->removeFromWorld();"),
        )
        self.assertIn("atmosphericShipLeavingControlDevice", scene)
        self.assertIn("atmosphericShipNeedsWorldRegistration", scene)
        self.assertIn(
            "clientObject->updateContainment(NetworkId::cms_invalid, -1)", scene
        )
        recovery_start = scene.index("bool const atmosphericShipNeedsWorldRegistration")
        recovery_end = scene.index("Existing ShipObject %s refreshed", recovery_start)
        recovery = scene[recovery_start:recovery_end]
        self.assertLess(
            recovery.index("clientObject->removeFromWorld();"),
            recovery.index("existingObject->setTransform_o2p (transform);"),
        )
        self.assertLess(
            recovery.index("clientObject->removeFromWorld();"),
            recovery.index("clientObject->asShipObject()->onEnterByAtmosphere();"),
        )
        self.assertLess(
            recovery.index("clientObject->asShipObject()->onEnterByAtmosphere();"),
            recovery.index("RenderWorld::addObjectNotifications(*clientObject);"),
        )
        self.assertLess(
            recovery.index("RenderWorld::addObjectNotifications(*clientObject);"),
            recovery.index("clientObject->addToWorld();"),
        )

        ship_object = read_server(
            "engine/server/library/serverGame/src/shared/object/ShipObject.cpp"
        )
        visibility = ship_object[
            ship_object.index("bool ShipObject::isVisibleOnClient") :
            ship_object.index(
                "// ----------------------------------------------------------------------",
                ship_object.index("bool ShipObject::isVisibleOnClient") + 1,
            )
        ]
        self.assertLess(
            visibility.index("galaxiesReborn.atmosphericShip.worldVisible"),
            visibility.index("TangibleObject::isVisibleOnClient(client)"),
        )
        transfer = ship_object[
            ship_object.index("void ShipObject::onContainerTransferComplete") :
            ship_object.index(
                "// ----------------------------------------------------------------------",
                ship_object.index("void ShipObject::onContainerTransferComplete") + 1,
            )
        ]
        self.assertIn("TangibleObject::onContainerTransferComplete", transfer)
        self.assertIn("SharedObjectTemplate::GOT_data_ship_control_device", transfer)
        self.assertIn("ObserveTracker::isObserving(*client, *this)", transfer)
        self.assertIn("UpdateContainmentMessage const worldContainment", transfer)
        self.assertIn("client->send(worldContainment, true)", transfer)

        self.assertIn("sendCreateAndBaselinesToClient(*client)", transfer)
        self.assertIn("ObserveTracker::onObjectMadeVisibleTo(*this, observers)", transfer)

        library = (
            TOOLS_ROOT.parent
            / "pre-cu-reborn-server-x64"
            / "dsrc/sku.0/sys.server/compiled/game/script/library/atmospheric_ship.java"
        ).read_text(encoding="utf-8")
        call_start = library.index("public static boolean callDown")
        call_end = library.index("public static boolean store", call_start)
        call = library[call_start:call_end]
        self.assertIn("!isOwner(player, ship)", call)
        self.assertIn("location callLocation = getCallLocation(player, ship);", call)
        self.assertIn("callLocation == null || !isCallPathAllowed", call)
        self.assertIn("boolean movedToWorld = setLocation(ship, callLocation);", call)
        self.assertIn("obj_id postCallContainer = getContainedBy(ship);", call)
        self.assertIn("if (!movedToWorld || isIdValid(postCallContainer))", call)
        self.assertIn("rollbackFailedCall(controlDevice, ship);", call)
        self.assertIn('doAnimationAction(player, "manipulate_low");', call)
        self.assertIn('playClientEffectObj(player, "clienteffect/space_command/sys_manipulation.cef", player, "");', call)
        self.assertNotIn("clienteffect/probot_delivery.cef", call)
        self.assertIn("The persistent ShipObject is the ground representation.", call)
        self.assertIn("destroyGroundProxy(ship);", call)
        self.assertNotIn("groundProxy = ensureGroundProxy", call)
        self.assertIn('messageTo(ship, "playAtmosphericArrivalEffect", arrivalParams, 0.25f, false);', call)
        arrival_script = (
            TOOLS_ROOT.parent
            / "pre-cu-reborn-server-x64"
            / "dsrc/sku.0/sys.server/compiled/game/script/space/ship/atmospheric_ship.java"
        ).read_text(encoding="utf-8")
        self.assertIn('playClientEffectObj(player, "clienteffect/space_command/shp_dock_release.cef", self, "");', arrival_script)
        self.assertIn("sendSystemMessage(player, SID_CALLING_FOR_PICKUP);", call)
        self.assertIn('messageTo(controlDevice, "completeAtmosphericShipCall", params, ITV_CALL_DELAY_SECONDS, false);', call)
        self.assertIn("public static boolean completeCallDown", call)
        self.assertLess(call.index("setLocation(ship, callLocation)"), call.index("setControlState(controlDevice, STATE_ACTIVE)"))
        for generic_itv_proxy in (
            "terminal_travel_instant_xwing",
            "terminal_travel_instant_tie",
            "terminal_travel_instant_royal_ship",
            "terminal_travel_instant_jalopy",
        ):
            self.assertNotIn(generic_itv_proxy, call)
        active_monitor = arrival_script[
            arrival_script.index("public int monitorAtmosphericFlight") :
            arrival_script.index("public int playAtmosphericArrivalEffect")
        ]
        self.assertIn("destroyGroundProxy(self);", active_monitor)
        self.assertNotIn("ensureGroundProxy", active_monitor)
        self.assertIn("pilot(player, interactionShip)", arrival_script)
        self.assertIn("enter(player, interactionShip)", arrival_script)
        self.assertIn("destroyGroundProxy(ship);", library)

        placement_start = library.index("public static float getCallFootprint")
        placement_end = library.index("public static transform getCallTransform", placement_start)
        placement = library[placement_start:placement_end]
        self.assertIn("getObjectCollisionRadius(ship)", placement)
        self.assertIn("float playerRadius = Math.max(0.0f, getObjectCollisionRadius(player));", placement)
        self.assertIn("Math.max(CALL_DISTANCE, footprint + playerRadius + CALL_SEARCH_PADDING)", placement)
        self.assertIn("locations.getGoodLocationAroundLocationAvoidCollidables", placement)
        self.assertIn("false, false, footprint", placement)

    def test_ground_store_lifts_ship_before_applying_containment(self) -> None:
        controller = read_tools(
            "src/engine/client/library/clientGame/src/shared/controller/RemoteShipController.cpp"
        )
        controller_header = read_tools(
            "src/engine/client/library/clientGame/src/shared/controller/RemoteShipController.h"
        )
        ship = read_tools(
            "src/engine/client/library/clientGame/src/shared/object/ShipObject.cpp"
        )
        scene = read_tools(
            "src/engine/client/library/clientGame/src/shared/scene/GroundScene.cpp"
        )

        self.assertIn("cms_atmosphericDepartureHeight = 160.0f", controller)
        self.assertIn("cms_atmosphericDepartureTimeSeconds = 4.0f", controller)
        self.assertIn("void RemoteShipController::leaveByAtmosphere()", controller)
        self.assertIn("m_isInAtmosphericDeparture", controller_header)
        departure = controller[
            controller.index("else if (m_isInAtmosphericDeparture)") :
            controller.index("else\n", controller.index("else if (m_isInAtmosphericDeparture)") + 1)
        ]
        self.assertIn("cms_atmosphericDepartureHeight * easedFraction", departure)
        self.assertIn("bool ShipObject::onLeaveByAtmosphere()", ship)

        containment_start = scene.index('else if(message.isType("UpdateContainmentMessage"))')
        containment_end = scene.index("else if(message.isType", containment_start + 1)
        containment = scene[containment_start:containment_end]
        self.assertIn("beginAtmosphericShipDeparture", containment)
        self.assertIn("atmosphericShipToStore->onLeaveByAtmosphere()", containment)
        self.assertIn("ms_pendingAtmosphericShipContainments[o.getNetworkId()]", containment)
        self.assertLess(
            containment.index("atmosphericShipToStore->onLeaveByAtmosphere()"),
            containment.index("target->updateContainment(o.getContainerId(), o.getSlotArrangement());"),
        )
        self.assertIn("updatePendingAtmosphericShipContainments(elapsedTime);", scene)
        pending = scene[
            scene.index("void GroundSceneNamespace::updatePendingAtmosphericShipContainments") :
            scene.index("//-----------------------------------------------------------------", scene.index("void GroundSceneNamespace::updatePendingAtmosphericShipContainments") + 1)
        ]
        self.assertIn("ship->updateContainment(current->second.containerId, current->second.arrangement);", pending)

    def test_stored_ship_state_self_heals_only_for_matching_nonparked_pcd(self) -> None:
        library = (
            TOOLS_ROOT.parent
            / "pre-cu-reborn-server-x64"
            / "dsrc/sku.0/sys.server/compiled/game/script/library/atmospheric_ship.java"
        ).read_text(encoding="utf-8")
        start = library.index("public static void normalizeStoredShipState")
        end = library.index("public static int getControlState", start)
        repair = library[start:end]
        self.assertIn("!ship.isLoaded()", repair)
        self.assertIn("getContainedBy(ship) != controlDevice || isParkedHousing(ship)", repair)
        for objvar in (
            "VAR_ACTIVE",
            "VAR_WORLD_VISIBLE",
            "VAR_GROUND_LOCATION",
            "VAR_TRANSITION_PENDING",
        ):
            self.assertIn(f"removeObjVar(ship, {objvar});", repair)
        self.assertIn("setControlState(controlDevice, STATE_STORED);", repair)
        self.assertLess(
            repair.index("getContainedBy(ship) != controlDevice || isParkedHousing(ship)"),
            repair.index("removeObjVar(ship, VAR_ACTIVE);"),
        )

    def test_saved_vehicle_and_mount_palette_is_private_preview_data(self) -> None:
        server = read_server(
            "engine/server/library/serverGame/src/shared/object/IntangibleObject.cpp"
        )
        icon = read_tools(
            "src/engine/client/library/clientUserInterface/src/shared/core/"
            "CuiIconManager.cpp"
        )
        attributes = read_tools(
            "src/engine/client/library/clientGame/src/shared/core/"
            "ObjectAttributeManager.cpp"
        )
        key = "gr_internal_preview_customization"
        self.assertIn('NestedList const paletteVariables(getObjVars(), "ai.pet.palvar.vars")', server)
        for objvar in ("creature_attribs.hue", "beast.hue", "beast.hue2", "beast.hue3"):
            self.assertIn(f'appendHueObjVar("{objvar}"', server)
        self.assertIn(key, server)
        self.assertIn(key, icon)
        self.assertIn("appearance->addCustomizationVariables", icon)
        self.assertIn("rangedVariable->setValue(value)", icon)
        self.assertIn(f'fullKey == "{key}"', attributes)
        self.assertIn('fullKey == "gr_internal_ship_preview_id"', attributes)
        self.assertIn('fullKey == "gr_internal_ship_preview_template"', attributes)
        self.assertIn('fullKey == "gr_internal_ship_preview_customization"', attributes)

    def test_scope_is_x64_tree_only(self) -> None:
        for root in (TOOLS_ROOT, SERVER_ROOT):
            self.assertIn("x64", root.name if root == TOOLS_ROOT else str(root.parent))


if __name__ == "__main__":
    unittest.main()
