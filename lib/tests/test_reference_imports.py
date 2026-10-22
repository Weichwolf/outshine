"""Reference hosts load the implementations they actually consume."""

import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class ReferenceImports(unittest.TestCase):
    def fresh(self, source):
        bootstrap = (
            f"import sys; sys.path.insert(0, {json.dumps(str(ROOT / 'lib/src'))})\n"
        )
        result = subprocess.run(
            [sys.executable, "-I", "-c", bootstrap + source],
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_rendering_does_not_import_generator_implementations(self):
        self.fresh("""
class BlockGenerators:
    def find_spec(self, name, *args):
        blocked = ('shapely', 'outshine_ref.contracts', 'outshine_ref.pipeline',
                   'outshine_ref.buildings', 'outshine_ref.geometry',
                   'outshine_ref.transport', 'outshine_ref.vegetation')
        if name.startswith(blocked):
            raise AssertionError('rendering requested generator dependency: ' + name)
sys.meta_path.insert(0, BlockGenerators())
from outshine_ref.rendering.blender_camera import configure_camera, verify_camera
from outshine_ref.rendering.blender_light import configure_daylight
assert all(callable(fn) for fn in (configure_camera, verify_camera, configure_daylight))
""")

    def test_consumed_exports_retain_their_original_objects(self):
        self.fresh("""
import outshine_ref as reference
from outshine_ref import LocalFrame, PreparedWorld, buildings, geometry, transport, vegetation
from outshine_ref.contracts import LocalFrame as contract
from outshine_ref.pipeline import PreparedWorld as world, prepare
assert reference.LocalFrame is LocalFrame is contract
assert reference.PreparedWorld is PreparedWorld is world
assert reference.prepare is prepare
for name, module in [('buildings', buildings), ('geometry', geometry),
                     ('transport', transport), ('vegetation', vegetation)]:
    assert getattr(reference, name) is module
""")

    def test_discovery_and_unknown_names_keep_python_module_semantics(self):
        self.fresh("""
import outshine_ref as reference
assert set(reference.__all__) <= set(dir(reference))
assert 'outshine_ref.contracts' not in sys.modules
try:
    reference.no_such_export
except AttributeError:
    pass
else:
    raise AssertionError('unknown package attribute was accepted')
assert 'outshine_ref.contracts' not in sys.modules
""")


if __name__ == "__main__":
    unittest.main()
