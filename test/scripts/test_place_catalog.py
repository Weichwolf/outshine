#!/usr/bin/env python3
"""Exercise the built client with new and changing content, without recompilation."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CLIENT = ROOT / 'build/outshine-client'


def scenario(lat=12.5, fov=42, width=321):
    return f'''<scenario>
<world lat="{lat}" lon="-23.75"/>
<render widthPx="{width}" heightPx="123"/>
<clock start="2026-01-02T03:04:05Z" live="no"/>
<views><view id="station" person="first" fovDeg="{fov}">
<at lat="{lat}" lon="-23.75" heightM="456.25" bearingDeg="73" pitchDeg="-12" samplesHeight="no"/>
</view></views></scenario>'''


class Catalog(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='outshine-place-catalog-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def run_client(self, *args, success=True, directory=None):
        result = subprocess.run([str(CLIENT), '--places', str(directory or self.directory), *args],
                                capture_output=True, text=True, timeout=30, cwd=ROOT)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertTrue(result.stderr or result.stdout)
        return result

    def test_runtime_content_and_removal(self):
        path = self.directory / 'NeverCompiledHere.scenario'
        path.write_text(scenario())
        self.assertEqual(self.run_client('places').stdout.strip().split('\t'),
                         ['NeverCompiledHere', '12.5', '-23.75', '456.25', '73', '-12', '42',
                          '321', '123', '2026-01-02T03:04:05Z'])
        path.write_text(scenario(lat=18.75, fov=51, width=417))
        row = self.run_client('places').stdout.strip().split('\t')
        self.assertEqual((row[1], row[6], row[7]), ('18.75', '51', '417'))
        self.run_client('shots', 'UnknownPlace', success=False)
        path.unlink()
        self.run_client('places', success=False)

    def test_sorting_and_no_fixed_count(self):
        for name in ('Zulu', 'Alpha'):
            (self.directory / f'{name}.scenario').write_text(scenario())
        names = [line.split('\t')[0] for line in self.run_client('places').stdout.splitlines()]
        self.assertEqual(names, ['Alpha', 'Zulu'])

    def test_no_fallback(self):
        self.run_client('places', success=False, directory=self.directory / 'absent')
        self.run_client('places', success=False)

    def test_malformed_or_invalid_data(self):
        path = self.directory / 'Invalid.scenario'
        for text in ('<scenario><broken>', '<scenario/>', scenario(fov=-1), scenario(lat=91),
                     scenario(width=0), scenario().replace('2026-01-02T03:04:05Z', 'invalid')):
            with self.subTest(text=text):
                path.write_text(text)
                self.run_client('places', success=False)

    def test_shipped_catalog_migration(self):
        rows = self.run_client('places', directory=ROOT / 'src/assets/places').stdout.splitlines()
        actual = {fields[0]: fields[1:] for fields in (row.split('\t') for row in rows)}
        self.assertEqual(set(actual), {'DarmstadtWest', 'Wien', 'Rosenheim', 'Husum', 'Olympiaturm',
                                       'Graz', 'Koerbersee', 'Malcesine', 'Feldkirch'})
        self.assertEqual(actual['Malcesine'], ['45.744855', '10.800445', '140', '290', '-2',
                                              '38.04', '1280', '720', '2026-09-07T10:40:00Z'])
        self.assertEqual(actual['Husum'][-1], '2026-09-07T10:30:00Z')


if __name__ == '__main__':
    unittest.main()
