"""The agreed Eevee daylight is explicit and does not replace owned local lights."""

import importlib.util
import unittest

import numpy as np
from outshine_ref.rendering.blender_light import configure_daylight


@unittest.skipUnless(
    importlib.util.find_spec("bpy"), "Optional Blender host is unavailable"
)
class BlenderDaylight(unittest.TestCase):
    def setUp(self):
        import bpy

        bpy.ops.wm.read_factory_settings(use_empty=True)
        self.scene = bpy.context.scene
        self.scene.render.engine = (
            "BLENDER_EEVEE" if bpy.app.version >= (5, 0, 0) else "BLENDER_EEVEE_NEXT"
        )

    def test_shared_profile_preserves_owned_area_light(self):
        import bpy

        light = bpy.data.lights.new("owned fill", "AREA")
        light.energy = 900.0
        obj = bpy.data.objects.new("owned fill", light)
        self.scene.collection.objects.link(obj)
        record = configure_daylight(self.scene)
        self.assertEqual(record["profile"], "research-daylight-v1")
        self.assertEqual(light.energy, 900.0)
        self.assertEqual(len([o for o in self.scene.objects if o.type == "LIGHT"]), 2)
        sun = next(
            o for o in self.scene.objects if o.type == "LIGHT" and o.data.type == "SUN"
        )
        np.testing.assert_allclose(sun.rotation_euler, [0.45, -0.65, -0.6], atol=1e-6)
        self.assertEqual(sun.data.energy, 2.5)
        self.assertAlmostEqual(sun.data.angle, 0.04, places=6)
        self.assertEqual(self.scene.view_settings.view_transform, "AgX")
        self.assertEqual(self.scene.view_settings.exposure, 1.5)
        background = self.scene.world.node_tree.nodes["Background"]
        np.testing.assert_allclose(
            background.inputs["Color"].default_value[:3], [0.35, 0.45, 0.6], atol=1e-7
        )
        self.assertAlmostEqual(
            background.inputs["Strength"].default_value, 0.35, places=6
        )

    def test_workbench_is_not_silently_presented_as_eevee(self):
        self.scene.render.engine = "BLENDER_WORKBENCH"
        with self.assertRaisesRegex(ValueError, "Eevee"):
            configure_daylight(self.scene)
        self.assertEqual(len(self.scene.objects), 0)


if __name__ == "__main__":
    unittest.main()
