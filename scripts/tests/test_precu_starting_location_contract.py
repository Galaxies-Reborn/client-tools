from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HUD_MANAGER_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiHudWindowManager.cpp"
)
MEDIATOR_FACTORY_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/core/"
    "SwgCuiMediatorFactorySetup.cpp"
)
AVATAR_LOCATION_CPP = ROOT / (
    "src/game/client/library/swgClientUserInterface/src/shared/page/"
    "SwgCuiAvatarLocation2.cpp"
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


class PrecuStartingLocationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.hud_manager_cpp = HUD_MANAGER_CPP.read_text(encoding="utf-8")
        cls.mediator_factory_cpp = MEDIATOR_FACTORY_CPP.read_text(encoding="utf-8")
        cls.avatar_location_cpp = AVATAR_LOCATION_CPP.read_text(encoding="utf-8")

    def test_starting_locations_activate_the_retained_avatar_location_mediator(self) -> None:
        received = function_body(
            self.hud_manager_cpp,
            "void SwgCuiHudWindowManager::onStartingLocationsReceived",
        )

        activation = (
            "CuiMediatorFactory::activate   "
            "(CuiMediatorTypes::AvatarLocation2)"
        )
        self.assertIn(activation, received)
        self.assertIn("SwgCuiHudFactory::setHudActive(false);", received)
        self.assertEqual(received.count("CuiMediatorFactory::activate"), 1)

    def test_server_location_payload_is_passed_to_avatar_location2(self) -> None:
        received = function_body(
            self.hud_manager_cpp,
            "void SwgCuiHudWindowManager::onStartingLocationsReceived",
        )
        set_locations = function_body(
            self.avatar_location_cpp,
            "void SwgCuiAvatarLocation2::setLocations",
        )

        self.assertIn("avloc2->setLocations (locations);", received)
        self.assertLess(
            received.index("CuiMediatorFactory::activate"),
            received.index("avloc2->setLocations (locations);"),
        )
        self.assertIn("*m_locationStatusVector = lsv;", set_locations)
        self.assertIn("setupPlanets ();", set_locations)

    def test_avatar_location2_is_registered_at_publish14_page_path(self) -> None:
        registrations = re.findall(
            r"MAKE_SWG_CTOR\s*\(\s*AvatarLocation2\s*,\s*\"([^\"]+)\"\s*\)",
            self.mediator_factory_cpp,
        )
        self.assertEqual(["/AvLoc2"], registrations)

    def test_ok_queues_selected_location_with_the_retained_command(self) -> None:
        ok = function_body(
            self.avatar_location_cpp,
            "void SwgCuiAvatarLocation2::ok ()",
        )

        self.assertIn("if (m_selectedLocation.empty ())", ok)
        self.assertIn("if (!m_selectedLocationAvailable)", ok)
        self.assertIn(
            'Crc::normalizeAndCalculate("newbieSelectStartingLocation")', ok
        )
        self.assertIn(
            "const Unicode::String & params = "
            "Unicode::narrowToWide (m_selectedLocation);",
            ok,
        )
        enqueue = (
            "ClientCommandQueue::enqueueCommand "
            "(newbieSelectStartingLocation, NetworkId::cms_invalid, params);"
        )
        self.assertEqual(1, ok.count(enqueue))
        self.assertLess(ok.index("Unicode::narrowToWide (m_selectedLocation)"), ok.index(enqueue))

    def test_starting_location_handoff_does_not_route_through_nge_creation_ui(self) -> None:
        received = function_body(
            self.hud_manager_cpp,
            "void SwgCuiHudWindowManager::onStartingLocationsReceived",
        )
        ok = function_body(
            self.avatar_location_cpp,
            "void SwgCuiAvatarLocation2::ok ()",
        )
        handoff = received + ok

        for forbidden_route in (
            "AvatarSummary",
            "AvatarCreation",
            "AvatarSetupProf",
            "AvatarProfessionTemplateSelect",
            "SwgCuiAvatarCreationHelper",
        ):
            with self.subTest(forbidden_route=forbidden_route):
                self.assertNotIn(forbidden_route, handoff)


if __name__ == "__main__":
    unittest.main()
