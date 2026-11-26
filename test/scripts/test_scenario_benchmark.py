#!/usr/bin/env python3
"""Exercise generic benchmark input, real frame counts and atomic failure outputs."""

import json
import math
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
import sys
import tempfile
import unittest

from place_gpu_profile import read_metal_profile

ROOT = Path(__file__).resolve().parents[2]
CLIENT = Path(os.environ.get('OUTSHINE_CLIENT', ROOT / 'build/outshine')).resolve()
SCENE = {
    'name': 'generic benchmark "input"',
    'render': {'widthPx': 32, 'heightPx': 32, 'fps': 120},
    'views': {'view': [{'id': 'start', 'fovDeg': 70,
                        'at': {'x': 0, 'y': 0, 'z': 4}}]},
}
PRODUCT_SCENE = """<scenario>
  <world lat="0" lon="0"/>
  <render widthPx="1280" heightPx="720" fps="30"/>
  <views><view id="station" nearM="1" farM="100" fovDeg="60">
    <at lat="0" lon="0" heightM="100" samplesHeight="no" bearingDeg="0"/>
  </view></views>
</scenario>"""


def write_panorama(root, anchor=(0, 100, 0), radius=0, colour=(1, 0, 0), revision='0',
                   axial_near=0, overlap_end=0, lighting=None, depth=10):
    root.mkdir(exist_ok=True)
    product = {
        'version': 1, 'representation': 'colour_radial_depth_cubemap',
        'face_order': ['px', 'nx', 'py', 'ny', 'pz', 'nz'], 'face_px': 2,
        'colour_encoding': 'rgba16f_linear_le_a_opaque_coverage',
        'depth_encoding': 'r32f_radial_metres_le_zero_without_geometry',
        'origin_lon_lat': [0, 0], 'anchor_eus_m': anchor, 'revision': revision * 64,
        'axial_near_m': axial_near,
        'axial_overlap_end_m': overlap_end,
        'maximum_range_m': 100, 'maximum_eye_translation_m': radius,
        'reference_exposure': 1, 'opaque_texels': [4 if depth > 0 else 0] * 6,
    }
    if lighting is not None:
        product['surface_lighting'] = lighting
    (root / 'panorama.json').write_text(json.dumps(product))
    (root / 'colour.rgba16f').write_bytes(struct.pack('<4e', *colour, depth > 0) * 24)
    (root / 'depth.r32f').write_bytes(struct.pack('<f', depth) * 24)


class ScenarioBenchmark(unittest.TestCase):
    def invoke(self, *args, cwd=ROOT, env=None):
        return subprocess.run([str(CLIENT), 'benchmark', *args], cwd=cwd,
                              env=env, capture_output=True, text=True, timeout=60,
                              check=False)

    @unittest.skipUnless(os.uname().sysname == 'Linux', 'Vulkan device selection')
    def test_explicit_missing_vulkan_device_never_falls_back(self):
        result = self.invoke('--scenario', json.dumps(SCENE), '--frames', '1', '--offline',
                             env=dict(os.environ, OUTSHINE_VULKAN_DEVICE='missing-outshine-device'))
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn('missing-outshine-device', result.stderr)

    def test_native_pass_profile_matches_the_measured_gpu_submissions(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-passes-') as folder:
            output = Path(folder) / 'result.json'
            prefix = Path(folder) / 'gpu-profile'
            profiler = ('OUTSHINE_METAL_ENCODER_PROFILE' if os.uname().sysname == 'Darwin'
                        else 'OUTSHINE_VULKAN_ENCODER_PROFILE')
            environment = dict(os.environ, **{profiler: str(prefix)})
            result = self.invoke('--scenario', json.dumps(SCENE), '--frames', '8',
                                 '--offline', '--output', str(output), env=environment)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads(output.read_text())
            profiles = list(Path(folder).glob('gpu-profile.*.json'))
            if report['backend'] == 'Metal':
                profile = read_metal_profile(Path(folder), report)
                self.assertTrue(profile['available'], profile.get('error'))
                self.assertEqual(len(profile['frames']), 8)
                self.assertTrue(profile['encoders'])
                return
            self.assertTrue(profiles)
            submissions = {}
            for path in profiles:
                profile = json.loads(path.read_text())
                self.assertEqual(profile['scope'], 'natural_render_pass_and_dispatch_boundaries')
                for submission in profile['submissions']:
                    submissions[profile['device'], submission['serial']] = submission
            for frame in report['gpu_times']['samples']:
                submission = submissions[frame['device'], frame['submission']]
                self.assertEqual(submission['dropped_passes'], 0)
                raster = [p for p in submission['passes'] if p['kind'] == 'render']
                self.assertTrue(raster)
                self.assertTrue(all(p['available'] and math.isfinite(p['gpu_ns'])
                                    and p['gpu_ns'] >= 0 for p in submission['passes']))
                self.assertGreaterEqual(sum(p['draw_calls'] for p in raster), 1)
                self.assertTrue(all(p['input_primitives'] is not None for p in raster))
                self.assertGreaterEqual(sum(p['input_primitives'] for p in raster), 1)

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
            work = []
            for sample in report['cpu_times']['samples']:
                self.assertGreaterEqual(sample['sync_wait_ms'], 0)
                self.assertGreaterEqual(sample['cpu_work_ms'], 0)
                self.assertAlmostEqual(sample['cpu_work_ms'] + sample['sync_wait_ms'],
                                       sample['frame_ms'], places=9)
                work.append(sample['cpu_work_ms'])
            self.assertEqual(report['cpu_times']['work']['p99_ms'], max(work))
            self.assertEqual(report['gpu_times']['rendered_frames'], 8)
            duration = 8 / 120
            position = report['motion']['finish']['position_m']
            self.assertEqual(len(position), 3)
            for actual, expected in zip(position, [duration, -2 * duration, 4 + 3 * duration]):
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
            self.assertTrue(math.isfinite(report['image_row_variation']))
            self.assertGreaterEqual(report['image_row_variation'], 0)
            metrics = {metric['name']: metric['value'] for metric in report['metrics']}
            self.assertEqual(metrics.get('cull snapshot: available'), 0)
            self.assertGreaterEqual(report['phases_ms']['final_inspection'], 0)
            for axis, value in zip(('east', 'up', 'south'), position):
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
            metrics = {metric['name']: metric['value'] for metric in report['metrics']}
            self.assertEqual(metrics.get('cull snapshot: available'), 0)
            self.assertGreaterEqual(report['phases_ms']['final_inspection'], 0)
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

    def test_panorama_outputs_cannot_overwrite_a_product_attachment(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-panorama-alias-') as folder:
            depth = Path(folder) / 'depth.r32f'
            depth.write_bytes(b'previous depth')
            alias = Path(folder) / 'depth-link.png'
            os.link(depth, alias)
            result = self.invoke('--scenario', json.dumps(SCENE), '--frames', '1',
                                 '--panorama-product', folder, '--image', str(alias))
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertIn('distinct files', result.stderr)
            self.assertEqual(depth.read_bytes(), b'previous depth')

    @unittest.skipUnless(os.uname().sysname == 'Darwin', 'native Metal product renderer')
    def test_far_depth_orders_prepared_near_geometry(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-near-far-') as folder:
            root = Path(folder)
            source = root / 'view.scenario'
            source.write_text(PRODUCT_SCENE)
            tile = root / 'tile.json'
            image = root / 'frame.png'
            report = root / 'report.json'
            write_panorama(root)

            def draw(*products):
                result = self.invoke('--scenario', str(source), '--frames', '1',
                                     '--image', str(image), '--output', str(report), *products)
                self.assertEqual(result.returncode, 0, result.stderr)
                return image.read_bytes(), json.loads(report.read_text())

            far = ('--panorama-product', folder)
            near = ('--tile-product', str(tile))
            background, _ = draw(*far)
            clipped = root / 'clipped'
            write_panorama(clipped, axial_near=1)
            for distance in (2, 20):
                # Both quads fill the viewport. Compare whole images to check every depth test.
                z = -distance
                half_side = distance * 2
                bounds = [[-half_side, 100-half_side, z], [half_side, 100+half_side, z]]
                tile.write_text(json.dumps({
                    'version': 1, 'coverage': [0, 0, 0], 'origin_lon_lat': [0, 0],
                    'revision': '1' * 64, 'bounds_eus_m': bounds, 'geometric_error_m': 0,
                    'vertices_eus_m': [bounds[0], [half_side, 100-half_side, z],
                                       bounds[1], [-half_side, 100+half_side, z]],
                    'normals_eus': [[0, 0, 1]] * 4, 'indices': [0, 1, 2, 0, 2, 3],
                }))
                foreground, _ = draw(*near)
                composite, metadata = draw(*far, *near)
                self.assertNotEqual(foreground, background)
                self.assertEqual(composite, foreground if distance == 2 else background)
                self.assertEqual(metadata['render_path'], 'metal_near_far')
                self.assertEqual(metadata['rendered_content']['draws_per_frame'], 2)
                self.assertEqual(metadata['rendered_content']['peak_triangles_per_frame'], 3)
                self.assertEqual(metadata['rendered_content']['near']['endpoint_triangles'], [2, 2])
                self.assertFalse(metadata['world_complete'])
                if distance == 2:
                    blended, _ = draw('--panorama-product', str(clipped), *near)
                    self.assertNotIn(blended, (background, foreground))

            packed = root / 'tile.surface'
            packer = ROOT / 'test/experiments/render/pack_surface_tile.py'
            subprocess.run([sys.executable, '-B', str(packer), str(tile), str(packed)], check=True)
            packed_image, packed_report = draw('--tile-product', str(packed))
            self.assertEqual(packed_image, foreground)
            self.assertEqual(packed_report['traffic']['asset_read_bytes'], packed.stat().st_size)
            packed.write_bytes(packed.read_bytes()[:-1])
            broken = self.invoke('--scenario', str(source), '--frames', '1',
                                 '--image', str(image), '--tile-product', str(packed))
            self.assertEqual(broken.returncode, 1, broken.stderr)
            self.assertIn('invalid packed surface tile', broken.stderr)
            self.assertEqual(image.read_bytes(), foreground)

    @unittest.skipUnless(os.uname().sysname == 'Darwin', 'native Metal product renderer')
    def test_flight_uses_a_prepared_successor_without_leaving_panorama_coverage(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-handoff-') as folder:
            root = Path(folder)
            scene = root / 'view.scenario'
            scene.write_text(PRODUCT_SCENE)
            endpoint = root / 'endpoint.scenario'
            longitude = math.degrees(math.atan2(12, 6378237))
            endpoint.write_text(PRODUCT_SCENE.replace('at lat="0" lon="0"',
                                                      f'at lat="0" lon="{longitude}"'))
            image = root / 'frame.png'
            write_panorama(root / 'first', radius=6)
            write_panorama(root / 'next', anchor=(8, 100, 0), radius=6, colour=(0, 1, 0),
                           revision='1')
            expected = self.invoke('--scenario', str(endpoint), '--frames', '1',
                                   '--panorama-product', str(root / 'next'), '--image', str(image))
            self.assertEqual(expected.returncode, 0, expected.stderr)
            endpoint_pixels = image.read_bytes()
            flight = ('--scenario', str(scene), '--frames', '12', '--linear-velocity', '30,0,0',
                      '--panorama-product', str(root / 'first'),
                      '--panorama-next', str(root / 'next'), '--image', str(image))
            result = self.invoke(*flight)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(image.read_bytes(), endpoint_pixels)
            report = json.loads(result.stdout)
            content = report['rendered_content']
            overlap = content['handoff']['blend_window_frames']
            self.assertAlmostEqual(overlap[0], 2, places=5)
            self.assertAlmostEqual(overlap[1], 6, places=5)
            self.assertEqual(content['handoff']['next']['revision'], '1' * 64)
            self.assertEqual(content['draws_per_frame'], 1)
            self.assertEqual(report['traffic']['upload_bytes'], 2 * 24 * 12)
            self.assertEqual(report['traffic']['source_delivery_bytes'], 0)

            write_panorama(root / 'first', radius=6, depth=0)
            write_panorama(root / 'next', anchor=(8, 100, 0), radius=6, colour=(0, 1, 0),
                           revision='1', depth=0)
            write_panorama(root / 'mixed', colour=(.5, .5, 0), depth=0)
            control = self.invoke('--scenario', str(scene), '--frames', '1',
                                  '--panorama-product', str(root / 'mixed'), '--image', str(image))
            self.assertEqual(control.returncode, 0, control.stderr)
            midpoint_pixels = image.read_bytes()
            middle = flight[:3] + ('4',) + flight[4:]
            result = self.invoke(*middle)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(image.read_bytes(), midpoint_pixels)

            for first_depth, next_depth in ((30, 0), (30, 60), (0, .5)):
                write_panorama(root / 'first', radius=6, depth=first_depth)
                write_panorama(root / 'next', anchor=(8, 100, 0), radius=6, colour=(0, 1, 0),
                               revision='1', depth=next_depth)
                control = self.invoke('--scenario', str(scene), '--frames', '4',
                                      '--linear-velocity', '30,0,0',
                                      '--panorama-product', str(root / 'first'), '--image', str(image))
                self.assertEqual(control.returncode, 0, control.stderr)
                foreground = image.read_bytes()
                result = self.invoke(*middle)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(image.read_bytes(), foreground)

            write_panorama(root / 'first', radius=2)
            write_panorama(root / 'next', anchor=(8, 100, 0), radius=4, revision='1')
            rejected = self.invoke(*flight)
            self.assertEqual(rejected.returncode, 1, rejected.stderr)
            self.assertIn('domains leave a gap', rejected.stderr)
            self.assertEqual(image.read_bytes(), foreground)

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

    @unittest.skipUnless(os.uname().sysname == 'Darwin', 'native Metal product renderer')
    def test_spatial_panorama_selects_the_overlapping_flight_pair_before_body_reads(self):
        with tempfile.TemporaryDirectory(prefix='outshine-panorama-index-') as folder:
            root = Path(folder)
            scene = root / 'view.scenario'
            scene.write_text(PRODUCT_SCENE)
            database = root / 'prepared.sqlite'
            image = root / 'frame.png'
            revision = 'a' * 64
            first, next_view = root / 'first', root / 'next'
            write_panorama(first, radius=6)
            write_panorama(next_view, anchor=(8, 100, 0), radius=6,
                           colour=(0, 1, 0), revision='1')

            def publish(*directories, version=revision):
                installed = subprocess.run(
                    [str(CLIENT), 'cache', 'import-panorama', str(database), version,
                     *map(str, directories)], check=False, capture_output=True, text=True, timeout=60)
                self.assertEqual(installed.returncode, 0, installed.stderr)

            flight = ('--scenario', str(scene), '--frames', '12', '--linear-velocity', '30,0,0',
                      '--image', str(image))
            direct = self.invoke(*flight, '--panorama-product', str(first), '--panorama-next',
                                 str(next_view))
            self.assertEqual(direct.returncode, 0, direct.stderr)
            pixels = image.read_bytes()
            publish(first, next_view)
            with sqlite3.connect(database) as cache:
                expected_read, = cache.execute('SELECT sum(length(bytes)) FROM package_bodies').fetchone()
            corner, distant, different = (root / name for name in ('corner', 'distant', 'other-set'))
            write_panorama(corner, anchor=(5, 95, 5), radius=6, revision='2')
            write_panorama(distant, anchor=(500, 100, 0), radius=6, revision='3')
            write_panorama(different, radius=20, revision='4')
            publish(corner, distant)
            publish(different, version='b' * 64)

            result = self.invoke(*flight, '--product-store', str(database),
                                 '--product-revision', revision)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(image.read_bytes(), pixels)
            report = json.loads(result.stdout)
            direct_report = json.loads(direct.stdout)
            self.assertEqual(report['rendered_content'], direct_report['rendered_content'])
            self.assertEqual(report['traffic']['asset_read_bytes'], expected_read)
            self.assertEqual(report['traffic']['panorama_decoded_bytes'],
                             direct_report['traffic']['panorama_decoded_bytes'])
            self.assertGreater(report['traffic']['panorama_requested_bytes'],
                               report['traffic']['panorama_decoded_bytes'])

    @unittest.skipUnless(os.uname().sysname == 'Darwin', 'native Metal product renderer')
    def test_indexed_tile_reads_only_selected_body_and_uses_parent_on_corruption(self):
        with tempfile.TemporaryDirectory(prefix='outshine-benchmark-tile-index-') as folder:
            root = Path(folder)
            scene = root / 'view.scenario'
            scene.write_text(PRODUCT_SCENE)
            database = root / 'prepared.sqlite'
            revision = 'a' * 64
            files = []
            for identity, address in (('1', [0, 0, 0]), ('2', [1, 1, 1]), ('3', [2, 0, 0])):
                source = root / f'{identity}.json'
                packed = root / f'{identity}.surface'
                source.write_text(json.dumps({
                    'version': 1, 'grid_side': 2, 'coverage': address, 'origin_lon_lat': [0, 0],
                    'revision': identity * 64, 'bounds_eus_m': [[-10, 90, -5], [10, 110, -5]],
                    'geometric_error_m': 1,
                    'vertices_eus_m': [[-10, 90, -5], [10, 90, -5], [-10, 110, -5], [10, 110, -5]],
                    'normals_eus': [[0, 0, 1]] * 4, 'indices': [0, 1, 3, 0, 3, 2],
                }))
                packer = ROOT / 'test/experiments/render/pack_surface_tile.py'
                subprocess.run([sys.executable, '-B', str(packer), str(source), str(packed)],
                               check=True)
                files.append(str(packed))
            imported = subprocess.run([str(CLIENT), 'cache', 'import-surface', str(database),
                                       revision, *files], capture_output=True, text=True, timeout=60,
                                      check=False)
            self.assertEqual(imported.returncode, 0, imported.stderr)
            arguments = ('--scenario', str(scene), '--frames', '1', '--product-store',
                         str(database), '--product-revision', revision)
            selected = self.invoke(*arguments)
            self.assertEqual(selected.returncode, 0, selected.stderr)
            report = json.loads(selected.stdout)
            self.assertEqual(report['rendered_content']['address'], [1, 1, 1])
            self.assertEqual(report['traffic']['source_delivery_bytes'], 0)
            self.assertEqual(report['traffic']['tile_decoded_bytes'], Path(files[1]).stat().st_size)
            self.assertEqual(report['traffic']['tile_requested_bytes'], Path(files[1]).stat().st_size)
            with sqlite3.connect(database) as cache:
                child, stored = cache.execute(
                    'SELECT b.key,length(b.bytes) FROM assets a, package_bodies b, asset_requests r '
                    'WHERE a.package=b.key AND r.asset_id=a.id AND a.level=1 AND a.parent IN '
                    '(SELECT request_key FROM asset_requests WHERE asset_id='
                    '(SELECT id FROM assets WHERE level=0))'
                ).fetchone()
                self.assertEqual(report['traffic']['asset_read_bytes'], stored)
                cache.execute('UPDATE package_bodies SET bytes=zeroblob(?) WHERE key=?',
                              (stored, child))
            parent = self.invoke(*arguments)
            self.assertEqual(parent.returncode, 0, parent.stderr)
            self.assertEqual(json.loads(parent.stdout)['rendered_content']['address'], [0, 0, 0])
            repaired = subprocess.run([str(CLIENT), 'cache', 'import-surface', str(database),
                                       revision, files[1]], capture_output=True, text=True,
                                      timeout=60, check=False)
            self.assertEqual(repaired.returncode, 0, repaired.stderr)
            selected_again = self.invoke(*arguments)
            self.assertEqual(selected_again.returncode, 0, selected_again.stderr)
            self.assertEqual(json.loads(selected_again.stdout)['rendered_content']['address'],
                             [1, 1, 1])
            alias = self.invoke(*arguments, '--image', str(database))
            self.assertEqual(alias.returncode, 2, alias.stderr)

    @unittest.skipUnless(os.uname().sysname == 'Darwin', 'native Metal product renderer')
    def test_surface_cut_promotes_only_a_complete_decodable_partition(self):
        with tempfile.TemporaryDirectory(prefix='outshine-surface-cut-') as folder:
            root = Path(folder)
            scene = root / 'view.scenario'
            scene.write_text(PRODUCT_SCENE)
            far = root / 'far'
            write_panorama(far, axial_near=1, overlap_end=9)
            database = root / 'prepared.sqlite'
            revision = 'a' * 64
            image = root / 'frame.png'

            def quad(address, xmin, xmax, ymin, ymax, overlay=False):
                path = root / ('-'.join(map(str, address)) + '.json')
                packed = path.with_suffix('.surface')
                bounds = [[xmin, ymin, -4], [xmax, ymax, -4]]
                recipe = {
                    'version': 1, 'grid_side': 2, 'coverage': address, 'origin_lon_lat': [0, 0],
                    'revision': '1' * 64, 'geometric_error_m': 10, 'bounds_eus_m': bounds,
                    'vertices_eus_m': [bounds[0], [xmax, ymin, -4], [xmin, ymax, -4], bounds[1]],
                    'normals_eus': [[0, 0, 1]] * 4, 'indices': [0, 1, 3, 0, 3, 2],
                    'colours_rgba8': [[80, 110, 45, 218]] * 4, 'ground_indices': 6,
                    'ground_roughness': True,
                }
                if overlay:
                    recipe['bounds_eus_m'] = [bounds[0], [xmax, ymax, -2]]
                    recipe['vertices_eus_m'] += [[0.5, 100.1, -2], [1, 100.1, -2], [1, 101, -2]]
                    recipe['normals_eus'] += [[0, 0, 1]] * 3
                    recipe['indices'] += [4, 5, 6]
                    recipe['colours_rgba8'] += [[217, 167, 24, 255]] * 3
                path.write_text(json.dumps(recipe))
                subprocess.run([sys.executable, '-B',
                                str(ROOT / 'test/experiments/render/pack_surface_tile.py'),
                                str(path), str(packed)], check=True, timeout=20)
                return packed

            columns = (-10, 25, 35)
            parent = quad([0, 0, 0], columns[0], columns[-1], 90, 110, overlay=True)
            children = [quad([1, x, y], columns[x], columns[x + 1], 90 + y * 10, 100 + y * 10,
                             overlay=x == 0 and y == 1)
                        for y in range(2) for x in range(2)]

            def publish(*paths):
                result = subprocess.run([str(CLIENT), 'cache', 'import-surface',
                                         str(database), revision, *map(str, paths)],
                                        capture_output=True, text=True, check=False, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)

            def capture(stream=False, indexed=False):
                extra = ('--stream-products',) if stream else ()
                panorama = () if indexed else ('--panorama-product', str(far))
                result = self.invoke('--scenario', str(scene), '--frames', '12' if stream else '1',
                                     '--product-store',
                                     str(database), '--product-revision', revision,
                                     '--image', str(image), *panorama, *extra)
                self.assertEqual(result.returncode, 0, result.stderr)
                return json.loads(result.stdout)

            publish(parent)
            complete = capture(stream=True)
            self.assertTrue(complete['surface_stream']['initial_cut_final'])
            self.assertEqual(complete['surface_stream']['first_refined_frame'], 0)
            self.assertEqual(complete['unrefined_frames'], 0)
            self.assertEqual(complete['traffic']['tile_requested_bytes'], parent.stat().st_size)

            publish(*children[:3])
            capture()
            unlit = image.read_bytes()
            lighting = {'to_sun_eus': [0, 0, 1], 'direct': [0.4, 0.9, 0.7],
                        'sky': [0.1, 0.2, 0.3], 'ground': [0.1, 0.1, 0.05]}
            write_panorama(far, axial_near=1, overlap_end=9, lighting=lighting)
            imported = subprocess.run([str(CLIENT), 'cache', 'import-panorama',
                                       str(database), revision, str(far)],
                                      capture_output=True, text=True, check=False, timeout=30)
            self.assertEqual(imported.returncode, 0, imported.stderr)
            partial = capture()
            pixels = image.read_bytes()
            self.assertNotEqual(pixels, unlit)
            capture(indexed=True)
            self.assertEqual(image.read_bytes(), pixels)
            self.assertEqual(partial['rendered_content']['near']['address'], [0, 0, 0])
            self.assertEqual(partial['rendered_content']['far']['axial_overlap_end_m'], 9)
            self.assertEqual(partial['rendered_content']['near']['overlay_triangles_per_frame'], 1)
            self.assertEqual(partial['traffic']['tile_requested_bytes'], parent.stat().st_size)
            waiting = capture(stream=True)
            self.assertFalse(waiting['surface_stream']['initial_cut_final'])
            self.assertEqual(waiting['surface_stream']['initial_tiles'], 1)
            self.assertEqual(waiting['surface_stream']['refined_tiles'], 1)
            self.assertEqual(waiting['surface_stream']['reused_tiles'], 1)
            self.assertEqual(waiting['traffic']['upload_bytes'], partial['traffic']['upload_bytes'])
            self.assertEqual(image.read_bytes(), pixels)

            publish(children[3])
            full = capture()
            self.assertEqual(full['rendered_content']['near']['draws_per_frame'], 4)
            self.assertEqual(full['rendered_content']['near']['overlay_triangles_per_frame'], 1)
            self.assertEqual(full['traffic']['tile_requested_bytes'],
                             sum(tile.stat().st_size for tile in children))
            self.assertEqual(image.read_bytes(), pixels)
            promoted = capture(stream=True, indexed=True)
            transition = promoted['surface_stream']
            self.assertEqual(transition['initial_tiles'], 1)
            self.assertEqual(transition['refined_tiles'], 4)
            self.assertEqual(transition['error'], '')
            self.assertGreaterEqual(transition['first_refined_frame'], 1)
            self.assertEqual(promoted['unrefined_frames'], transition['first_refined_frame'])
            self.assertEqual(promoted['profile']['pacing'], 'realtime')
            self.assertEqual(image.read_bytes(), pixels)
            with sqlite3.connect(database) as store:
                store.execute('UPDATE package_bodies SET bytes=zeroblob(length(bytes)) '
                              'WHERE key=(SELECT package FROM assets WHERE level=1 AND miny>20 '
                              'LIMIT 1)')
            recovered = capture()
            self.assertEqual(recovered['rendered_content']['near']['address'], [0, 0, 0])
            self.assertEqual(recovered['traffic']['tile_decoded_bytes'], parent.stat().st_size)
            self.assertEqual(image.read_bytes(), pixels)


if __name__ == '__main__':
    unittest.main()
