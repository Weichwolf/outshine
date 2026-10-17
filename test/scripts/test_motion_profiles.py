#!/usr/bin/env python3
"""Verify render-rate-independent route motion through the public offline client."""
import csv
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
CLIENT = Path(os.environ.get('OUTSHINE_CLIENT', ROOT / 'build/outshine-client')).resolve()


class MotionProfiles(unittest.TestCase):
    def test_fixed_physics_reaches_same_pose_at_each_render_rate(self):
        with tempfile.TemporaryDirectory(prefix='outshine-motion-profiles-') as directory:
            work = Path(directory)
            (work / 'src').symlink_to(ROOT / 'src', target_is_directory=True)
            (work / 'build').mkdir()
            (work / 'build/shaders').symlink_to(ROOT / 'build/shaders', target_is_directory=True)
            source = ET.parse(ROOT / 'src/assets/diagnostic-places/Hockenheimring.scenario')
            ET.SubElement(source.getroot().find('world'), 'relief',
                          {'kind': 'sineGrid', 'amplitudeM': '0'})
            path = work / 'Route.scenario'
            source.write(path)
            poses = []
            for rate in (25, 30, 60):
                with self.subTest(rate=rate):
                    folder = work / str(rate)
                    result = subprocess.run(
                        [str(CLIENT), 'run', '--view', 'lap', '--at-seconds', '0.2',
                         '--motion', '--rows', '--offline', '--cache-dir', str(work / 'sources'),
                         '--into', str(folder), '--scenario-overrides',
                         json.dumps({'render': {'widthPx': 160, 'heightPx': 90, 'fps': rate}}),
                         str(path), 'Route'], cwd=work, capture_output=True, text=True, timeout=60)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    rows = [line.split('\t') for line in result.stdout.splitlines()]
                    motion = next(row for row in rows if row[0] == 'MOTION')
                    profile = next(row for row in rows if row[0] == 'PROFILE')
                    capture = next(row for row in rows if row[0] == 'CAPTURE')
                    count = round(rate * 0.2)
                    self.assertEqual(int(motion[3]), count)
                    self.assertEqual(profile[2:5], ['160', '90', str(rate)])
                    self.assertAlmostEqual(float(capture[3]), 0.2)
                    self.assertAlmostEqual(float(capture[4]), 0.5 * 3 * 0.2 ** 2)
                    with Path(motion[10]).open() as trace:
                        samples = list(csv.DictReader(trace, delimiter='\t'))
                    self.assertEqual(len(samples), count)
                    for index, sample in enumerate(samples, 1):
                        self.assertAlmostEqual(float(sample['time_s']), index / rate, places=6)
                        self.assertGreater(float(sample['eye_clearance_m']), 0)
                        if index > 1:
                            self.assertGreater(float(sample['camera_frame_serial']),
                                               float(samples[index - 2]['camera_frame_serial']))
                    end = samples[-1]
                    poses.append([float(end[key]) for key in
                                  ('station_m', 'camera_eye_east_m', 'camera_eye_up_m',
                                   'camera_eye_south_m', 'camera_forward_east',
                                   'camera_forward_up', 'camera_forward_south')])
            for pose in poses[1:]:
                for actual, expected in zip(pose, poses[0]):
                    self.assertAlmostEqual(actual, expected, places=6)


if __name__ == '__main__':
    unittest.main()
