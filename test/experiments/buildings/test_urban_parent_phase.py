"""Analytic phase limits, rather than mirroring the quadrature implementation."""

import unittest

import numpy as np
from compare_urban_parents import camera
from compare_urban_phase import footprint_periods
from urban_parent_light import shade
from urban_parent_phase import phase_quadrature, phase_rays, render_phase
from urban_parent_stats import UrbanParent


def homogeneous_parent():
    return UrbanParent(
        np.ones((16, 16, 3)),
        np.array([0, 0, 1, 512, 16, 0.75, 0.15]),
        np.array([6.0, 12.0, 24.0, 48.0]),
        np.array([0.62, 0.28, 0.08, 0.02]),
    )


class ParentPhaseTest(unittest.TestCase):
    def test_footprint_distinguishes_resolved_and_unresolved_recipe(self):
        parent = homogeneous_parent()
        box = np.array([[638, 358], [642, 362]])
        self.assertGreater(footprint_periods(parent, camera(512, 240000, 0), box)[0], 1)
        self.assertLess(footprint_periods(parent, camera(512, 1000, 0), box)[0], 1)

    def test_vertical_area_and_light_match_analytic_mixture(self):
        parent = homogeneous_parent()
        origins = np.array([[253.0, 255.0, 100.0], [257.0, 261.0, 100.0]])
        directions = np.tile([0.0, 0.0, -1.0], (2, 1))
        profile = {
            "kind": "sun",
            "direction": [0, 0, 1],
            "intensity": 3,
            "ambient": 0.1,
        }
        expected_alpha = 0.85 * 0.75**2
        roof = shade(
            origins, np.array([0, 0, 1]), -directions, 0.6, np.ones(2), profile
        )
        ground = shade(
            origins, np.array([0, 0, 1]), -directions, 0.2, np.ones(2), profile
        )
        for side in [3, 4, 8]:
            rgb, alpha, _ = render_phase(
                parent, origins, directions, profile, side=side
            )
            np.testing.assert_allclose(alpha, expected_alpha, rtol=0, atol=2e-14)
            np.testing.assert_allclose(
                rgb,
                expected_alpha * roof + (1 - expected_alpha) * ground,
                rtol=0,
                atol=2e-14,
            )

    def test_phase_erases_subcell_translation_without_rebuilding_assets(self):
        parent = homogeneous_parent()
        origins = np.array([[235.0, 231.0, 100.0], [238.0, 234.0, 100.0]])
        directions = np.tile([0.1, 0.2, -1.0], (2, 1))
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        a, d, weights = phase_rays(parent, origins, directions, 4)
        ground = a - (a[:, 2] / d[:, 2])[:, None] * d
        np.testing.assert_allclose(ground[:16], ground[16:], rtol=0, atol=1e-13)
        np.testing.assert_allclose(weights.sum(), 1, rtol=0, atol=2e-15)
        before = {key: value.tobytes() for key, value in parent.arrays().items()}
        profile = {
            "kind": "sun",
            "direction": [0.4, -0.2, 1],
            "intensity": 3,
            "ambient": 0.1,
        }
        rgb, alpha, _ = render_phase(parent, origins, directions, profile)
        np.testing.assert_allclose(rgb[0], rgb[1], rtol=0, atol=1e-13)
        np.testing.assert_allclose(alpha[0], alpha[1], rtol=0, atol=1e-13)
        self.assertEqual(
            before, {key: value.tobytes() for key, value in parent.arrays().items()}
        )

    def test_empty_parent_and_invalid_quadrature(self):
        parent = homogeneous_parent()
        parent.centre_fractions[:] = 0
        origins = np.array([[256.0, 256.0, 100.0]])
        directions = np.array([[0.0, 0.0, -1.0]])
        profile = {
            "kind": "sun",
            "direction": [0, 0, 1],
            "intensity": 3,
            "ambient": 0.1,
        }
        rgb, alpha, _ = render_phase(parent, origins, directions, profile)
        expected = shade(
            origins, np.array([0, 0, 1]), -directions, 0.2, np.ones(1), profile
        )
        np.testing.assert_allclose(rgb, expected, rtol=0, atol=1e-14)
        np.testing.assert_array_equal(alpha, 0)
        for side, fill in [(2, 0.75), (17, 0.75), (4, 1), (4, 0)]:
            with self.assertRaises(ValueError):
                phase_quadrature(side, fill)


if __name__ == "__main__":
    unittest.main()
