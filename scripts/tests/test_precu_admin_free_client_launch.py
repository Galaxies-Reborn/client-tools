from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
REGISTRY_KEY = (
    ROOT
    / "src/engine/shared/library/sharedFoundation/src/win32/RegistryKey.cpp"
)


class AdminFreeClientLaunchTests(unittest.TestCase):
    def test_startup_opens_machine_registry_without_creating_it(self) -> None:
        source = REGISTRY_KEY.read_text(encoding="utf-8")
        install = source[
            source.index("void RegistryKey::install") :
            source.index("void RegistryKey::remove", source.index("void RegistryKey::install"))
        ]

        self.assertIn("RegOpenKeyEx(", install)
        self.assertIn("HKEY_LOCAL_MACHINE", install)
        self.assertIn("KEY_READ", install)
        self.assertNotIn("localMachineKey->createSubkey", install)

    def test_missing_machine_key_falls_back_to_current_user(self) -> None:
        source = REGISTRY_KEY.read_text(encoding="utf-8")
        install = source[
            source.index("void RegistryKey::install") :
            source.index("void RegistryKey::remove", source.index("void RegistryKey::install"))
        ]

        self.assertIn("machineProductKeyResult == ERROR_SUCCESS", install)
        self.assertIn(
            "productMachineKey = currentUserKey->openSubkey(useRegistryPath, AF_READ);",
            install,
        )


if __name__ == "__main__":
    unittest.main()
