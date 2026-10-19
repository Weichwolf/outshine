"""Preserve header reachability and ordered resolution across compile contexts."""

import importlib.util
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


if __name__ == '__main__':
    unittest.main()
