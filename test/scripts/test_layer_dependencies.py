"""Preserve header reachability and ordered resolution across compile contexts."""

import importlib.util
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

SCRIPT = Path(__file__).with_name('layer-contract.py')
SPEC = importlib.util.spec_from_file_location('layer_contract', SCRIPT)
CONTRACT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONTRACT)


class HeaderTraversal(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='outshine-layer-dependencies-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.reader = CONTRACT.HeaderDependencies(self.root)

    def write(self, name, text=''):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def test_local_header_precedes_include_directories(self):
        source = self.write('src/Main.cpp', '#include "Same.h"\n')
        local = self.write('src/Same.h')
        other = self.write('external/Same.h', '#include "Forbidden.h"\n')
        self.write('external/Forbidden.h')
        self.assertEqual(self.reader.read(source, [other.parent]), {source, local})

    def test_include_order_and_each_compile_context_are_preserved(self):
        source = self.write('src/Main.cpp', '#include "Same.h"\n')
        first = self.write('one/Same.h')
        second = self.write('two/Same.h')
        self.assertEqual(self.reader.read(source, [first.parent, second.parent]), {source, first})
        self.assertEqual(self.reader.read(source, [second.parent, first.parent]), {source, second})

    def test_cycles_duplicates_and_missing_headers_keep_complete_reachability(self):
        source = self.write('src/Main.cpp', '#include "A.h"\n#include "Missing.h"\n#include "A.h"\n')
        first = self.write('src/A.h', '#include "B.h"\n')
        second = self.write('src/B.h', '#include "A.h"\n')
        self.assertEqual(self.reader.read(source, []), {source, first, second})

    def test_external_first_match_does_not_fall_through_to_product_header(self):
        source = self.write('src/Main.cpp', '#include "Same.h"\n')
        inside = self.write('inside/Same.h')
        with tempfile.TemporaryDirectory(prefix='outshine-layer-external-') as directory:
            outside = Path(directory).resolve()
            (outside / 'Same.h').write_text('')
            self.assertEqual(self.reader.read(source, [outside, inside.parent]), {source})

    def test_shared_graph_is_not_read_or_statted_again(self):
        first = self.write('src/One.cpp', '#include "Shared.h"\n')
        second = self.write('src/Two.cpp', '#include "Shared.h"\n')
        shared = self.write('headers/Shared.h', '#include "Leaf.h"\n')
        leaf = self.write('headers/Leaf.h')
        counts = {'reads': 0, 'stats': 0}
        read_text, is_file = Path.read_text, Path.is_file

        def counted_read(path, *args, **kwargs):
            counts['reads'] += 1
            return read_text(path, *args, **kwargs)

        def counted_file(path):
            counts['stats'] += 1
            return is_file(path)

        with patch.object(Path, 'read_text', counted_read), patch.object(Path, 'is_file', counted_file):
            self.assertEqual(self.reader.read(first, [shared.parent]), {first, shared, leaf})
            before = dict(counts)
            self.assertEqual(self.reader.read(second, [shared.parent]), {second, shared, leaf})
            self.assertEqual(counts['reads'] - before['reads'], 1)
            self.assertEqual(counts['stats'], before['stats'])

    def test_new_run_reads_changed_inputs_and_does_not_keep_negative_results(self):
        source = self.write('src/Main.cpp', '#include "Later.h"\n')
        self.assertEqual(self.reader.read(source, []), {source})
        later = self.write('src/Later.h')
        next_run = CONTRACT.HeaderDependencies(self.root)
        self.assertEqual(next_run.read(source, []), {source, later})


class NativeStageDependencies(unittest.TestCase):
    root = SCRIPT.parents[2]
    required = {'src/render/stages/StagePipelines.cpp',
                'src/render/device/SpirvFile.cpp', 'src/base/io/ReadTextFile.cpp'}

    def sources(self, declaration):
        names = {'SourceUnits', 'NativeGroups', 'GeneratorGroups', 'EngineRootGroups',
                 'SubtreeGroups', 'TestProfile', 'LayerGroups'}
        functions = []
        collecting = False
        for line in declaration.splitlines():
            if line.removesuffix('() {') in names and line.endswith('() {'):
                collecting = True
            if collecting:
                functions.append(line)
                if line == '}':
                    collecting = False
        environment = dict(os.environ)
        environment['SOURCE_UNITS'] = subprocess.check_output(
            [sys.executable, 'test/scripts/source_units.py'], cwd=self.root, text=True)
        environment['SOURCE_DIRECTORIES'] = '\n'.join(
            str(path.relative_to(self.root)) for path in (self.root / 'src').rglob('*')
            if path.is_dir())
        command = '\n'.join(functions) + (
            '\nfor group in $(LayerGroups outshine/src/render/stages/StagePipelines); '
            'do SourceUnits "$group"; done\n')
        result = subprocess.run(['sh', '-eu', '-c', command], cwd=self.root,
                                env=environment, capture_output=True, text=True, check=True)
        return result.stdout.splitlines()

    def test_native_stage_sources_stay_in_their_declared_components(self):
        units = self.sources((self.root / 'test/run.sh').read_text())
        self.assertEqual(len(units), len(set(units)))
        self.assertTrue(self.required <= set(units))
        self.assertTrue(any(unit.startswith('src/render/device/native/') for unit in units))
        self.assertEqual([unit for unit in units if unit not in self.required and
                          not unit.startswith('src/render/device/native/')], [])

    def test_original_fallback_reintroduces_unrelated_implementations(self):
        original = (self.root / 'test/run.sh').read_text()
        fallback, count = re.subn(
            r'(?m)^    outshine/src/render/stages/StagePipelines\).*\n', '', original, count=1)
        self.assertEqual(count, 1)
        units = self.sources(fallback)
        self.assertTrue(any(unit.startswith(('src/engine/', 'src/generators/')) for unit in units))


if __name__ == '__main__':
    unittest.main()
