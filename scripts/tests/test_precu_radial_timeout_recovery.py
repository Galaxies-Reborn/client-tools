import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
RADIAL_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientUserInterface/src/shared/core/"
    "CuiRadialMenuManager.cpp"
)


class PreCuRadialTimeoutRecoveryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = RADIAL_SOURCE.read_text(encoding="utf-8")

    def test_unanswered_snapshot_object_requests_have_a_bounded_retry_budget(self):
        self.assertIn("int retryCount;", self.source)
        self.assertIn("int const s_maxResponseRetries = 1;", self.source)
        self.assertIn("responseInfo.retryCount = 0;", self.source)
        self.assertIn("entry.retryCount < s_maxResponseRetries", self.source)
        self.assertIn("++entry.retryCount;", self.source)

    def test_exhausted_request_releases_cache_menu_and_notification_state(self):
        update = re.search(
            r"void CuiRadialMenuManager::update\(\)\s*\{(.*?)\n\}",
            self.source,
            re.DOTALL,
        )
        self.assertIsNotNone(update)
        body = update.group(1)
        self.assertIn("abandoning unanswered request", body)
        self.assertIn("cacheData.responsePending = false;", body)
        self.assertIn("cacheData.ok = false;", body)
        self.assertIn("s_pendingServerNotifications.erase", body)
        self.assertIn("clear();", body)
        self.assertIn("s_pendingResponses.erase", body)


if __name__ == "__main__":
    unittest.main()
