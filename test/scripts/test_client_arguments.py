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


if __name__ == '__main__':
    unittest.main()
