#!/usr/bin/env python3
"""Exercise generic benchmark input, real frame counts and atomic failure outputs."""

import json
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CLIENT = Path(os.environ.get('OUTSHINE_CLIENT', ROOT / 'build/outshine')).resolve()
SCENE = {
    'name': 'generic benchmark "input"',
    'render': {'widthPx': 32, 'heightPx': 32, 'fps': 120},
    'views': {'view': [{'id': 'start', 'fovDeg': 70,
                        'at': {'x': 0, 'y': 0, 'z': 4}}]},
}


class ScenarioBenchmark(unittest.TestCase):
    def invoke(self, *args, cwd=ROOT, env=None):
        return subprocess.run([str(CLIENT), 'benchmark', *args], cwd=cwd,
                              env=env, capture_output=True, text=True, timeout=60,
                              check=False)

    def test_real_frames_motion_and_final_image(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-motion-') as folder:
            output = Path(folder) / 'result.json'
            image = Path(folder) / 'last.png'
            result = self.invoke('--scenario', json.dumps(SCENE), '--frames', '8',
                                 '--linear-velocity', '1,-2,3',
                                 '--angular-velocity', '0,-90,0',
                                 '--view', 'start', '--offline',
                                 '--output', str(output), '--image', str(image), cwd=folder)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stdout, '')
            report = json.loads(output.read_text())
            self.assertEqual(report['mode'], 'scenario')
            self.assertEqual(report['scenario'], SCENE['name'])
            self.assertTrue(report['measurement_complete'])
            self.assertTrue(report['world_complete'])
            self.assertEqual(report['profile']['width_px'], 32)
            self.assertEqual(report['profile']['height_px'], 32)
            self.assertEqual(report['profile']['fps'], 120)
            self.assertEqual(report['motion']['frames'], 8)
            self.assertEqual(len(report['cpu_times']['samples']), 8)
            self.assertEqual(report['gpu_times']['rendered_frames'], 8)
            duration = 8 / 120
            for actual, expected in zip(report['motion']['finish']['position_m'],
                                        [duration, -2 * duration, 4 + 3 * duration], strict=True):
                self.assertAlmostEqual(actual, expected, places=9)
            self.assertAlmostEqual(report['motion']['start']['perspective_fov_deg'], 70, places=9)
            self.assertAlmostEqual(report['motion']['finish']['perspective_fov_deg'], 70, places=9)
            angle = math.radians(-90 * duration)
            model = report['motion']['finish']['world_from_camera']
            for at, expected in {0: math.cos(angle), 2: -math.sin(angle),
                                 8: math.sin(angle), 10: math.cos(angle)}.items():
                self.assertAlmostEqual(model[at], expected, places=9)
            gpu = report['gpu_times']
            if gpu['complete']:
                self.assertEqual(len(gpu['samples']), 8)
                identities = {(s['device'], s['submission']) for s in gpu['samples']}
                self.assertEqual(len(identities), 8)
                self.assertTrue(all(math.isfinite(s['elapsed_ms']) and s['elapsed_ms'] >= 0
                                    for s in gpu['samples']))
            else:
                self.assertIsNone(gpu['frame'])
                self.assertIsNone(report['budget']['gpu_p99_met'])
                self.assertFalse(report['budget']['met'])
            png = image.read_bytes()
            self.assertEqual(png[:8], b'\x89PNG\r\n\x1a\n')
            self.assertEqual(struct.unpack('>II', png[16:24]), (32, 32))
            metrics = {metric['name']: metric['value'] for metric in report['metrics']}
            for axis, value in zip(('east', 'up', 'south'),
                                   report['motion']['finish']['position_m'], strict=True):
                self.assertAlmostEqual(metrics[f'render last submitted camera: eye {axis}'],
                                       value, places=9)

    def test_benchmark_always_runs_without_target_waits(self):
        scene = dict(SCENE, render={'widthPx': 32, 'heightPx': 32, 'fps': 2})
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-throughput-') as folder:
            reports = []
            images = []
            for repeat in (0, 1):
                image = Path(folder) / f'{repeat}.png'
                result = self.invoke('--scenario', json.dumps(scene), '--frames', '6',
                                     '--linear-velocity', '1,-2,3',
                                     '--angular-velocity', '0,-90,0', '--offline',
                                     '--image', str(image))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                report = json.loads(result.stdout)
                self.assertTrue(report['world_complete'])
                self.assertTrue(report['measurement_complete'])
                self.assertEqual(report['motion']['frames'], 6)
                self.assertEqual(report['gpu_times']['rendered_frames'], 6)
                self.assertEqual(report['profile']['fps'], 2)
                reports.append(report)
                images.append(image.read_bytes())
            self.assertEqual(reports[0]['motion'], reports[1]['motion'])
            self.assertEqual(reports[0]['declaration_sha256'], reports[1]['declaration_sha256'])
            self.assertEqual(images[0], images[1])
            for report in reports:
                self.assertEqual(report['benchmark_version'], 2)
                self.assertEqual(report['profile']['pacing'], 'unpaced')
                self.assertNotIn('paced_window', report['phases_ms'])
                self.assertLess(report['phases_ms']['unpaced_window'], 1500)
                self.assertAlmostEqual(report['motion']['time_step_s'], 0.5)
                self.assertEqual(report['motion']['finish']['position_m'], [3, -6, 13])
                duration_ms = (report['phases_ms']['unpaced_window'] +
                               report['phases_ms']['final_gpu_flush'])
                self.assertAlmostEqual(report['wall_throughput_fps'], 6000 / duration_ms)

    def test_file_input_overrides_and_stdout(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-input-') as folder:
            scene = Path(folder) / 'scene.json'
            scene.write_text(json.dumps(SCENE))
            result = self.invoke('--scenario', str(scene), '--frames', '1', '--offline',
                                 '--scenario-overrides', '{"render":{"fps":30}}')
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual(report['profile']['fps'], 30)
            self.assertEqual(len(report['cpu_times']['samples']), 1)
            self.assertEqual(report['gpu_times']['rendered_frames'], 1)

    def test_shared_resolution_rate_profiles_drive_real_frames(self):
        for height, width in ((720, 1280), (1080, 1920), (1440, 2560), (2160, 3840)):
            for rate in (30, 60):
                with self.subTest(height=height, fps=rate):
                    profile = ROOT / 'data/profiles' / f'{height}p{rate}.json'
                    result = self.invoke('--scenario', json.dumps(SCENE), '--frames', '1',
                                         '--offline', '--scenario-overrides', profile.read_text())
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    report = json.loads(result.stdout)
                    actual = report['profile']
                    self.assertEqual((actual['width_px'], actual['height_px'], actual['fps']),
                                     (width, height, rate))
                    self.assertEqual(actual['pacing'], 'unpaced')
                    self.assertAlmostEqual(actual['budget_ms'], 1000 / rate)
                    self.assertAlmostEqual(report['motion']['time_step_s'], 1 / rate)
                    self.assertEqual(report['gpu_times']['rendered_frames'], 1)
                    self.assertTrue(report['measurement_complete'])
                    self.assertTrue(report['world_complete'])

    def test_large_translation_preserves_submitted_orientation(self):
        result = self.invoke('--scenario', json.dumps(SCENE), '--frames', '1',
                             '--linear-velocity', '1e20,0,0',
                             '--angular-velocity', '0,-7200,0', '--offline')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        model = report['motion']['finish']['world_from_camera']
        metrics = {m['name']: m['value'] for m in report['metrics']}
        for at, axis in ((8, 'east'), (9, 'up'), (10, 'south')):
            self.assertAlmostEqual(-model[at],
                                   metrics[f'render last submitted camera: forward {axis}'],
                                   places=14)
        self.assertAlmostEqual(-model[8], math.sqrt(3) / 2, places=14)

    def test_invalid_values_fail_before_video_and_preserve_files(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-invalid-') as folder:
            output = Path(folder) / 'result.json'
            image = Path(folder) / 'last.png'
            output.write_text('previous JSON')
            image.write_bytes(b'previous PNG')
            env = dict(os.environ, SDL_VIDEODRIVER='outshine-intentionally-unavailable')
            base = ('--scenario', json.dumps(SCENE), '--frames', '1',
                    '--output', str(output), '--image', str(image))
            for extra in (('--frames', '2'), ('--linear-velocity', 'nan,0,0'),
                          ('--angular-velocity', '0,0'), ('--preload-seconds', '0'),
                          ('--gpu-validation',)):
                with self.subTest(extra=extra):
                    result = self.invoke(*base, *extra, env=env)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    self.assertNotIn('SDL', result.stdout + result.stderr)
                    self.assertEqual(output.read_text(), 'previous JSON')
                    self.assertEqual(image.read_bytes(), b'previous PNG')

    def test_aliasing_outputs_fail_before_video(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-alias-') as folder:
            output = Path(folder) / 'result.json'
            output.write_text('previous JSON')
            hardlink = Path(folder) / 'hardlink.png'
            os.link(output, hardlink)
            env = dict(os.environ, SDL_VIDEODRIVER='outshine-intentionally-unavailable')
            for image in (str(output.parent) + '/./result.json', str(hardlink)):
                with self.subTest(image=image):
                    result = self.invoke('--scenario', json.dumps(SCENE), '--frames', '1',
                                         '--output', str(output), '--image', image, env=env)
                    self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                    self.assertIn('distinct output paths', result.stderr)
                    self.assertNotIn('SDL', result.stdout + result.stderr)
                    self.assertEqual(output.read_text(), 'previous JSON')

    def test_runtime_failure_preserves_both_outputs(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-failure-') as folder:
            output = Path(folder) / 'result.json'
            image = Path(folder) / 'last.png'
            output.write_text('previous JSON')
            image.write_bytes(b'previous PNG')
            result = self.invoke('--scenario', json.dumps(SCENE), '--frames', '1',
                                 '--output', str(output), '--image', str(image), cwd=folder,
                                 env=dict(os.environ, SDL_VIDEODRIVER='outshine-intentionally-unavailable'))
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertEqual(result.stdout, '')
            self.assertEqual(output.read_text(), 'previous JSON')
            self.assertEqual(image.read_bytes(), b'previous PNG')


if __name__ == '__main__':
    unittest.main()
