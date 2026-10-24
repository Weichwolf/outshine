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


def shell_functions(declaration, names):
    functions = []
    collecting = False
    for line in declaration.splitlines():
        if line.removesuffix('() {') in names and line.endswith('() {'):
            collecting = True
        if collecting:
            functions.append(line)
            if line == '}':
                collecting = False
    return '\n'.join(functions)


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

    def test_objective_cpp_uses_the_same_header_contract(self):
        header = self.write('src/MetalBinding.h')
        source = self.write('src/Device.mm', '#include "MetalBinding.h"\n')
        self.assertEqual(self.reader.read(source, []), {source, header})


class NativeStageDependencies(unittest.TestCase):
    root = SCRIPT.parents[2]
    required = frozenset({'src/render/stages/StagePipelines.cpp',
                          'src/render/device/SpirvFile.cpp', 'src/base/io/ReadTextFile.cpp'})

    def test_database_emission_never_compiles_archives_or_links(self):
        import json
        with tempfile.TemporaryDirectory(prefix='outshine-compile-db-') as temp:
            root = Path(temp).resolve()
            for source in ('src/base/Value.cpp', 'src/client/Main.cpp'):
                path = root / source
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            (root / 'obj').mkdir()
            functions = shell_functions((self.root / 'test/run.sh').read_text(),
                                        {'BuildGroup', 'BuildLibrary'})
            stubs = r'''
COMPILEDB=1 BUILD=. OBJDIR=obj JOBS=2 CXX=forbidden-compiler
CXXSTD=-std=c++23 OPT=-O2 WARN=-Werror SAN= EXTRA_DEFINES=-DREAL_FLAGS=1
PrepareCompilerCache() { :; }
GroupIncludes() { printf '%s' '-Isrc/base'; }
GroupToolchain() { printf '%s' '-std=c++23'; }
SetGroupCompileFlags() { setId=exact; arcSetId=exact; arcStd=$groupStd; }
SourceUnits() {
  case "${1:-all}" in
    all) printf '%s\n' src/base/Value.cpp src/client/Main.cpp ;;
    src/base) printf '%s\n' src/base/Value.cpp ;;
    src/client) printf '%s\n' src/client/Main.cpp ;;
  esac
}
python3() { :; }
CompileCachedObject() { printf 'unexpected compiler\n' >&2; exit 99; }
ArchiveFresh() { printf 'unexpected archive\n' >&2; exit 99; }
FreshAgainst() { printf 'unexpected linker\n' >&2; exit 99; }
Die() { printf '%s\n' "$*" >&2; exit 99; }
BuildLibrary
'''
            result = subprocess.run(['sh', '-eu', '-c', functions + stubs], cwd=root,
                                    capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            entries = json.loads('[' + (root / 'compile_commands.part').read_text().rstrip(',\n') + ']')
            self.assertEqual({Path(entry['file']).relative_to(root).as_posix() for entry in entries},
                             {'src/base/Value.cpp', 'src/client/Main.cpp'})
            self.assertTrue(all('-std=c++23 -O2 -Werror' in row['command'] for row in entries))
            self.assertIn('-DREAL_FLAGS=1', entries[0]['command'])
            self.assertEqual(list((root / 'obj').iterdir()), [])
            self.assertFalse((root / 'build').exists())

    def sources(self, declaration, suite='outshine/src/render/stages/StagePipelines'):
        names = {'SourceUnits', 'NativeGroups', 'GeneratorGroups', 'EngineRootGroups',
                 'SubtreeGroups', 'TestProfile', 'LayerGroups'}
        functions = shell_functions(declaration, names)
        environment = dict(os.environ)
        environment['SOURCE_UNITS'] = subprocess.check_output(
            [sys.executable, 'test/scripts/source_units.py'], cwd=self.root, text=True)
        environment['SOURCE_DIRECTORIES'] = '\n'.join(
            str(path.relative_to(self.root)) for path in (self.root / 'src').rglob('*')
            if path.is_dir())
        command = functions + (
            f'\nfor group in $(LayerGroups {suite}); '
            'do SourceUnits "$group"; done\n')
        result = subprocess.run(['sh', '-eu', '-c', command], cwd=self.root,
                                env=environment, capture_output=True, text=True, check=True)
        return result.stdout.splitlines()

    def test_value_and_geographic_tests_do_not_build_unrelated_modules(self):
        declaration = (self.root / 'test/run.sh').read_text()
        for suite in ('outshine/src/render/RenderConfiguration',
                      'outshine/src/base/scene/Material',
                      'outshine/src/base/scene/ProjectedErrorBudget'):
            with self.subTest(suite=suite):
                self.assertEqual(self.sources(declaration, suite), [])
        self.assertEqual(set(self.sources(declaration, 'outshine/src/base/geo/GeoCellId')),
                         {'src/base/geo/GeoCellId.cpp', 'src/base/geo/GeoCellDemand.cpp'})

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


class Sha256Dependencies(unittest.TestCase):
    root = SCRIPT.parents[2]
    profiles = ('profile/base', 'profile/content', 'profile/engine', 'harness/claims',
                'profile/plant-asset', 'profile/plant-crown-cache', 'profile/plant-limb',
                'profile/stage-pipelines', 'profile/calibration', 'profile/tasks',
                'profile/plant-recipe-json')

    def links(self, platform, profiles=None, available=True):
        names = {'SourceUnits', 'NativeGroups', 'GeneratorGroups', 'EngineRootGroups',
                 'SubtreeGroups', 'TestProfile', 'LayerGroups', 'LayerLink',
                 'LayerLibraryLink', 'Sha256Flags', 'NativeLink', 'SpirvCrossFlags',
                 'OsqpFlags', 'HighsFlags'}
        functions = shell_functions((self.root / 'test/run.sh').read_text(), names)
        environment = dict(os.environ, BUILD_PLATFORM=platform,
                           LIBCRYPTO_AVAILABLE=str(int(available)),
                           NATIVE_LINK_FLAGS='-Wl,test-hardening')
        environment['SOURCE_UNITS'] = subprocess.check_output(
            [sys.executable, 'test/scripts/source_units.py'], cwd=self.root, text=True)
        environment['SOURCE_DIRECTORIES'] = '\n'.join(
            str(path.relative_to(self.root)) for path in (self.root / 'src').rglob('*')
            if path.is_dir())
        stub = r"""
#!/bin/sh
for package do
  if [ "$package" = libcrypto ]; then
    [ "$LIBCRYPTO_AVAILABLE" = 1 ] || exit 1
    case "$1" in --libs) printf '%s' '-lcrypto' ;; esac
  fi
done
exit 0
"""
        loop = r"""
for suite do
  if for group in $(LayerGroups "$suite"); do SourceUnits "$group"; done |
      grep -qx 'src/base/format/Sha256.cpp'; then sha=1; else sha=0; fi
  printf '%s\t%s\t' "$suite" "$sha"
  LayerLink "$suite"
  printf '\n'
done
"""
        with tempfile.TemporaryDirectory(prefix='outshine-sha-link-') as directory:
            package_tool = Path(directory) / 'pkg-config'
            package_tool.write_text(stub.lstrip())
            package_tool.chmod(0o755)
            environment['PATH'] = directory + os.pathsep + environment['PATH']
            return subprocess.run(['sh', '-eu', '-c', functions + loop,
                                   'sha-link-test', *(profiles or self.profiles)],
                                  cwd=self.root, env=environment, capture_output=True, text=True,
                                  check=False)

    def test_linux_links_crypto_exactly_when_the_declared_sources_use_sha(self):
        result = self.links('Linux')
        self.assertEqual(result.returncode, 0, result.stderr)
        for row in result.stdout.splitlines():
            profile, uses_sha, flags = row.split('\t', 2)
            self.assertEqual(flags.split().count('-Wl,test-hardening'), 1, profile)
            self.assertEqual(flags.split().count('-lcrypto'), int(uses_sha), profile)

    def test_apple_uses_commoncrypto_without_an_openssl_package(self):
        result = self.links('Darwin', available=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn('-lcrypto', result.stdout)

    def test_missing_crypto_does_not_block_native_stage_profiles(self):
        isolated = self.links('Linux', ['profile/stage-pipelines', 'profile/calibration'],
                              available=False)
        self.assertEqual(isolated.returncode, 0, isolated.stderr)
        required = self.links('Linux', ['profile/base'], available=False)
        self.assertNotEqual(required.returncode, 0)
        self.assertIn('install libssl-dev', required.stderr)


if __name__ == '__main__':
    unittest.main()
