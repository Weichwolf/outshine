"""Exercise the real shader recipes and compiler in a small isolated source tree."""

import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ShaderDependencies(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='outshine-shader-dependencies-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.compiler = os.environ.get('GLSLANG', str(ROOT / 'build/deps/install/bin/glslangValidator'))
        self.write('test/scripts/shader_dependencies.py',
                   (ROOT / 'test/scripts/shader_dependencies.py').read_text())
        for name in ('src/world/sky/AtmosphereCore.h', 'src/world/sky/AtmosphereConstants.inc',
                     'src/render/stages/SceneConstants.inc', 'src/render/stages/GroundConstants.inc'):
            self.write(name, '')
        self.write('src/render/shaders/leaf.glsl', 'vec4 value() { return vec4(1.0); }\n')
        self.write('src/render/shaders/branch.glsl', '#include "leaf.glsl"\n')
        self.write('src/render/shaders/a.frag', self.shader('#include "branch.glsl"', 'value()'))
        self.write('src/render/shaders/b.frag', self.shader('', 'vec4(0.0)'))

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    @staticmethod
    def shader(include, expression):
        return ('#version 450\n#extension GL_GOOGLE_include_directive : require\n' + include +
                '\nlayout(location=0) out vec4 colour;\nvoid main() { colour=' + expression + '; }\n')

    def build(self, compiler=None, targets=None):
        return subprocess.run(['make', '-f', str(ROOT / 'Makefile'),
                               'SELF_DIR=' + str(self.root), 'GLSLANG=' + (compiler or self.compiler),
                               *(targets or ['build/shaders/a.frag.spv', 'build/shaders/b.frag.spv'])],
                              cwd=self.root, capture_output=True, text=True, timeout=30)

    def stamps(self):
        return tuple((self.root / ('build/shaders/' + name + '.frag.spv')).stat().st_mtime_ns
                     for name in ('a', 'b'))

    def assert_built(self):
        result = self.build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return self.stamps()

    def test_transitive_include_rebuilds_only_its_consumers(self):
        before = self.assert_built()
        self.assertEqual(self.assert_built(), before)
        time.sleep(1.05)
        self.write('src/render/shaders/leaf.glsl', 'vec4 value() { return vec4(0.5); }\n')
        after = self.assert_built()
        self.assertGreater(after[0], before[0])
        self.assertEqual(after[1], before[1])
        self.assertEqual(self.assert_built(), after)

    def test_missing_metadata_rebuilds_only_its_artifact(self):
        before = self.assert_built()
        (self.root / 'build/shaders/a.frag.spv.d').unlink()
        time.sleep(1.05)
        after = self.assert_built()
        self.assertGreater(after[0], before[0])
        self.assertEqual(after[1], before[1])

    def test_deleted_include_is_recompiled_and_still_missing_input_fails(self):
        self.assert_built()
        time.sleep(1.05)
        (self.root / 'src/render/shaders/leaf.glsl').unlink()
        self.write('src/render/shaders/branch.glsl', 'vec4 value() { return vec4(0.25); }\n')
        self.assert_built()
        time.sleep(1.05)
        self.write('src/render/shaders/branch.glsl', '#include "leaf.glsl"\n')
        result = self.build()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('leaf.glsl', result.stdout + result.stderr)

    def test_conditional_variant_include_does_not_rebuild_other_layouts(self):
        self.write('src/render/shaders/subjectShadowVertex.glsl',
                   '#version 450\n#extension GL_GOOGLE_include_directive : require\n'
                   '#if SHADOW_UVS == 1\n#include "leaf.glsl"\n#else\n'
                   'vec4 value() { return vec4(0.0); }\n#endif\n'
                   'void main() { gl_Position=value(); }\n')
        targets = ['build/shaders/subjectShadow-00.vert.spv',
                   'build/shaders/subjectShadow-10.vert.spv']
        result = self.build(targets=targets)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        before = [(self.root / name).stat().st_mtime_ns for name in targets]
        time.sleep(1.05)
        self.write('src/render/shaders/leaf.glsl', 'vec4 value() { return vec4(0.25); }\n')
        result = self.build(targets=targets)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        after = [(self.root / name).stat().st_mtime_ns for name in targets]
        self.assertEqual(before[0], after[0])
        self.assertGreater(after[1], before[1])

    def test_shared_cpu_gpu_include_rebuilds_its_shader(self):
        self.write('src/world/sky/AtmosphereConstants.inc',
                   'vec4 value() { return vec4(1.0); }\n')
        self.write('src/render/shaders/branch.glsl',
                   '#include "world/sky/AtmosphereConstants.inc"\n')
        before = self.assert_built()
        time.sleep(1.05)
        self.write('src/world/sky/AtmosphereConstants.inc',
                   'vec4 value() { return vec4(0.75); }\n')
        after = self.assert_built()
        self.assertGreater(after[0], before[0])
        self.assertEqual(after[1], before[1])

    def test_invalid_dependency_receipt_removes_new_output(self):
        tool = self.write('broken-compiler', '#!/bin/sh\nwhile [ "$#" -gt 0 ]; do\n'
                          'case "$1" in -o) shift; : > "$1" ;; --depfile) shift; : > "$1" ;; esac\n'
                          'shift\ndone\n')
        tool.chmod(0o755)
        result = self.build(str(tool))
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse((self.root / 'build/shaders/a.frag.spv').exists())


if __name__ == '__main__':
    unittest.main()
