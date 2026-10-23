"""Analytical scalar surfaces, cubic tangencies and explicit non-unit bounds."""

import unittest

import numpy as np
from trilinear_support import (
    CORNERS,
    evaluate,
    first_surface,
    intervals,
    lipschitz_bound,
)


class TrilinearSupportTest(unittest.TestCase):
    def test_true_unit_lipschitz_samples_do_not_give_a_unit_interpolant(self):
        values = np.linalg.norm(CORNERS, axis=1)
        value, gradient = evaluate(values, np.zeros(3), np.ones(3), np.zeros((1, 3)))
        self.assertEqual(value[0], 0)
        np.testing.assert_array_equal(gradient, [[1, 1, 1]])
        self.assertAlmostEqual(lipschitz_bound(values, np.ones(3)), np.sqrt(3))

    def trace(self, values, origins, directions, stop=None):
        origins, directions = np.asarray(origins, float), np.asarray(directions, float)
        stop = np.full(len(origins), np.inf) if stop is None else np.asarray(stop)
        hit, near, far, axes, box_near = intervals(
            np.zeros(3), np.ones(3), origins, directions, stop, 0.001
        )
        self.assertTrue(hit.all())
        return first_surface(
            values,
            np.zeros(3),
            np.ones(3),
            origins,
            directions,
            near,
            far,
            axes,
            box_near,
        )

    def test_planar_surface_has_exact_depth_and_gradient(self):
        depth, normal, unknown, _ = self.trace(
            CORNERS[:, 2] - 0.5, [[0.3, 0.4, 2]], [[0, 0, -1]]
        )
        np.testing.assert_allclose(depth, [1.5], atol=1e-12)
        np.testing.assert_allclose(normal, [[0, 0, 1]], atol=1e-12)
        self.assertFalse(unknown.any())

    def test_double_root_is_detected_without_inventing_an_opaque_entry(self):
        values = (CORNERS[:, 0] - 0.5) * (CORNERS[:, 1] - 0.5)
        depth, _, _, work = self.trace(values, [[-1, -1, 0.5]], [[1, 1, 0]])
        self.assertTrue(np.isinf(depth[0]))
        self.assertGreater(work["nonentry_roots"], 0)

    def test_triple_root_has_an_entry_but_no_stable_pbr_normal(self):
        values = -np.prod(CORNERS - 0.5, axis=1)
        depth, normal, _, _ = self.trace(values, [[-1, -1, -1]], [[1, 1, 1]])
        np.testing.assert_allclose(depth, [1.5], atol=1e-5)
        np.testing.assert_array_equal(normal, [[0, 0, 0]])

    def test_ray_in_a_zero_plane_is_not_automatically_opaque(self):
        depth, _, _, _ = self.trace(CORNERS[:, 2] - 0.5, [[-1, 0.3, 0.5]], [[1, 0, 0]])
        self.assertTrue(np.isinf(depth[0]))

    def test_clipped_negative_field_has_a_real_box_entry_cap(self):
        depth, normal, unknown, work = self.trace(
            np.full(8, -1.0), [[0.5, 0.5, 2]], [[0, 0, -1]]
        )
        np.testing.assert_array_equal(depth, [1])
        np.testing.assert_array_equal(normal, [[0, 0, 1]])
        self.assertEqual(work["box_cap_hits"], 1)
        self.assertFalse(unknown.any())

    def test_interior_near_plane_is_an_unqualified_need(self):
        depth, _, unknown, _ = self.trace(
            np.full(8, -1.0), [[0.5, 0.5, 0.5]], [[0, 0, 1]]
        )
        self.assertTrue(np.isinf(depth[0]))
        self.assertTrue(unknown[0])

    def test_positive_constant_field_does_not_invent_a_surface(self):
        depth, _, _, _ = self.trace(np.full(8, 0.01), [[0.5, 0.5, 2]], [[0, 0, -1]])
        self.assertTrue(np.isinf(depth[0]))

    def test_zero_on_a_box_face_uses_the_implicit_normal_when_defined(self):
        values = CORNERS[:, 2] - 0.5 - CORNERS[:, 0]
        depth, normal, _, _ = self.trace(values, [[-1, 0.3, 0.5]], [[1, 0, 0]])
        np.testing.assert_allclose(depth, [1])
        np.testing.assert_allclose(
            normal, np.array([[-1, 0, 1]]) / np.sqrt(2), atol=1e-12
        )
