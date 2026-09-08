#!/usr/bin/env python3
"""Integration checks use the actual analyser plus explicit failure injection."""
import json
import os
import shlex
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / 'test/scripts/tidy_analysis.py'
TOOL = Path(os.environ.get('LLVM_BIN', '/opt/homebrew/opt/llvm/bin')) / 'clang-tidy'


class TidyExecution(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='outshine-tidy-oracle-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'src/client').mkdir(parents=True)
        (self.root / '.clang-tidy').write_text(json.dumps({
            'Checks': '-*,bugprone-integer-division', 'WarningsAsErrors': '',
            'HeaderFilterRegex': '.*'}))
        self.main = self.root / 'src/client/Main.cpp'
        self.main.write_text('int main() { return 0; }\n')
        self.database([self.main])

    def database(self, paths):
        template = json.loads((ROOT / 'compile_commands.json').read_text())[0]
        source = Path(template['file'])
        if not source.is_absolute():
            source = Path(template['directory']) / source
        original = template.get('arguments') or shlex.split(template['command'])
        records = []
        for path in paths:
            args = [str(path) if Path(arg) == source else arg for arg in original]
            records.append({'directory': template['directory'], 'file': str(path),
                            'arguments': args})
        (self.root / 'compile_commands.json').write_text(json.dumps(records))

    def run_analysis(self, expected, tool=TOOL, timeout=30):
        report = self.root / 'report'
        run = subprocess.run([sys.executable, str(RUNNER), '--root', str(self.root),
                              '--report', str(report), '--tool', str(tool), '--jobs', '2',
                              '--timeout', str(timeout)], capture_output=True, text=True, timeout=45)
        self.assertEqual(run.returncode, expected, run.stdout + run.stderr)
        return json.loads((report / 'tidy.execution.json').read_text())

    def test_real_clean_analysis_can_be_zero(self):
        summary = self.run_analysis(0)
        self.assertTrue(summary['complete'])
        self.assertEqual(summary['findings'], 0)
        self.assertEqual(len(summary['units']), 1)
        self.assertEqual(summary['units'][0]['file'], str(self.main.resolve()))

    def test_real_warning_is_red(self):
        self.main.write_text('double division(int a, int b) { return a / b; }\n')
        summary = self.run_analysis(1)
        self.assertTrue(summary['complete'])
        self.assertGreater(summary['findings'], 0)

    def test_parse_failure_cannot_hide_beside_warning(self):
        bad = self.root / 'src/Broken.cpp'
        bad.write_text('int broken( {\n')
        self.main.write_text('double division(int a, int b) { return a / b; }\n')
        self.database([self.main, bad])
        summary = self.run_analysis(2)
        self.assertFalse(summary['complete'])
        self.assertEqual(len(summary['units']), 2)
        self.assertGreater(summary['findings'], 0)

    def test_missing_entry_point_and_empty_database_are_red(self):
        for paths in ([], [self.root / 'Ghost.cpp']):
            with self.subTest(paths=paths):
                self.database(paths)
                self.assertFalse(self.run_analysis(2)['complete'])

    def test_duplicate_configuration_and_missing_tool(self):
        self.database([self.main, self.main])
        self.assertFalse(self.run_analysis(2)['complete'])
        self.database([self.main])
        self.assertFalse(self.run_analysis(2, self.root / 'missing-tool')['complete'])

    def test_process_failure_and_timeout_are_not_zero(self):
        fake = self.root / 'failing-tool'
        for statement, timeout in [('raise SystemExit(3)', 30), ('time.sleep(5)', 0.1)]:
            with self.subTest(statement=statement):
                fake.write_text(f'#!{sys.executable}\nimport sys, time\n'
                                'if "--version" in sys.argv: print("injected tool"); sys.exit(0)\n'
                                + statement + '\n')
                fake.chmod(0o755)
                summary = self.run_analysis(2, fake, timeout)
                self.assertFalse(summary['complete'])
                self.assertEqual(len(summary['units']), 1)


if __name__ == '__main__':
    unittest.main()
