"""Check bounded reference passes retain total energy, named AOVs and full image."""

import unittest

import drjit as dr
import mitsuba as mi
import numpy as np
from mitsuba_sampling import integrator


class SamplingControls(unittest.TestCase):
    def test_invalid_limits_fail_before_render(self):
        for limit in (True, 0, -1, 3, 64, 4.0):
            with self.assertRaises(ValueError):
                integrator({"samples_per_pass": limit}, 32)

    def test_multiple_passes_preserve_emission_and_named_aovs(self):
        mi.set_variant("llvm_ad_rgb")
        dr.set_thread_count(2)
        records = []
        for limit in (None, 4):
            scene = mi.load_dict(
                {
                    "type": "scene",
                    "integrator": integrator(
                        {"samples_per_pass": limit, "denoise": True}, 32
                    ),
                    "sensor": {
                        "type": "perspective",
                        "sampler": {"type": "independent", "sample_count": 32},
                        "film": {
                            "type": "hdrfilm",
                            "width": 64,
                            "height": 32,
                            "rfilter": {"type": "box"},
                            "pixel_format": "rgb",
                        },
                    },
                    "plane": {
                        "type": "rectangle",
                        "to_world": mi.ScalarTransform4f()
                        .translate([0, 0, 3])
                        .rotate([0, 1, 0], 180)
                        .scale(10),
                        "bsdf": {"type": "diffuse", "reflectance": 0.0},
                        "emitter": {
                            "type": "area",
                            "radiance": {"type": "rgb", "value": [0.8, 0.6, 0.4]},
                        },
                    },
                }
            )
            image = mi.render(scene, spp=32, seed=17)
            dr.eval(image)
            image = np.asarray(image)
            layers = {
                name: np.asarray(bitmap)
                for name, bitmap in scene.sensors()[0].film().bitmap().split()
            }
            self.assertEqual(image.shape[:2], (32, 64))
            self.assertEqual(scene.sensors()[0].sampler().sample_count(), 32)
            self.assertTrue({"image", "albedo", "normal"} <= layers.keys())
            np.testing.assert_allclose(
                layers["image"][..., :3], image[..., :3], rtol=1e-6, atol=1e-7
            )
            np.testing.assert_allclose(
                image[..., :3],
                np.broadcast_to([0.8, 0.6, 0.4], (32, 64, 3)),
                atol=2e-6,
                rtol=1e-6,
            )
            np.testing.assert_allclose(layers["albedo"], 0.0, atol=1e-7)
            np.testing.assert_allclose(layers["normal"][..., 2], -1.0, atol=1e-7)
            records.append(image)
        np.testing.assert_allclose(records[0], records[1], atol=2e-6, rtol=1e-6)


if __name__ == "__main__":
    unittest.main()
