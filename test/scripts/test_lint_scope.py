"""Changed-scope coverage must follow transitive headers and complete Git changes."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from lint_scope import affected_units, changed_paths, full_reasons


class LintScope(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='outshine-lint-scope-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()

    def write(self, name, text=''):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def record(self, name, includes=()):
        return {'file': str(self.root / name), 'directory': str(self.root),
                'arguments': ['c++', *['-I' + folder for folder in includes], '-c', name]}

    def test_transitive_header_consumers_include_objective_cpp(self):
        self.write('src/Shared.h')
        self.write('src/Wrapper.h', '#include "Shared.h"\n')
        self.write('src/A.cpp', '#include "Wrapper.h"\n')
        self.write('src/B.mm', '#include "Shared.h"\n')
        self.write('src/C.cpp')
        records = [self.record('src/' + name) for name in ('A.cpp', 'B.mm', 'C.cpp')]
        self.assertEqual(affected_units(self.root, records, {Path('src/Shared.h')}),
                         [self.root / 'src/A.cpp', self.root / 'src/B.mm'])

    def test_actual_include_order_distinguishes_equal_names(self):
        self.write('src/one/Shared.h')
        self.write('src/two/Shared.h')
        self.write('src/A.cpp', '#include <Shared.h>\n')
        self.write('src/B.cpp', '#include <Shared.h>\n')
        records = [self.record('src/A.cpp', ['src/one', 'src/two']),
                   self.record('src/B.cpp', ['src/two', 'src/one'])]
        self.assertEqual(affected_units(self.root, records, {Path('src/two/Shared.h')}),
                         [self.root / 'src/B.cpp'])

    def test_removed_headers_conservatively_select_all_units(self):
        self.write('src/A.cpp')
        self.write('src/B.cpp')
        records = [self.record('src/' + name) for name in ('A.cpp', 'B.cpp')]
        self.assertEqual(len(affected_units(self.root, records, {Path('src/Removed.h')})), 2)

    def test_build_rules_require_full_integration_but_local_code_does_not(self):
        for name in ('Makefile', '.clang-tidy', 'src/render/reaches',
                     'test/run.sh', 'test/scripts/tidy_analysis.py',
                     'src/render/shaders/light.frag'):
            with self.subTest(path=name):
                self.assertTrue(full_reasons({Path(name)}))
        self.assertFalse(full_reasons({Path('src/base/Math.cpp'), Path('src/base/Math.h')}))

    def test_git_selection_keeps_staged_unstaged_untracked_and_deleted_files(self):
        def git(*args):
            return subprocess.run(['git', *args], cwd=self.root, check=True,
                                  capture_output=True, text=True)
        git('init', '-q')
        for name in ('staged.cpp', 'unstaged.cpp', 'deleted.h'):
            self.write(name, 'before\n')
        git('add', '.')
        git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
            'commit', '-qm', 'baseline')
        self.write('staged.cpp', 'after\n')
        git('add', 'staged.cpp')
        self.write('unstaged.cpp', 'after\n')
        self.write('new.mm')
        (self.root / 'deleted.h').unlink()
        self.assertEqual(changed_paths(self.root, 'HEAD'),
                         {Path(name) for name in ('staged.cpp', 'unstaged.cpp', 'new.mm', 'deleted.h')})


if __name__ == '__main__':
    unittest.main()
