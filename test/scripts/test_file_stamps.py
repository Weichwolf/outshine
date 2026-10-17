"""Verify actual product compilation across cache hits, edits and restored timestamps."""

import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ObjectCache(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='outshine-object-cache-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.objects = self.root / 'obj'
        self.objects.mkdir()
        source = (ROOT / 'test/run.sh').read_text()
        declarations = '\n'.join(re.search('^' + name + r'=.*$', source, re.M).group()
                                 for name in ('CXX', 'CXXSTD', 'WARN'))
        functions = []
        for name in ('FileStamp', 'SourceStamp', 'UpToDate', 'PrepareCompilerCache',
                     'CompileCachedObject', 'FreshAgainst', 'ArchiveFresh',
                     'SetGroupCompileFlags', 'BuildGroup', 'BinaryStamp'):
            match = re.search(r'^' + name + r'\(\) \{\n.*?^\}\n', source, re.M | re.S)
            if name not in ('FileStamp', 'SetGroupCompileFlags', 'BuildGroup', 'BinaryStamp') and match is None:
                continue
            self.assertIsNotNone(match, name)
            functions.append(match.group())
        self.helper = self.root / 'build.sh'
        self.helper.write_text('ROOT=' + shlex.quote(str(ROOT)) + '\n' + declarations + '\n' + '''#!/bin/sh
set -eu
OPT=${OPT:--O0}
SAN=
EXTRA_DEFINES=
OBJECTS=
JOBS=1
OBJDIR=$PWD/obj
GroupIncludes() { printf '%s' '-Isrc/base'; }
GroupToolchain() { printf '%s' "$CXXSTD -fno-exceptions"; }
SourceUnits() { printf '%s\\n' 'src/base/Probe.cpp'; }
Die() { printf '%s\\n' "$*" >&2; exit 1; }
''' + '\n'.join(functions) + '\nBuildGroup src/base\nprintf "%s\\n" "$OBJECTS"\n')
        directory = self.root / 'src/base'
        directory.mkdir(parents=True)
        self.source = directory / 'Probe.cpp'
        self.header = directory / 'value.h'
        self.source.write_text('#include "value.h"\nconst char *value() { return VALUE; }\n')
        self.header.write_text('#define VALUE "VERSION_A"\n')
        self.backdate_inputs()

    def backdate_inputs(self):
        for path in (self.source, self.header):
            metadata = path.stat()
            os.utime(path, ns=(metadata.st_atime_ns, metadata.st_mtime_ns - 60000000000))

    def build(self, extra=None, success=True):
        environment = dict(os.environ)
        environment.update(extra or {})
        result = subprocess.run(['sh', str(self.helper)], cwd=self.root, env=environment,
                                capture_output=True, text=True, timeout=30)
        if not success:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            return None
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        paths = result.stdout.split()
        self.assertEqual(len(paths), 1, result.stdout)
        return Path(paths[0])

    def test_frozen_invocation_shares_groups_but_separates_profiles(self):
        source = (ROOT / 'test/run.sh').read_text()
        wrapper = re.search(r'^BuildCaseGroup\(\) \{\n.*?^\}\n', source, re.M | re.S).group()
        script = self.helper.read_text().split('\nBuildGroup src/base\n')[0]
        script += '\n' + wrapper + '''
OBJECTS=prefix
BuildCaseGroup src/base
first=$OBJECTS
OBJECTS=prefix
BuildCaseGroup src/base
[ "$OBJECTS" = "$first" ]
OPT=-O1
OBJECTS=prefix
BuildCaseGroup src/base
[ "$OBJECTS" != "$first" ]
printf '%s\\n' "$first" "$OBJECTS"
'''
        result = subprocess.run(['sh'], input=script, cwd=self.root, capture_output=True,
                                text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        paths = [line.removeprefix('prefix ').strip() for line in result.stdout.splitlines()]
        self.assertEqual(len(paths), 2)
        for path in paths:
            self.assertIn(b'VERSION_A', Path(path).read_bytes())

    def test_unchanged_inputs_reuse_the_actual_compiled_object(self):
        object_path = self.build()
        before = object_path.stat().st_mtime_ns
        self.assertEqual(self.build(), object_path)
        self.assertEqual(object_path.stat().st_mtime_ns, before)
        self.assertIn(b'VERSION_A', object_path.read_bytes())

    def test_same_size_header_edit_with_restored_mtime_rebuilds_actual_code(self):
        object_path = self.build()
        metadata = self.header.stat()
        self.header.write_text('#define VALUE "VERSION_B"\n')
        os.utime(self.header, ns=(metadata.st_atime_ns, metadata.st_mtime_ns))
        self.build()
        self.assertIn(b'VERSION_B', object_path.read_bytes())
        self.assertNotIn(b'VERSION_A', object_path.read_bytes())

    def test_backwards_source_timestamp_rebuilds_actual_code(self):
        object_path = self.build()
        metadata = self.source.stat()
        self.source.write_text('const char *value() { return "VERSION_C"; }\n')
        os.utime(self.source, ns=(metadata.st_atime_ns, metadata.st_mtime_ns - 100000000000))
        self.build()
        self.assertIn(b'VERSION_C', object_path.read_bytes())
        self.assertNotIn(b'VERSION_A', object_path.read_bytes())

    def test_timestamp_only_touch_does_not_recompile_content(self):
        object_path = self.build()
        before = object_path.stat().st_mtime_ns
        metadata = self.header.stat()
        os.utime(self.header, ns=(metadata.st_atime_ns, before + 1000000000))
        self.build()
        self.assertEqual(object_path.stat().st_mtime_ns, before)

    def test_missing_header_refuses_cache_hit_and_recovers_after_repair(self):
        self.build()
        saved = self.header.read_bytes()
        self.header.unlink()
        self.build(success=False)
        self.header.write_bytes(saved)
        self.assertIn(b'VERSION_A', self.build().read_bytes())

    def test_dependency_paths_with_spaces_reuse_compiled_objects(self):
        moved = self.header.with_name('value with spaces.h')
        self.header.rename(moved)
        self.header = moved
        self.source.write_text('#include "value with spaces.h"\nconst char *value() { return VALUE; }\n')
        self.backdate_inputs()
        object_path = self.build()
        before = object_path.stat().st_mtime_ns
        self.build()
        self.assertEqual(object_path.stat().st_mtime_ns, before)

    def test_changed_compile_flags_select_a_separate_object(self):
        first = self.build()
        second = self.build({'OPT': '-O1'})
        self.assertNotEqual(first, second)
        self.assertIn(b'VERSION_A', second.read_bytes())

    def test_changed_include_environment_cannot_reuse_previous_header(self):
        self.source.write_text('#include <cache_selector.h>\nconst char *value() { return VALUE; }\n')
        for name, value in (('a', 'ENV_A'), ('b', 'ENV_B')):
            directory = self.root / name
            directory.mkdir()
            (directory / 'cache_selector.h').write_text('#define VALUE "' + value + '"\n')
        first = self.build({'CPATH': str(self.root / 'a')})
        self.assertIn(b'ENV_A', first.read_bytes())
        second = self.build({'CPATH': str(self.root / 'b')})
        self.assertIn(b'ENV_B', second.read_bytes())
        self.assertNotIn(b'ENV_A', second.read_bytes())

    def test_compiler_version_is_part_of_the_object_configuration(self):
        compiler = shutil.which(os.environ.get('CXX', 'c++'))
        self.assertIsNotNone(compiler)
        version = self.root / 'compiler-version'
        version.write_text('fixture-version-one\n')
        wrapper = self.root / 'compiler'
        wrapper.write_text('#!/bin/sh\nif [ "$1" = --version ]; then cat "' + str(version) +
                           '"; else exec "' + compiler + '" "$@"; fi\n')
        wrapper.chmod(0o755)
        first = self.build({'CXX': str(wrapper)})
        version.write_text('fixture-version-two\n')
        second = self.build({'CXX': str(wrapper)})
        self.assertNotEqual(first, second)
        self.assertIn(b'VERSION_A', second.read_bytes())

    def test_same_second_object_change_invalidates_actual_archive(self):
        object_path = self.build()
        archive = self.root / 'product.a'
        subprocess.run(['ar', 'rcs', str(archive), str(object_path)], check=True,
                       capture_output=True, timeout=10)
        self.header.write_text('#define VALUE "VERSION_B"\n')
        self.build()
        self.assertIn(b'VERSION_B', object_path.read_bytes())
        second = object_path.stat().st_mtime_ns // 1000000000 * 1000000000
        os.utime(archive, ns=(second, second + 100000000))
        os.utime(object_path, ns=(second, second + 800000000))
        script = self.helper.read_text().split('\nBuildGroup src/base\n')[0]
        result = subprocess.run(['sh', '-c', script + '\nArchiveFresh "$1" "$2"',
                                 'archive-check', str(archive), str(object_path)],
                                cwd=self.root, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)

    def test_case_binary_stamp_checks_content_after_restored_timestamp(self):
        object_path = self.build()
        harness = self.root / 'test/harness/shared'
        harness.mkdir(parents=True)
        (harness / 'Check.h').write_text('#pragma once\n')
        script = self.helper.read_text().split('\nBuildGroup src/base\n')[0]
        script += '\nOBJECTS=$2\nBinaryStamp "$1" src/base/Probe.cpp\n'

        def stamp():
            result = subprocess.run(['sh', '-c', script, 'case-stamp', 'case', str(object_path)],
                                    cwd=self.root, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return result.stdout

        before = stamp()
        metadata = self.source.stat()
        self.source.write_text(self.source.read_text().replace('value()', 'other()'))
        os.utime(self.source, ns=(metadata.st_atime_ns, metadata.st_mtime_ns))
        self.assertNotEqual(stamp(), before)


if __name__ == '__main__':
    unittest.main()
