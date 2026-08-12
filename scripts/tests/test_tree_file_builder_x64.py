import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
BUILDER_SOURCE = REPOSITORY_ROOT / (
    "src/engine/shared/application/TreeFileBuilder/src/shared/TreeFileBuilder.cpp"
)
BUILDER_PROJECT = REPOSITORY_ROOT / (
    "src/engine/shared/application/TreeFileBuilder/build/win32/"
    "TreeFileBuilder.vcxproj"
)
BUNDLED_BUILDER = REPOSITORY_ROOT / "tools" / "TreeFileBuilder.exe"


def read_pe_machine(path: Path) -> int:
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise AssertionError(f"not a PE file: {path}")
    pe_offset = int.from_bytes(data[0x3C:0x40], "little")
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise AssertionError(f"invalid PE signature: {path}")
    return int.from_bytes(data[pe_offset + 4 : pe_offset + 6], "little")


class TreeFileBuilderX64Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = BUILDER_SOURCE.read_text(encoding="utf-8")
        cls.project = BUILDER_PROJECT.read_text(encoding="utf-8")
        match = re.search(
            r'<ItemDefinitionGroup '
            r'Condition="\'\$\(Configuration\)\|\$\(Platform\)\'=='
            r'\'Release\|x64\'">(.*?)</ItemDefinitionGroup>',
            cls.project,
            re.DOTALL,
        )
        if not match:
            raise AssertionError("Release|x64 TreeFileBuilder configuration is missing")
        cls.release_x64 = match.group(1)

    def test_x64_uses_a_pointer_safe_argument_parser(self):
        self.assertIn("#if defined(_WIN64)", self.source)
        self.assertIn("parseWin64Options(", self.source)
        self.assertIn('!strncmp(argument, "-r=", 3)', self.source)
        self.assertIn("applicationArgv", self.source)

    def test_builder_owns_its_zlib_compressor(self):
        self.assertIn(
            '#include "sharedCompression/ZlibCompressor.h"', self.source
        )
        self.assertIn("new ZlibCompressor()", self.source)
        self.assertNotIn("borrowCompressor(", self.source)
        self.assertNotIn("returnCompressor(", self.source)

    def test_release_x64_links_only_x64_runtime_libraries(self):
        dependencies = (
            "archive.lib;unicodeArchive.lib;sharedMessageDispatch.lib;"
            "sharedLog.lib;sharedNetworkMessages.lib;zlib.lib"
        )
        self.assertIn(dependencies, self.release_x64)
        self.assertIn(
            r"..\..\..\..\..\..\build\win32\x64\Release",
            self.release_x64,
        )
        self.assertNotIn(
            r"external\3rd\library\zlib\lib\win32",
            self.release_x64,
        )

    def test_bundled_archive_builder_is_amd64(self):
        self.assertEqual(read_pe_machine(BUNDLED_BUILDER), 0x8664)


if __name__ == "__main__":
    unittest.main()
