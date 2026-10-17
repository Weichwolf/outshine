#!/usr/bin/env python3
"""Exercise paced Place capture through the built client, offline with a prepared Place asset cache."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CLIENT = Path(os.environ.get('OUTSHINE_CLIENT', ROOT / 'build/outshine-client')).resolve()
PLACE = os.environ.get('OUTSHINE_PROFILE_PLACE', 'Husum')


class PlaceProfiles(unittest.TestCase):
    def test_requested_rate_drives_real_capture(self):
        with tempfile.TemporaryDirectory(prefix='outshine-place-profiles-') as directory:
            work = Path(directory)
            (work / 'src').symlink_to(ROOT / 'src', target_is_directory=True)
            (work / 'build').mkdir()
            (work / 'build/shaders').symlink_to(ROOT / 'build/shaders', target_is_directory=True)
            for rate in (25, 30, 60):
                with self.subTest(rate=rate):
                    result = subprocess.run(
                        [str(CLIENT), '--places', str(ROOT / 'src/assets/places'), 'shots', '--offline', '--rows',
                         '--scenario-overrides',
                         json.dumps({'render': {'fps': rate}}), PLACE],
                        cwd=work, capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    rows = [line.split('\t') for line in result.stdout.splitlines()
                            if line.startswith('ROW\t')]
                    self.assertEqual(len(rows), 1, result.stdout)
                    row = rows[0]
                    self.assertEqual((row[1], row[3], row[7], row[18], row[19]),
                                     (PLACE, '1', str(rate), '360', '-'), result.stdout)
                    self.assertGreaterEqual(float(row[16]), 999.0)
                    profiles = [line.split('\t') for line in result.stdout.splitlines()
                                if line.startswith('PROFILE\t')]
                    self.assertEqual(len(profiles), 1, result.stdout)
                    self.assertEqual(profiles[0][1:5], [PLACE, '1280', '720', str(rate)])
                    self.assertAlmostEqual(float(profiles[0][5]), 1000 / rate, places=6)
                    pictures = list((work / 'build/shots/places').glob(f'{PLACE}-*.png'))
                    self.assertEqual(len(pictures), 1, 'only the latest successful shot remains')
                    self.assertEqual(pictures[0].name, f'{PLACE}-{row[2]}.png')


if __name__ == '__main__':
    unittest.main()
