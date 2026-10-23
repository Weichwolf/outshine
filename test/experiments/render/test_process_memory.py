"""Darwin bytes, Linux KiB and explicit rejection of unknown unit assumptions."""

import unittest
from types import SimpleNamespace
from unittest.mock import patch

from process_memory import peak_rss_bytes, rss_bytes


class MemoryUnitsTest(unittest.TestCase):
    def test_darwin_raw_value_already_is_bytes(self):
        self.assertEqual(rss_bytes(67108864, "darwin"), 67108864)

    def test_linux_raw_value_is_kibibytes(self):
        self.assertEqual(rss_bytes(65536, "linux"), 67108864)

    def test_other_platform_is_not_silently_assumed_to_be_linux(self):
        with self.assertRaisesRegex(ValueError, "not qualified"):
            rss_bytes(65536, "freebsd")

    def test_current_platform_is_used_by_the_os_read(self):
        with (
            patch("process_memory.sys.platform", "darwin"),
            patch(
                "process_memory.resource.getrusage",
                return_value=SimpleNamespace(ru_maxrss=67108864),
            ),
        ):
            self.assertEqual(peak_rss_bytes(), 67108864)
