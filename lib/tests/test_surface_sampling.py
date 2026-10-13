"""Explicit selected surface, shared contacts, ambiguity and bounded omissions."""

from dataclasses import FrozenInstanceError
import unittest

import numpy as np

from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler


class Contacts(unittest.TestCase):
    def setUp(self):
        self.mesh = Mesh(np.array([[0., 0., 10.], [2, 0, 10.2], [2, 2, 10.6], [0, 2, 10.4]]),
                         np.array([[0, 1, 2], [0, 2, 3]]))

    def test_affine_heights_upward_normals_shared_edges_and_vertices(self):
        xy = np.array([[.5, .5], [1., 1.], [0., 0.], [1.8, .2], [.2, 1.8]])
        result = SurfaceSampler(self.mesh).sample(xy)
        np.testing.assert_allclose(result.points_enu_m[:, 2], 10 + xy @ [.1, .2], atol=1e-12)
        normal = np.array([-.1, -.2, 1.]) / np.sqrt(1.05)
        np.testing.assert_allclose(result.normals_enu, np.tile(normal, (len(xy), 1)), atol=1e-12)
        self.assertEqual(result.face_indices.tolist(), [0, 0, 0, 0, 1])
        self.assertEqual(result.reasons, ("",) * len(xy))

    def test_inputs_owned_and_prepared_output_immutable(self):
        sampler = SurfaceSampler(self.mesh)
        xy = np.array([[1., 1.]])
        result = sampler.sample(xy)
        xy[:] = 5
        np.testing.assert_array_equal(result.points_enu_m[0, :2], [1., 1.])
        with self.assertRaises(ValueError):
            result.points_enu_m[0, 2] = 0
        with self.assertRaises(TypeError):
            result.report["valid"] = 0
        with self.assertRaises(FrozenInstanceError):
            sampler.xy_tolerance_m = 1.

    def test_winding_translation_and_triangle_normal_are_not_smoothed(self):
        p = self.mesh.vertices.copy()
        p[3, 2] += .1
        p[:, :2] += [1e6, -2e6]
        mesh = Mesh(p, self.mesh.triangles[:, ::-1])
        result = SurfaceSampler(mesh).sample(np.array([[1., 1.], [.1, 1.9]]) + [1e6, -2e6])
        self.assertEqual(result.face_indices.tolist(), [0, 1])
        self.assertGreater(np.linalg.norm(result.normals_enu[0] - result.normals_enu[1]), .01)
        self.assertTrue(np.all(result.normals_enu[:, 2] > 0))

    def test_outside_and_metric_tolerance_do_not_become_zero(self):
        result = SurfaceSampler(self.mesh, xy_tolerance_m=1e-6).sample(np.array([[-1e-7, 1.], [-1e-4, 1.]]))
        self.assertAlmostEqual(result.points_enu_m[0, 2], 10.2 - 1e-8)
        self.assertEqual(result.reasons[1], "outside_surface")
        self.assertTrue(np.isnan(result.points_enu_m[1, 2]))
        self.assertTrue(np.isnan(result.normals_enu[1]).all())
        self.assertEqual(result.face_indices[1], -1)

    def test_stacked_or_coincident_unshared_faces_are_ambiguous(self):
        for offset in (0., 5.):
            p = np.vstack((self.mesh.vertices[:3], self.mesh.vertices[:3] + [0, 0, offset]))
            result = SurfaceSampler(Mesh(p, np.array([[0, 1, 2], [3, 4, 5]]))).sample(np.array([[1.5, .5], [1., 1.]]))
            self.assertEqual(result.reasons, ("ambiguous_projected_contact",) * 2)
            self.assertTrue(np.isnan(result.points_enu_m[:, 2]).all())

    def test_candidate_budget_preserves_previous_contacts(self):
        sampler = SurfaceSampler(self.mesh)
        result = sampler.sample(np.array([[1.8, .2], [.2, 1.8], [1., 1.]]), max_candidates=2)
        self.assertEqual(result.reasons[0], "")
        self.assertEqual(result.reasons[1:], ("candidate_budget_exhausted",) * 2)
        self.assertLessEqual(result.report["candidates"], 2)
        self.assertTrue(np.isnan(result.points_enu_m[1:, 2]).all())

    def test_degenerate_vertical_duplicate_faces_and_bad_inputs_rejected(self):
        for p in (np.array([[0., 0., 0.], [1, 0, 0], [2, 0, 0]]),
                  np.array([[0., 0., 0.], [1, 0, 0], [1, 0, 2]])):
            with self.assertRaises(ValueError):
                SurfaceSampler(Mesh(p, np.array([[0, 1, 2]])))
        with self.assertRaises(ValueError):
            SurfaceSampler(Mesh(self.mesh.vertices, np.array([[0, 1, 2], [2, 0, 1]])))
        with self.assertRaises(ValueError):
            SurfaceSampler(self.mesh, max_faces=1)
        for points in (np.array([[0, np.nan]]), np.ones((3, 3))):
            with self.assertRaises(ValueError):
                SurfaceSampler(self.mesh).sample(points)
        with self.assertRaises(ValueError):
            SurfaceSampler(self.mesh).sample(np.zeros((2, 2)), max_queries=1)

    def test_empty_requests_and_empty_surface(self):
        mesh = Mesh(np.empty((0, 3)), np.empty((0, 3), dtype=int))
        result = SurfaceSampler(mesh).sample(np.array([[2., 3.]]))
        self.assertEqual(result.reasons, ("outside_surface",))
        self.assertTrue(np.isnan(result.points_enu_m[0, 2]))
        self.assertEqual(result.report["candidates"], 0)
        result = SurfaceSampler(self.mesh).sample(np.empty((0, 2)))
        self.assertEqual(result.points_enu_m.shape, (0, 3))

    def test_numeric_limits_and_zero_candidate_budget(self):
        result = SurfaceSampler(self.mesh).sample(np.array([[1., 1.]]), max_candidates=0)
        self.assertEqual(result.reasons, ("candidate_budget_exhausted",))
        for options in (dict(xy_tolerance_m=-1), dict(height_tolerance_m=np.nan), dict(max_faces=True)):
            with self.assertRaises(ValueError):
                SurfaceSampler(self.mesh, **options)
        p = np.array([[0., 0., 0.], [1e200, 0, 0], [0, 1e200, 1.]])
        with self.assertRaises(ValueError):
            SurfaceSampler(Mesh(p, np.array([[0, 1, 2]])))


if __name__ == "__main__":
    unittest.main()
