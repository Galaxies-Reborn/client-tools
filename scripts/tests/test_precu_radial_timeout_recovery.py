import re
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
RADIAL_SOURCE = REPOSITORY_ROOT / (
    "src/engine/client/library/clientUserInterface/src/shared/core/"
    "CuiRadialMenuManager.cpp"
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

    def test_popup_clear_removes_the_popup_context_widget(self):
        clear = function_body(
            self.source, "void CuiRadialMenuManager::clear ()"
        )
        popup_branch = clear[clear.index("if (ms_popup)") :]
        self.assertIn("PopContextWidgets (ms_popup)", popup_branch)
        self.assertNotIn("PopContextWidgets (ms_radial)", popup_branch)


if __name__ == "__main__":
    unittest.main()
