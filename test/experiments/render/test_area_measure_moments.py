"""Closed-form polygons, mixture moments, normal identity and nonlinear limits."""

import unittest

import numpy as np
import shapely
from area_measure_moments import convex_moments, program, reduce_moments, sigma_points
from area_measure_render import evaluate
from area_measure_render import program as full_program
from packed_normals import pack, unpack
from visible_normal_material import tables


class MomentTests(unittest.TestCase):
    def test_triangle_matches_closed_form_and_reverse_winding(self):
        vertices = np.array([[0, 0], [1, 0], [0, 1]], float) + 1e6
        for ring in (vertices, vertices[::-1]):
            mass, mean, covariance, _ = convex_moments(
                shapely.Polygon(ring), np.full(2, 1e6)
            )
            self.assertEqual(mass, 0.5)
            np.testing.assert_allclose(mean, [1 / 3, 1 / 3], atol=1e-15)
            np.testing.assert_allclose(
                covariance, [[1 / 18, -1 / 36], [-1 / 36, 1 / 18]], atol=1e-15
            )

    def test_rectangle_matches_independent_affine_uniform_moments(self):
        affine = np.array([[2.0, 0.5], [-0.25, 1.0]])
        points = np.array([[0, 0], [1, 0], [1, 1], [0, 1]]) @ affine.T + [3, 7]
        mass, mean, covariance, _ = convex_moments(
            shapely.Polygon(points), np.array([3, 7])
        )
        self.assertAlmostEqual(mass, abs(np.linalg.det(affine)))
        np.testing.assert_allclose(mean, affine @ [0.5, 0.5], atol=1e-15)
        np.testing.assert_allclose(
            covariance, affine @ (np.eye(2) / 12) @ affine.T, atol=1e-15
        )

    def test_disconnected_mixture_matches_full_weighted_samples(self):
        rng = np.random.default_rng(73)
        xy = rng.uniform(-0.5, 0.5, (100, 2))
        weights = rng.uniform(0.01, 2, 100)
        groups = rng.integers(0, 4, 100)
        values = np.column_stack((weights, xy, np.zeros((100, 3))))
        keys, mass, mean, covariance = reduce_moments(groups, values)
        for at, key in enumerate(keys):
            keep = groups == key
            w = weights[keep] / weights[keep].sum()
            expected_mean = w @ xy[keep]
            self.assertAlmostEqual(mass[at], weights[keep].sum())
            np.testing.assert_allclose(mean[at], expected_mean, atol=1e-15)
            expected_second = np.einsum("n,ni,nj->ij", w, xy[keep], xy[keep])
            np.testing.assert_allclose(
                covariance[at] + np.outer(mean[at], mean[at]),
                expected_second,
                atol=1e-15,
            )

    def test_four_nodes_match_degree_two_but_not_skewed_cubic(self):
        _, mean, covariance, _ = convex_moments(
            shapely.Polygon([[0, 0], [1, 0], [0, 1]]), np.zeros(2)
        )
        nodes, _ = sigma_points(mean[None], covariance[None])
        np.testing.assert_allclose(nodes.mean(1)[0], mean, atol=1e-15)
        second = np.einsum("ni,nj->ij", nodes[0], nodes[0]) / 4
        np.testing.assert_allclose(
            second, [[1 / 6, 1 / 12], [1 / 12, 1 / 6]], atol=1e-15
        )
        self.assertGreater(abs(np.mean(nodes[0, :, 0] ** 3) - 1 / 10), 0.007)

    def test_invalid_covariance_and_holes_are_not_silently_repaired(self):
        with self.assertRaisesRegex(ValueError, "positive semidefinite"):
            sigma_points(np.zeros((1, 2)), np.array([[[-0.1, 0], [0, 1]]]))
        polygon = shapely.Polygon(
            [[0, 0], [2, 0], [2, 2], [0, 2]],
            [[[0.5, 0.5], [1.5, 0.5], [1.5, 1.5], [0.5, 1.5]]],
        )
        with self.assertRaisesRegex(ValueError, "without holes"):
            convex_moments(polygon, np.zeros(2))

    def test_pixel_normal_separation_and_filtered_ggx(self):
        words = pack([[0, 0, -1], [1, 0, 0]])
        arrays = {
            "child": np.array([-1]),
            "leaf": np.array([0]),
            "offsets": np.array([0, 2]),
            "normal_words": words,
            "weights": np.array([0.25, 0.75]),
        }
        geometry = {
            "eye": np.zeros(3),
            "basis": np.eye(3),
            "start": np.array([-0.01, -0.01]),
            "step": np.full(2, 0.02),
            "plane": 2.0,
        }
        view = (np.zeros(3), np.diag([1.0, -1.0, 1.0]), 100.0, 4, 4)
        box = np.array([[0, 0], [4, 4]])
        full = full_program(arrays, geometry, view, box)
        compact = program(arrays, geometry, view, box)
        self.assertEqual(compact["work"]["pixel_normal_groups"], 8)
        self.assertEqual(len(compact["rows"]), 32)
        np.testing.assert_array_equal(
            np.unique(compact["normal"], axis=0), np.unique(unpack(words), axis=0)
        )
        for roughness in (0.12, 0.45):
            a, coverage_a = evaluate(full, [4, 4], [0, 0, -1], roughness, tables())
            b, coverage_b = evaluate(compact, [4, 4], [0, 0, -1], roughness, tables())
            np.testing.assert_allclose(coverage_a, coverage_b, atol=1e-14)
            self.assertTrue(np.isfinite(a).all() and np.isfinite(b).all())
            if roughness == 0.12:
                wide_error = float(abs(a - b).max())
                self.assertGreater(wide_error, 1e-4)
        geometry["start"], geometry["step"] = np.full(2, -0.001), np.full(2, 0.002)
        view = (*view[:2], 1000.0, *view[3:])
        full = full_program(arrays, geometry, view, box)
        compact = program(arrays, geometry, view, box)
        a, _ = evaluate(full, [4, 4], [0, 0, -1], 0.12, tables())
        b, _ = evaluate(compact, [4, 4], [0, 0, -1], 0.12, tables())
        self.assertLess(float(abs(a - b).max()), wide_error / 100)


if __name__ == "__main__":
    unittest.main()
