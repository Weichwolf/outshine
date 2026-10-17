"""Check archive edges using real compiler output, including local calls and ELF sections."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class CallgraphTests(unittest.TestCase):
    def compile_edges(self, flags, code=None):
        with tempfile.TemporaryDirectory(prefix='outshine-callgraph-',
                                         dir=os.environ.get('TMPDIR', '/tmp')) as directory:
            work = Path(directory)
            source = work / 'calls.cpp'
            source.write_text(code or '''extern "C" void blocking_leaf();
extern "C" {
static void local_step() { blocking_leaf(); }
void frame_root() { local_step(); }
}
''')
            compiler = shlex.split(os.environ.get('CXX', 'c++'))
            standard = next(line.removeprefix('CXXSTD=') for line in (ROOT / 'test/run.sh').read_text().splitlines()
                            if line.startswith('CXXSTD='))
            subprocess.run([*compiler, *shlex.split(standard), '-O0', '-fno-inline', *flags,
                            '-c', str(source), '-o', str(work / 'calls.o')], check=True)
            archive = work / 'calls.a'
            subprocess.run(['ar', 'rcs', str(archive), str(work / 'calls.o')], check=True)
            result = subprocess.run(['sh', str(ROOT / 'test/harness/shared/graph/callgraph.sh'),
                                     str(archive), str(work / 'walk')], check=True,
                                    capture_output=True, text=True)
            pairs = [line.split('\t') for line in result.stdout.splitlines()]
            self.assertTrue(pairs, 'a real archive must produce graph evidence')
            self.assertTrue(all(len(pair) == 2 for pair in pairs))
            symbols = sorted({name for pair in pairs for name in pair})
            demangled = subprocess.run(['c++filt'], input='\n'.join(symbols), text=True,
                                       capture_output=True, check=True).stdout.splitlines()
            names = {symbol: name.lstrip('_').split('(')[0]
                     for symbol, name in zip(symbols, demangled)}
            return {(names[caller], names[callee]) for caller, callee in pairs}

    def test_external_edge_is_observed(self):
        self.assertIn(('local_step', 'blocking_leaf'), self.compile_edges([]))

    @unittest.skipUnless(sys.platform.startswith('linux'), 'ELF direct calls and section relocations')
    def test_local_direct_call_is_observed(self):
        self.assertIn(('frame_root', 'local_step'), self.compile_edges([]))

    @unittest.skipUnless(sys.platform.startswith('linux'), 'ELF function sections')
    def test_function_sections_keep_both_edges(self):
        edges = self.compile_edges(['-ffunction-sections'])
        self.assertIn(('frame_root', 'local_step'), edges)
        self.assertIn(('local_step', 'blocking_leaf'), edges)

    @unittest.skipUnless(sys.platform.startswith('linux'), 'ELF relocation placeholders')
    def test_relocation_targets_replace_displayed_addresses(self):
        edges = self.compile_edges([], '''#include <string>
extern "C" void blocking_leaf();
void file_target(std::string text) { blocking_leaf(); }
void make_text(const std::string &text) { file_target(text); }
''')
        for caller, callee in edges:
            if caller.startswith('std::'):
                self.assertNotIn(callee, {'file_target', 'make_text', 'blocking_leaf'})

    def test_corrupt_archive_is_an_error(self):
        with tempfile.TemporaryDirectory(prefix='outshine-callgraph-',
                                         dir=os.environ.get('TMPDIR', '/tmp')) as directory:
            work = Path(directory)
            archive = work / 'broken.a'
            archive.write_text('not an archive')
            result = subprocess.run(['sh', str(ROOT / 'test/harness/shared/graph/callgraph.sh'),
                                     str(archive), str(work / 'walk')],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
