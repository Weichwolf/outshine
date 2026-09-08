#!/usr/bin/env python3
"""Exercise repository coverage and real clang-format outcomes in isolated checkouts."""
from contextlib import redirect_stderr, redirect_stdout
import io
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from format_sources import ROOT, format_sources, sources


class Formatting(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='outshine-format-test-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        subprocess.run(['git', 'init', '-q', self.root], check=True)
        (self.root / '.clang-format').write_bytes((ROOT / '.clang-format').read_bytes())
        self.tool = str(Path(os.environ['LLVM_BIN']) / 'clang-format')

    def write(self, name, text='int main(){return 0;}\n'):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def run_format(self, check=True, tool=None):
        output = io.StringIO()
        with redirect_stdout(output), redirect_stderr(output):
            result = format_sources(self.root, tool or self.tool, check)
        return result, output.getvalue()

    def test_real_diagnostics_count_files_and_fix_is_idempotent(self):
        path = self.write('src/with space.cpp')
        self.write('include/value.h', 'int value();\n')
        status, output = self.run_format()
        self.assertEqual(status, 1)
        self.assertIn('2 owned C++ files checked; 1 failed', output)
        self.assertIn('error:', output)
        self.assertEqual(self.run_format(False)[0], 0)
        self.assertEqual(path.read_text(), 'int main() {\n  return 0;\n}\n')
        before = path.read_bytes()
        self.assertEqual(self.run_format()[0], 0)
        self.assertEqual(self.run_format(False)[0], 0)
        self.assertEqual(path.read_bytes(), before)

    def test_owned_population_excludes_vendor_ignored_deleted_and_shader_files(self):
        (self.root / '.gitignore').write_text('test/.venv/\nsrc/ignored.cpp\n')
        wanted = ['src/main.cpp', 'include/public.h', 'test/check.cpp']
        for name in wanted + ['test/.venv/vendor.h', 'src/ignored.cpp',
                              'src/render/shaders/generated.h', 'elsewhere/foreign.cpp']:
            self.write(name)
        deleted = self.write('src/deleted.cpp')
        subprocess.run(['git', 'add', 'src/deleted.cpp'], cwd=self.root, check=True)
        deleted.unlink()
        self.assertEqual(set(map(str, sources(self.root))), set(wanted))

    def test_empty_coverage_is_an_error(self):
        self.assertEqual(self.run_format()[0], 2)

    def test_missing_tool_is_an_error(self):
        self.write('src/main.cpp')
        self.assertEqual(self.run_format(tool=str(self.root / 'missing-tool'))[0], 2)


if __name__ == '__main__':
    unittest.main()
