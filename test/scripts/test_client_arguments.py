#!/usr/bin/env python3
"""Prove coordinate rejection precedes platform initialization in the actual client."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CLIENT = ROOT / 'build/outshine-client'


class Coordinates(unittest.TestCase):
    def test_help_describes_commands_and_stats_without_starting_platform(self):
        env = dict(os.environ, SDL_VIDEODRIVER='outshine-intentionally-unavailable')
        cases = ((('--help',), ('render', 'run', 'shots', 'measures', '<command> --help')),
                 (('render', '--help'), ('asset.gltf|asset.glb', '--camera', '--stats', 'draw_frames')),
                 (('run', '--help'), ('--view', '--motion', '--samples', '--stats',
                                     '--probe-pixel', 'playable/refined')),
                 (('shots', '--help'), ('--preload-seconds', '--offline', '--stats')),
                 (('measures', '--help'), ('diagnostic samples', '--stats')),
                 (('height', '--help'), ('latitude-deg', 'longitude-deg')))
        for args, expected in cases:
            with self.subTest(args=args):
                result = subprocess.run([str(CLIENT), *args], cwd=ROOT, env=env,
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stderr)
                for fragment in expected:
                    self.assertIn(fragment, result.stdout)
                self.assertNotIn('SDL', result.stdout + result.stderr)
        unknown = subprocess.run([str(CLIENT), 'unknown', '--help'], cwd=ROOT, env=env,
                                 capture_output=True, text=True, timeout=10)
        self.assertEqual(unknown.returncode, 2)

    def test_run_stats_include_setup_even_when_startup_fails(self):
        env = dict(os.environ, SDL_VIDEODRIVER='outshine-intentionally-unavailable')
        result = subprocess.run([str(CLIENT), 'run', '--stats', 'missing.scenario'], cwd=ROOT,
                                env=env, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 2)
        rows = [line.split('\t') for line in result.stdout.splitlines()
                if line.startswith('STAT\t')]
        stats = {row[2]: row[3] for row in rows}
        self.assertEqual(stats['status'], 'failed')
        self.assertGreater(float(stats['setup_ms']), 0)
        self.assertGreaterEqual(float(stats['elapsed_ms']), float(stats['setup_ms']))

    def query(self, *args):
        env = dict(os.environ, SDL_VIDEODRIVER='outshine-intentionally-unavailable')
        return subprocess.run([str(CLIENT), 'height', *args], cwd=ROOT, env=env,
                              capture_output=True, text=True, timeout=10)

    def test_ignored_number_result_is_a_compile_error(self):
        database = json.loads((ROOT / 'compile_commands.json').read_text())
        command = next(entry for entry in database
                       if (Path(entry['directory']) / entry['file']).resolve() ==
                       (ROOT / 'src/client/Main.cpp').resolve())
        arguments = command.get('arguments') or shlex.split(command['command'])
        with tempfile.TemporaryDirectory(prefix='outshine-nodiscard-') as temporary:
            source = Path(temporary) / 'result.cpp'
            arguments = [str(source) if argument == command['file'] else
                         '-fsyntax-only' if argument == '-c' else argument for argument in arguments]
            for body, accepted in (('return outshine::ParseFiniteNumber("1").has_value() ? 0 : 1;', True),
                                   ('outshine::ParseFiniteNumber("1");', False)):
                source.write_text('#include "format/Number.h"\nint main() {' + body + '}\n')
                result = subprocess.run(arguments, cwd=command['directory'], capture_output=True,
                                        text=True, timeout=30)
                if accepted:
                    self.assertEqual(result.returncode, 0, result.stderr)
                else:
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('-Wunused-result', result.stderr)

    def test_exact_arity_before_platform_initialization(self):
        for args in ((), ('1',), ('1', '2', '3')):
            result = self.query(*args)
            self.assertEqual(result.returncode, 2)
            self.assertIn('height requires exactly', result.stderr)
            self.assertNotIn('SDL', result.stdout + result.stderr)

    def test_invalid_coordinates_before_platform_initialization(self):
        for bad in ('', 'junk', '12junk', 'nan', 'inf', '-inf', '1e999', '1e-9999',
                    ' 1', '1 ', '1,5', '+-1', '0x10'):
            for args in ((bad, '12'), ('12', bad)):
                with self.subTest(args=args):
                    result = self.query(*args)
                    self.assertEqual(result.returncode, 2)
                    self.assertIn('finite latitude', result.stderr)
                    self.assertNotIn('SDL', result.stdout + result.stderr)
        for args in (('90.001', '0'), ('-90.001', '0'), ('0', '180.001'), ('0', '-180.001')):
            result = self.query(*args)
            self.assertEqual(result.returncode, 2)
            self.assertIn('finite latitude', result.stderr)
            self.assertNotIn('SDL', result.stdout + result.stderr)

    def test_valid_coordinates_reach_the_injected_platform_failure(self):
        for args in (('0', '0'), ('-90', '-180'), ('+90', '+180'), ('4.5e1', '1.25e1')):
            result = self.query(*args)
            self.assertEqual(result.returncode, 2)
            self.assertIn('SDL did not start', result.stdout)
            self.assertNotIn('finite latitude', result.stderr)

    def test_cache_directory_is_validated_before_platform_initialization(self):
        env = dict(os.environ, SDL_VIDEODRIVER='outshine-intentionally-unavailable')
        for verb in ('run', 'shots'):
            for cache in ('', '--offline'):
                result = subprocess.run([str(CLIENT), verb, '--cache-dir', cache], cwd=ROOT,
                                        env=env, capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 2)
                self.assertIn('--cache-dir requires a nonempty directory', result.stderr)
                self.assertNotIn('SDL', result.stdout + result.stderr)
        accepted = subprocess.run([str(CLIENT), 'run', '--offline', '--cache-dir', '/tmp/outshine-test-cache',
                                   'missing.scenario'], cwd=ROOT, env=env,
                                  capture_output=True, text=True, timeout=10)
        self.assertEqual(accepted.returncode, 2)
        self.assertIn('SDL did not start', accepted.stdout)

    def test_invalid_pixel_probe_is_rejected_before_platform_initialization(self):
        env = dict(os.environ, SDL_VIDEODRIVER='outshine-intentionally-unavailable')
        for value in ('', '-1,0', '1,-1', '1', '1,2,3', 'nan,1', '2147483648,0'):
            result = subprocess.run([str(CLIENT), 'run', '--view', 'lap', '--at-seconds', '1',
                                     '--probe-pixel', value, 'missing.scenario'], cwd=ROOT,
                                    env=env, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 2, (value, result.stderr))
            self.assertIn('--probe-pixel requires nonnegative integer x,y', result.stderr)
            self.assertNotIn('SDL', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
