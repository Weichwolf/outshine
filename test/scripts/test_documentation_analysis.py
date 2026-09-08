#!/usr/bin/env python3
"""Exercise real Doxygen output, input coverage and failed-tool negative controls."""
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

from documentation_analysis import ROOT, analyse

DOCUMENTED = '''/** @file */
/** A value returned by the fixture. */
struct Value {
  /** Number of stored items. */
  int Count;
};
'''


class Documentation(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='outshine-documentation-test-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.report = self.root / 'report'
        (self.root / 'doc').mkdir()
        (self.root / 'doc/Doxyfile').write_bytes((ROOT / 'doc/Doxyfile').read_bytes())
        self.header = self.write('include/with space/Value.h', DOCUMENTED)
        self.tool = shutil.which('doxygen')
        self.assertIsNotNone(self.tool, 'the real Doxygen is required; this test cannot skip')

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def wrapper(self, body):
        path = self.write('tool', f'#!{sys.executable}\n' + body)
        path.chmod(0o755)
        return str(path)

    def run_analysis(self, tool=None, timeout=10):
        with redirect_stdout(io.StringIO()):
            status = analyse(self.root, self.report, tool or self.tool, timeout)
        return status, json.loads((self.report / 'doxygen.json').read_text())

    def test_real_clean_documentation_and_public_header_coverage(self):
        self.write('src/client/Internal.h', 'struct Undocumented {};\n')
        self.write('include/Empty.hpp', '')
        status, summary = self.run_analysis()
        self.assertEqual(status, 0, summary)
        self.assertTrue(summary['complete'])
        self.assertEqual(len(summary['parsed']), 2)
        self.assertEqual(summary['expected'], summary['parsed'])
        self.assertEqual(summary['diagnostics'], 0)

    def test_undocumented_member_and_enum_value_are_diagnostics(self):
        self.header.write_text(DOCUMENTED.replace('/** Number of stored items. */', '') +
                               '/** Supported mode. */\nenum class Mode { Missing };\n')
        status, summary = self.run_analysis()
        self.assertEqual(status, 1, summary)
        self.assertTrue(summary['complete'])
        self.assertGreaterEqual(summary['diagnostics'], 2)
        log = (self.report / 'doxygen.log').read_text()
        self.assertIn('Count', log)
        self.assertIn('Missing', log)

    def test_missing_tool_and_empty_input_are_incomplete(self):
        status, summary = self.run_analysis(tool=str(self.root / 'missing'))
        self.assertEqual(status, 2)
        self.assertFalse(summary['complete'])
        self.header.unlink()
        status, summary = self.run_analysis()
        self.assertEqual(status, 2)
        self.assertIn('coverage is empty', summary['error'])

    def test_failure_after_real_output_is_not_clean(self):
        tool = self.wrapper('import subprocess, sys\n'
                            f'subprocess.run([{self.tool!r}, "-"], '
                            'input=sys.stdin.read(), text=True, check=True)\n'
                            'sys.exit(7)\n')
        status, summary = self.run_analysis(tool)
        self.assertEqual(status, 2)
        self.assertEqual(summary['returncode'], 7)
        self.assertFalse(summary['complete'])

    def test_success_without_output_cannot_reuse_previous_artifacts(self):
        self.assertEqual(self.run_analysis()[0], 0)
        status, summary = self.run_analysis(self.wrapper('pass\n'))
        self.assertEqual(status, 2)
        self.assertFalse(summary['complete'])

    def test_timeout_and_signal_are_incomplete(self):
        for body, timeout in [('import time\ntime.sleep(1)\n', 0.05),
                              ('import os, signal\nos.kill(os.getpid(), signal.SIGTERM)\n', 10)]:
            with self.subTest(body=body):
                status, summary = self.run_analysis(self.wrapper(body), timeout)
                self.assertEqual(status, 2)
                self.assertFalse(summary['complete'])

    def test_real_output_with_excluded_header_is_incomplete(self):
        self.write('include/Other.h', '/** @file */\n')
        override = '\nINPUT = "include/with space/Value.h"\n'
        tool = self.wrapper('import subprocess, sys\n'
                            f'config = sys.stdin.read() + {override!r}\n'
                            f'sys.exit(subprocess.run([{self.tool!r}, "-"], '
                            'input=config, text=True).returncode)\n')
        status, summary = self.run_analysis(tool)
        self.assertEqual(status, 2)
        self.assertFalse(summary['complete'])
        self.assertIn('header coverage differs', summary['error'])


if __name__ == '__main__':
    unittest.main()
