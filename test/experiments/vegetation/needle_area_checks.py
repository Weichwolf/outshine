"""Independent pixel-area integrals, source overlap, rotation and budget controls."""

import unittest
from unittest.mock import patch

import numpy as np
from needle_area_light import integrate, triangle_rule
from scatter_scene import ScatterScene


def sheet():
    return {
        "vertices": np.array([[0.0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]]),
        "faces": np.array([[0, 1, 2], [0, 2, 3]]),
        "normals": np.tile([0.0, 0, 1.0], (2, 3, 1)),
    }


class NeedleAreaChecks(unittest.TestCase):
    def test_rule_mass_and_first_moments(self):
        for order in (1, 2, 4, 8):
            bary, weights = triangle_rule(order)
            self.assertAlmostEqual(float(weights.sum()), 1, places=14)
            np.testing.assert_allclose(weights @ bary, np.full(3, 1 / 3), atol=1e-14)
            self.assertTrue((bary >= 0).all())

    def test_complete_sheet_equals_analytic_clipped_pixel_areas(self):
        source = sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        result, work = integrate(
            source, scene, np.array([0.0, 0, 1]), [[0, 0, 1]], size=4, order=2
        )
        edges = np.linspace(-0.02, 1.02, 5)
        overlap = np.maximum(np.minimum(edges[1:], 1) - np.maximum(edges[:-1], 0), 0)
        expected = np.outer(overlap, overlap) / (0.26 * 0.26)
        for field in result:
            np.testing.assert_allclose(field, expected, atol=1e-12)
        self.assertLessEqual(work["maximum_sample_batch"], 8192)

    def test_two_layers_do_not_double_count_visible_area(self):
        source = sheet()
        upper = source["vertices"] + [0, 0, 0.5]
        source = {
            "vertices": np.r_[source["vertices"], upper],
            "faces": np.r_[source["faces"], source["faces"] + 4],
            "normals": np.tile([0.0, 0, 1.0], (4, 3, 1)),
        }
        scene = ScatterScene(source["vertices"], source["faces"])
        result, _ = integrate(
            source, scene, np.array([0.0, 0, 1]), [[0, 0, 1]], size=4, order=2
        )
        self.assertAlmostEqual(float(result[0].mean()), 1 / 1.04**2, places=12)
        np.testing.assert_allclose(result[0], result[2], atol=1e-12)

    def test_budget_before_source_ray_work(self):
        source = sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        with (
            patch.object(
                scene, "nearest", side_effect=AssertionError("ray before budget")
            ),
            self.assertRaisesRegex(ValueError, "budget"),
        ):
            integrate(
                source,
                scene,
                np.array([0.0, 0, 1]),
                [[0, 0, 1]],
                size=4,
                order=2,
                max_samples=1,
            )

    def test_edge_on_sheet_is_zero(self):
        source = sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        result, work = integrate(
            source, scene, np.array([1.0, 0, 0]), [[0, 0, 1]], size=4
        )
        np.testing.assert_array_equal(result, np.zeros((3, 4, 4)))
        self.assertEqual(work["sampled_points"], 0)

    def test_rotation_preserves_projected_integral(self):
        source = sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        first, _ = integrate(source, scene, np.array([0.0, 0, 1]), [[0, 0, 1]], size=4)
        rotation = np.array([[0.0, 1, 0], [0, 0, 1], [1, 0, 0]])
        source = {
            k: v @ rotation if k in ("vertices", "normals") else v
            for k, v in source.items()
        }
        scene = ScatterScene(source["vertices"], source["faces"])
        second, _ = integrate(source, scene, np.array([1.0, 0, 0]), [[1, 0, 0]], size=4)
        np.testing.assert_allclose(first, second, atol=1e-12)


if __name__ == "__main__":
    unittest.main()
