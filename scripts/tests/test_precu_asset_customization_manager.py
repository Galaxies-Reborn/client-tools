from pathlib import Path
import unittest


SOURCE = (
    Path(__file__).parents[2]
    / "src"
    / "engine"
    / "shared"
    / "library"
    / "sharedGame"
    / "src"
    / "shared"
    / "core"
    / "AssetCustomizationManager.cpp"
)


class PreCuAssetCustomizationManagerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_ucmp_is_read_as_raw_bytes_before_layout_detection(self) -> None:
        self.assertIn("int const variableUsageDataSize = iff.getChunkLengthLeft();", self.source)
        self.assertIn("iff.read_uint8(variableUsageDataSize, variableUsageData);", self.source)
        self.assertNotIn(
            "iff.read_uint16(3 * s_maxValidVariableUsageId",
            self.source,
        )

    def test_compact_precu_and_wide_nge_layouts_are_both_validated(self) -> None:
        self.assertIn("cs_compactVariableUsageComponentSize", self.source)
        self.assertIn("cs_wideVariableUsageComponentSize", self.source)
        self.assertIn("compactEncodingValid", self.source)
        self.assertIn("wideEncodingValid", self.source)

    def test_layout_validation_is_bounded_by_loaded_id_tables(self) -> None:
        for maximum in (
            "s_maxValidVariableId",
            "s_maxValidRangeId",
            "s_maxValidDefaultId",
        ):
            self.assertIn(maximum, self.source)
        self.assertIn("(id < 1) || (id > maximumIds[componentIndex])", self.source)

    def test_wide_layout_remains_preferred_when_both_are_valid(self) -> None:
        self.assertIn(
            "int const componentSize = wideEncodingValid ? "
            "cs_wideVariableUsageComponentSize : "
            "cs_compactVariableUsageComponentSize;",
            self.source,
        )


if __name__ == "__main__":
    unittest.main()
