"""Independent internal ray sums and installed Mitsuba delta-BSDF controls."""

import math
import unittest

import mitsuba as mi
import numpy as np
from thin_glass import (
    interface_reflectance,
    lateral_displacement,
    sheet_coefficients,
)


class ThinGlass(unittest.TestCase):
    def test_normal_grazing_identity_and_energy(self):
        for ior in (1.0, 1.01, 1.33, 1.52, 2.5, 3.0):
            f0 = ((ior - 1) / (ior + 1)) ** 2
            self.assertAlmostEqual(interface_reflectance(1, ior), f0)
            self.assertAlmostEqual(sheet_coefficients(1, ior)[0], 2 * f0 / (1 + f0))
            for cosine in np.linspace(0, 1, 101):
                reflected, transmitted = sheet_coefficients(cosine, ior)
                self.assertGreaterEqual(reflected, 0)
                self.assertGreaterEqual(transmitted, 0)
                self.assertAlmostEqual(reflected + transmitted, 1)
            self.assertEqual(sheet_coefficients(0, ior), (0, 1) if ior == 1 else (1, 0))

    def test_explicit_internal_ray_series(self):
        for cosine in (0.005, 0.05, 0.5, 1.0):
            f = interface_reflectance(cosine)
            reflected, transmitted, entered = f, 0.0, 1 - f
            for _ in range(10000):
                transmitted += entered * (1 - f)
                entered *= f
                reflected += entered * (1 - f)
                entered *= f
                if entered < 1e-14:
                    break
            self.assertLess(entered, 1e-14)
            np.testing.assert_allclose(
                sheet_coefficients(cosine), (reflected, transmitted), rtol=0, atol=1e-13
            )

    def test_installed_mitsuba_sampling_probabilities_and_directions(self):
        mi.set_variant("llvm_ad_rgb")
        ctx = mi.BSDFContext()
        si = mi.SurfaceInteraction3f()
        for ior in (1.01, 1.33, 1.52, 2.5):
            bsdf = mi.load_dict(
                {"type": "thindielectric", "int_ior": ior, "ext_ior": 1.0}
            )
            for cosine in (1.0, 0.8, 0.5, 0.05, 0.001):
                reflected, transmitted = sheet_coefficients(cosine, ior)
                for sign in (-1, 1):
                    si.wi = mi.Vector3f(
                        math.sqrt(1 - cosine * cosine), 0, sign * cosine
                    )
                    for sample1, expected, z in (
                        (0.0, reflected, sign * cosine),
                        (1.0, transmitted, -sign * cosine),
                    ):
                        sample, weight = bsdf.sample(ctx, si, sample1, mi.Point2f(0.5))
                        self.assertAlmostEqual(
                            float(sample.pdf[0]), expected, delta=2e-6
                        )
                        np.testing.assert_allclose(
                            np.array(sample.wo).ravel(),
                            [-math.sqrt(1 - cosine * cosine), 0, z],
                            atol=1e-6,
                        )
                        np.testing.assert_array_equal(np.array(weight), 1)
                        self.assertEqual(float(sample.eta[0]), 1)
        print(
            "Installed oracle",
            mi.__version__,
            "llvm_ad_rgb, 80 delta samples",
            flush=True,
        )

    def test_slab_offset_bound_and_snell_geometry(self):
        for cosine in (0.0, 0.05, 0.5, 1.0):
            theta = math.acos(cosine)
            inside = math.asin(math.sin(theta) / 1.52)
            expected = 0.006 * math.sin(theta - inside) / math.cos(inside)
            actual = lateral_displacement(cosine, 0.006)
            self.assertAlmostEqual(actual, expected)
            self.assertGreaterEqual(actual, 0)
            self.assertLessEqual(actual, 0.006)
        self.assertEqual(lateral_displacement(0, 0.006, 1), 0)

    def test_invalid_inputs_are_not_clamped_into_results(self):
        for cosine in (-0.1, 1.1, math.nan, math.inf):
            with self.assertRaises(ValueError):
                sheet_coefficients(cosine)
        for ior in (0, 0.99, 3.1, math.nan, math.inf):
            with self.assertRaises(ValueError):
                sheet_coefficients(0.5, ior)
        for thickness in (-0.1, math.nan, math.inf):
            with self.assertRaises(ValueError):
                lateral_displacement(0.5, thickness)


if __name__ == "__main__":
    unittest.main()
