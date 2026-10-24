"""Check diagnostic bytes and explicit/standalone test artifact roots."""

import os
import json
import shlex
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
from cmake_context import compile_flags

STANDARD_FLAGS = compile_flags()


class CheckHarness(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="outshine-check-harness-")
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.root = Path(cls.temporary.name)
        cls.source = cls.root / "control.cpp"
        cls.source.write_text("""#include "Check.h"
#include <string_view>
int main(int argc, char **argv) {
  using namespace outshine::Test;
  const std::string_view mode = argc > 1 ? argv[1] : "pass";
  if (mode == "path") { std::println("{}", PlantedPath("asset.sqlite")); return 0; }
  if (mode == "pass") { Checked(true, "true", "claim", "control", 7); }
  if (mode == "fail") { Checked(false, "false", "claim", "control", 7); }
  if (mode == "partial") { Partial(0.5, "input"); }
  if (mode == "unprepared") { Unprepared("input"); }
  if (mode == "skip") { Skip("input"); }
  if (mode == "missing") { static_cast<void>(Sources("outshine-no-such-tree")); }
  return Report();
}
""")
        cls.compiler = shlex.split(os.environ.get("CXX", "c++"))
        cls.flags = STANDARD_FLAGS
        cls.plain = cls.build("standalone")

    @classmethod
    def build(cls, name, *defines):
        binary = cls.root / name
        subprocess.run(
            [
                *cls.compiler,
                *cls.flags,
                "-I" + str(ROOT / "test/harness/shared"),
                *defines,
                str(cls.source),
                "-o",
                str(binary),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        return binary

    def run_control(self, mode, binary=None, **environment):
        return subprocess.run(
            [str(binary or self.plain), mode],
            capture_output=True,
            text=True,
            env={**os.environ, **environment},
            check=False,
        )

    def test_diagnostics_and_statuses(self):
        cases = [
            ("pass", 0, "", "1 0 0 0 0"),
            ("fail", 1, "FAIL control:7  claim\n       CHECK(false)\n", "1 1 0 0 0"),
            ("partial", 0, "PARTIAL 0.500000 input\n", "0 0 0 0 1"),
            ("unprepared", 1, "UNPREPARED input\n", "0 0 0 1 0"),
            ("skip", 0, "SKIP input\n", "0 0 1 0 0"),
            ("empty", 1, "FAIL no claim was checked\n", "0 1 0 0 0"),
        ]
        labels = ["CHECKS", "FAILURES", "SKIPPED", "UNPREPARED", "PARTIAL"]
        for mode, status, prefix, counts in cases:
            with self.subTest(mode=mode):
                result = self.run_control(mode)
                trailer = (
                    " ".join(
                        f"{key} {value}" for key, value in zip(labels, counts.split())
                    )
                    + "\n"
                )
                self.assertEqual(
                    (result.returncode, result.stdout), (status, prefix + trailer)
                )
        missing = self.run_control("missing")
        self.assertEqual(missing.returncode, 1)
        self.assertTrue(
            missing.stdout.startswith("FAIL the tree 'outshine-no-such-tree'")
        )
        self.assertTrue(
            missing.stdout.endswith("FAILURES 2 SKIPPED 0 UNPREPARED 0 PARTIAL 0\n")
        )

    def test_declared_root_is_one_literal_argument(self):
        for name in ["ordinary", 'space quote" slash\\ dollar$ tick` grün']:
            with self.subTest(name=name):
                root = str(self.root / name)
                encoded = "-DOUTSHINE_TEST_NEST=" + json.dumps(root, ensure_ascii=False)
                binary = self.build("declared", encoded)
                result = self.run_control("path", binary, OUTSHINE_NEST="/ignored")
                self.assertEqual(
                    (result.returncode, result.stdout), (0, root + "/asset.sqlite\n")
                )

    def test_standalone_is_pid_scoped_and_rejects_missing_temp(self):
        root = self.root / "standalone root"
        root.mkdir()
        paths = [
            self.run_control("path", TMPDIR=str(root), OUTSHINE_NEST="/ignored")
            for _ in range(2)
        ]
        for result in paths:
            self.assertEqual(result.returncode, 0)
            self.assertRegex(result.stdout, r"/outshine-\d+-asset\.sqlite\n$")
            self.assertEqual(Path(result.stdout.strip()).parent, root)
        self.assertNotEqual(paths[0].stdout, paths[1].stdout)
        failure = self.run_control("path", TMPDIR=str(root / "missing"))
        self.assertNotEqual(failure.returncode, 0)
        self.assertTrue(
            failure.stderr.startswith("FAIL no system temporary directory:")
        )


if __name__ == "__main__":
    unittest.main()
