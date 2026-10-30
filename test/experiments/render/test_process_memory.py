"""Darwin bytes, Linux KiB and explicit rejection of unknown unit assumptions."""

import json
import subprocess
import sys
import unittest
from types import SimpleNamespace
from unittest.mock import patch

from process_memory import linux_peak_rss_bytes, peak_rss_bytes, rss_bytes


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

    def test_linux_current_executable_peak_comes_from_vmhwm(self):
        with (
            patch("process_memory.sys.platform", "linux"),
            patch(
                "process_memory.Path.read_text",
                return_value="VmRSS:\t100 kB\nVmHWM:\t250 kB\n",
            ),
            patch(
                "process_memory.resource.getrusage",
                side_effect=AssertionError("inherited peak"),
            ),
        ):
            self.assertEqual(peak_rss_bytes(), 256000)
        for status in ("VmRSS: 100 kB", "VmHWM: -1 kB", "VmHWM: 100 MB"):
            with self.assertRaisesRegex(ValueError, "unavailable"):
                linux_peak_rss_bytes(status)

    @unittest.skipUnless(sys.platform.startswith("linux"), "Linux exec/VmHWM contract")
    def test_exec_does_not_import_the_preceding_images_peak(self):
        after = (
            "import json,resource;from process_memory import peak_rss_bytes;"
            "print(json.dumps([peak_rss_bytes(),resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*1024]))"
        )
        before = (
            "import os,sys;previous=bytearray(32*1024*1024);"
            f"os.execv(sys.executable,[sys.executable,'-B','-c',{after!r}])"
        )
        current, inherited = json.loads(
            subprocess.check_output([sys.executable, "-B", "-c", before], text=True)
        )
        self.assertLess(current + 16 * 1024 * 1024, inherited)
