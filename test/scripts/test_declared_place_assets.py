#!/usr/bin/env python3
"""Check offline generated and shipped inputs through fresh public client processes."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CLIENT = Path(os.environ.get('OUTSHINE_CLIENT', ROOT / 'build/outshine-client')).resolve()
SCENARIO = '''<scenario>
<world lat="49.3274" lon="8.5659" sightM="1000" vegetation="no">
  <relief kind="sineGrid" amplitudeM="0"/>{features}
</world>
<render widthPx="160" heightPx="90" fps="30"/>
{providers}
<lighting/><clock start="2026-01-02T03:04:05Z" live="no"/>
<views><view id="station" person="first" fovDeg="42">
  <at lat="49.3274" lon="8.5659" heightM="900" bearingDeg="73"
      pitchDeg="-30" samplesHeight="no"/>
</view></views>
</scenario>'''
XML = '''<providers><provider kind="osm" dataset="openstreetmap"
pin="sha256:f50914eac077325eee1b3e88eae0eeffac58ff086e180cba4ec8734e94661d7f"
location="world/osm/HockenheimringGrandPrix.osm" whenAbsent="fail"
westDeg="8.54" southDeg="49.315" eastDeg="8.61" northDeg="49.34"/></providers>'''
AREA = '''<osm><area kind="building" heightM="{height}"
points="49.3274,8.5659 49.3274,8.5661 49.3276,8.5661 49.3276,8.5659"/></osm>'''


class DeclaredPlaceAssets(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='outshine-declared-place-')
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)
        (self.work / 'src').symlink_to(ROOT / 'src', target_is_directory=True)
        (self.work / 'build').mkdir()
        (self.work / 'build/shaders').symlink_to(ROOT / 'build/shaders', target_is_directory=True)
        (self.work / 'places').mkdir()

    def scenario(self, features='', providers=''):
        (self.work / 'places/Flat.scenario').write_text(
            SCENARIO.format(features=features, providers=providers))

    def capture(self, hit):
        result = subprocess.run(
            [str(CLIENT), '--places', str(self.work / 'places'), 'shots', '--offline',
             '--rows', '--stats', '--cache-dir', str(self.work / 'sources'), 'Flat'],
            cwd=self.work, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f'PERF Flat native region: hit={hit} ', result.stdout)
        rows = [line.split('\t') for line in result.stdout.splitlines()
                if line.startswith('ROW\t')]
        self.assertEqual(len(rows), 1, result.stdout)
        row = rows[0]
        self.assertEqual((row[1], row[3], row[7], row[18], row[19]),
                         ('Flat', '1', '30', '360', '-'), result.stdout)
        self.assertGreaterEqual(float(row[16]), 999.0)
        pictures = list((self.work / 'build/shots/places').glob('Flat-*.png'))
        self.assertEqual([path.name for path in pictures], [f'Flat-{row[2]}.png'])

    def test_synthetic_terrain_reuses_complete_asset(self):
        self.scenario()
        self.capture(hit=0)
        self.capture(hit=1)

    def test_shipped_xml_reuses_complete_asset(self):
        self.scenario(providers=XML)
        self.capture(hit=0)
        self.capture(hit=1)

    def test_declared_content_change_invalidates_complete_asset(self):
        self.scenario(features=AREA.format(height=12))
        self.capture(hit=0)
        self.capture(hit=1)
        self.scenario(features=AREA.format(height=24))
        self.capture(hit=0)
        self.capture(hit=1)


if __name__ == '__main__':
    unittest.main()
