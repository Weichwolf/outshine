#!/usr/bin/env python3
"""Compile independent SDL-layout fixtures, then break the actual package and reflection tool."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / 'test/scripts/shader_artifacts.py'
GLSLANG = os.environ.get('GLSLANG', str(ROOT / 'build/deps/install/bin/glslangValidator'))

GRAPHICS = '''#version 450
layout(set = RESOURCE_SET, binding = 0) uniform sampler2D sampled;
layout(rgba32f, set = RESOURCE_SET, binding = 1) readonly uniform image2D stored;
layout(std430, set = RESOURCE_SET, binding = 2) readonly buffer Input { vec4 value; } incoming;
layout(std140, set = UNIFORM_SET, binding = 0) uniform Params { vec4 value; } params;
OUTPUT_DECLARATION
void main() { OUTPUT = texture(sampled, vec2(0)) + imageLoad(stored, ivec2(0)) + incoming.value + params.value; }
'''
VERTEX = GRAPHICS.replace('RESOURCE_SET', '0').replace('UNIFORM_SET', '1').replace(
    'OUTPUT_DECLARATION', '').replace('OUTPUT', 'gl_Position')
FRAGMENT = GRAPHICS.replace('RESOURCE_SET', '2').replace('UNIFORM_SET', '3').replace(
    'OUTPUT_DECLARATION', 'layout(location = 0) out vec4 colour;').replace('OUTPUT', 'colour')
COMPUTE = '''#version 450
layout(local_size_x = 1, local_size_y = 2, local_size_z = 1) in;
layout(set = 0, binding = 0) uniform sampler2D sampled;
layout(rgba32f, set = 0, binding = 1) readonly uniform image2D stored;
layout(std430, set = 0, binding = 2) readonly buffer Input { vec4 value; } incoming;
layout(rgba32f, set = 1, binding = 0) writeonly uniform image2D targetImage;
layout(std430, set = 1, binding = 1) buffer Output { vec4 value; } outgoing;
layout(std140, set = 2, binding = 0) uniform Params { vec4 value; } params;
void main() {
  vec4 value = texture(sampled, vec2(0)) + imageLoad(stored, ivec2(0)) + incoming.value + params.value;
  imageStore(targetImage, ivec2(0), value);
  outgoing.value = value;
}
'''


class ShaderArtifacts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='outshine-shader-check-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.package = self.root / 'build/shaders'
        self.package.mkdir(parents=True)
        self.manifest = self.root / 'build/shader-artifacts.txt'
        self.names = [f'build/shaders/fixture.{stage}.spv' for stage in ('vert', 'frag', 'comp')]
        self.manifest.write_text('\n'.join(self.names) + '\n')
        self.contracts = self.root / 'build/compute-shaders.json'
        self.contract = {'artifact': self.names[2], 'shape': {
            'samplers': 1, 'readonly_textures': 1, 'readwrite_textures': 1,
            'readonly_buffers': 1, 'readwrite_buffers': 1, 'uniform_buffers': 1,
            'group_x': 1, 'group_y': 2, 'group_z': 1}}
        self.contracts.write_text(json.dumps([self.contract]))
        for stage, source in (('vert', VERTEX), ('frag', FRAGMENT), ('comp', COMPUTE)):
            self.compile(stage, source)

    def compile(self, stage, source):
        path = self.root / ('source.' + stage)
        path.write_text(source)
        result = subprocess.run([GLSLANG, '-V', '--target-env', 'vulkan1.0', str(path), '-o',
                                 str(self.package / ('fixture.' + stage + '.spv'))],
                                text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def check(self, accepted, *options):
        result = subprocess.run([sys.executable, str(CHECKER), '--root', str(self.root),
                                 '--report', str(self.root / 'report'), *options],
                                text=True, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0 if accepted else 1, result.stdout + result.stderr)
        return json.loads((self.root / 'report/shader-audit.json').read_text())

    def test_real_stages_and_complete_resource_order(self):
        report = self.check(True)
        self.assertEqual(report['valid'], 3)
        self.assertEqual(report['reflected'], 3)
        self.assertEqual(report['compute_contracts']['valid'], 1)
        self.assertEqual(len(report['tool_sha256']), 64)
        self.assertEqual(sorted(len(row['resources']) for row in report['artifacts']), [4, 4, 6])
        self.assertTrue(all(len(row['sha256']) == 64 for row in report['artifacts']))

    def test_missing_graphics_and_compute_artifacts_are_not_rebuilt_or_skipped(self):
        for name in self.names:
            with self.subTest(name=name):
                path = self.root / name
                original = path.read_bytes()
                path.unlink()
                report = self.check(False)
                self.assertEqual(report['checked'], 3)
                self.assertEqual(report['valid'], 2)
                self.assertFalse(path.exists())
                path.write_bytes(original)

    def test_empty_duplicate_partial_or_escaping_inventory_and_undeclared_artifact(self):
        original = self.manifest.read_text()
        for content in ('', original + self.names[0] + '\n', self.names[0] + '\n',
                        '../outside.vert.spv\n', 'build/shaders/fixture.geom.spv\n'):
            with self.subTest(content=content):
                self.manifest.write_text(content)
                self.assertTrue(self.check(False)['errors'])
        self.manifest.write_text(original)
        shutil.copyfile(self.root / self.names[0], self.package / 'undeclared.vert.spv')
        report = self.check(False)
        self.assertEqual(report['valid'], 3)
        self.assertIn('undeclared shader artifact', report['errors'][0])

    def test_truncated_corrupt_and_wrong_stage_modules(self):
        path = self.root / self.names[0]
        original = path.read_bytes()
        for content in (b'', original[:-1], b'BAD!' + original[4:],
                        (self.root / self.names[1]).read_bytes()):
            with self.subTest(length=len(content)):
                path.write_bytes(content)
                self.assertEqual(self.check(False)['valid'], 2)
        path.write_bytes(original)

    def test_wrong_sets_gaps_duplicate_bindings_and_resource_order(self):
        mutations = [
            COMPUTE.replace('set = 0, binding = 0', 'set = 3, binding = 0'),
            COMPUTE.replace('set = 0, binding = 0', 'set = 0, binding = 7'),
            COMPUTE.replace('set = 0, binding = 2', 'set = 0, binding = 1'),
            COMPUTE.replace('set = 0, binding = 0', 'set = 0, binding = TEMP').replace(
                'set = 0, binding = 1', 'set = 0, binding = 0').replace('TEMP', '1'),
            COMPUTE.replace('readonly buffer Input', 'buffer Input'),
        ]
        for source in mutations:
            with self.subTest(source=source):
                self.compile('comp', source)
                self.assertEqual(self.check(False)['valid'], 2)

    def test_graphics_sets_and_write_access(self):
        for stage, source in (('vert', VERTEX), ('frag', FRAGMENT)):
            for altered in (source.replace('readonly buffer Input', 'buffer Input'),
                            source.replace('binding = 2', 'binding = 8'),
                            source.replace('set = 1', 'set = 3') if stage == 'vert' else
                            source.replace('set = 3', 'set = 1')):
                with self.subTest(stage=stage, altered=altered):
                    self.compile(stage, altered)
                    self.assertEqual(self.check(False)['valid'], 2)
            self.compile(stage, source)

    def test_unsupported_descriptor_arrays_push_constants_and_workgroup_specialization(self):
        self.compile('vert', VERTEX.replace('sampled;', 'sampled[2];').replace('texture(sampled,',
                                                                             'texture(sampled[0],'))
        self.assertEqual(self.check(False)['valid'], 2)
        self.compile('vert', VERTEX.replace('layout(std140, set = 1, binding = 0)',
                                            'layout(push_constant)'))
        self.assertEqual(self.check(False)['valid'], 2)
        self.compile('vert', VERTEX)
        self.compile('comp', COMPUTE.replace('local_size_x = 1', 'local_size_x_id = 0'))
        self.assertEqual(self.check(False)['valid'], 2)

    def test_every_compute_contract_field_is_checked_against_real_reflection(self):
        for field in self.contract['shape']:
            with self.subTest(field=field):
                changed = json.loads(json.dumps(self.contract))
                changed['shape'][field] += 1
                self.contracts.write_text(json.dumps([changed]))
                report = self.check(False)
                self.assertEqual(report['valid'], 3)
                self.assertEqual(report['compute_contracts']['checked'], 1)
                self.assertEqual(report['compute_contracts']['valid'], 0)
                self.assertTrue(any('shape.' + field in error for error in report['errors']))

    def test_compute_catalog_coverage_and_schema_cannot_be_skipped(self):
        bad_shape = json.loads(json.dumps(self.contract))
        del bad_shape['shape']['group_z']
        unbuilt = dict(self.contract, artifact='build/shaders/unbuilt.comp.spv')
        for content in ('', '[]', '{}', json.dumps([self.contract, self.contract]),
                        json.dumps([unbuilt]), json.dumps([bad_shape])):
            with self.subTest(content=content):
                self.contracts.write_text(content)
                report = self.check(False)
                self.assertTrue(report['compute_contracts']['errors'])
        self.contracts.unlink()
        self.assertTrue(self.check(False)['compute_contracts']['errors'])

    def test_missing_failing_malformed_and_timed_out_tool(self):
        self.assertTrue(self.check(False, '--tool', str(self.root / 'absent-tool'))['errors'])
        tool = self.root / 'injected-tool'
        for body in ('import sys; sys.exit(9)', 'print("not JSON")',
                     'import time; time.sleep(5)'):
            with self.subTest(body=body):
                tool.write_text('#!' + sys.executable + '\n' + body + '\n')
                tool.chmod(0o755)
                timeout = '0.2' if 'time.sleep' in body else '30'
                report = self.check(False, '--tool', str(tool), '--timeout', timeout)
                self.assertEqual(report['checked'], 3)
                self.assertEqual(report['valid'], 0)
                for artifact in report['artifacts']:
                    if 'sys.exit' in body:
                        self.assertEqual(artifact.get('returncode'), 9, artifact)
                    elif 'not JSON' in body:
                        self.assertEqual(artifact.get('returncode'), 0, artifact)
                        self.assertFalse(artifact['reflected'])
                    else:
                        self.assertIn('timed out', artifact['error'])


if __name__ == '__main__':
    unittest.main()
