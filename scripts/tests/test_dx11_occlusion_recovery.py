import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
SWAP_CHAIN_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/application/Direct3d11/src/win32/"
    "Direct3d11_SwapChain.cpp"
)


def function_source(source, signature):
    start = source.index(signature)
    end = source.index("// ----------------------------------------------------------------------", start)
    return source[start:end]


class Direct3d11OcclusionRecoveryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SWAP_CHAIN_SOURCE.read_text(encoding="utf-8")
        cls.probe = function_source(
            cls.source,
            "bool Direct3d11_SwapChainNamespace::probeOcclusion(bool &visible)",
        )
        cls.present = function_source(
            cls.source,
            "bool Direct3d11_SwapChain::present()",
        )

    def test_occlusion_is_retained_and_polled_at_a_low_rate(self):
        interval = re.search(
            r"cms_occlusionProbeIntervalMilliseconds\s*=\s*(\d+)",
            self.source,
        )
        self.assertIsNotNone(interval)
        self.assertGreaterEqual(int(interval.group(1)), 50)
        self.assertLessEqual(int(interval.group(1)), 500)
        self.assertIn("if (now < ms_nextOcclusionProbeTick)", self.probe)
        self.assertIn(
            "ms_nextOcclusionProbeTick = now + "
            "cms_occlusionProbeIntervalMilliseconds;",
            self.probe,
        )
        self.assertNotIn("Sleep(", self.probe)

        occluded = self.present.index("if (hresult == DXGI_STATUS_OCCLUDED)")
        retained = self.present.index("ms_occluded = true;", occluded)
        scheduled = self.present.index(
            "ms_nextOcclusionProbeTick = GetTickCount64() + "
            "cms_occlusionProbeIntervalMilliseconds;",
            retained,
        )
        self.assertLess(occluded, retained)
        self.assertLess(retained, scheduled)

    def test_occluded_frames_skip_composite_until_test_present_succeeds(self):
        window_gate = self.present.index(
            "if (IsIconic(ms_window) || !IsWindowVisible(ms_window))"
        )
        gate = self.present.index("if (ms_occluded)")
        composite = self.present.index(
            "Direct3d11_SceneTarget::composite();",
            gate,
        )
        self.assertLess(window_gate, gate)
        self.assertLess(gate, composite)
        window_block = self.present[window_gate:gate]
        self.assertIn("ms_occluded = true;", window_block)
        self.assertIn("return true;", window_block)
        self.assertIn("if (!visible)\n\t\t\treturn true;", self.present)
        self.assertIn(
            "Present(0, DXGI_PRESENT_TEST | DXGI_PRESENT_DO_NOT_WAIT)",
            self.probe,
        )

        success_start = self.probe.index("if (hresult == S_OK)")
        success_end = self.probe.index(
            "if (hresult == DXGI_STATUS_OCCLUDED", success_start
        )
        success = self.probe[success_start:success_end]
        self.assertIn("ms_occluded = false;", success)
        self.assertIn("visible = true;", success)

    def test_probe_classifies_all_expected_dxgi_results(self):
        self.assertIn(
            'checkForDeviceRemoved(hresult, "Present occlusion probe")',
            self.probe,
        )
        self.assertIn(
            "hresult == DXGI_STATUS_OCCLUDED || "
            "hresult == DXGI_ERROR_WAS_STILL_DRAWING",
            self.probe,
        )
        self.assertIn("if (FAILED(hresult))", self.probe)
        self.assertIn("++Direct3d11_Metrics::presentFailures;", self.probe)
        self.assertNotIn("++Direct3d11_Metrics::presents;", self.probe)

    def test_normal_present_is_nonblocking_without_disabling_vsync(self):
        self.assertIn(
            "UINT const syncInterval = "
            "(ms_swapChainFlags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) ? 0 : 1;",
            self.present,
        )
        self.assertIn("DXGI_PRESENT_DO_NOT_WAIT", self.present)
        self.assertIn("DXGI_PRESENT_ALLOW_TEARING", self.present)
        self.assertIn(
            "Present(syncInterval, presentFlags)",
            self.present,
        )

        skipped = self.present.index(
            "if (hresult == DXGI_ERROR_WAS_STILL_DRAWING)"
        )
        failed = self.present.index("if (FAILED(hresult))", skipped)
        self.assertLess(skipped, failed)
        skipped_block = self.present[skipped:failed]
        self.assertIn("return true;", skipped_block)
        self.assertNotIn("presentFailures", skipped_block)


if __name__ == "__main__":
    unittest.main()
