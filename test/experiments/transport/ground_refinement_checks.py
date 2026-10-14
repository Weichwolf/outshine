"""Whole-child grades, unchanged contacts, paired splits and exact LOD envelope."""

import unittest

import numpy as np
import shapely

from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from ground_refinement import refine_ground
from surface_height_checks import normal_gradients


class Refinement(unittest.TestCase):
    def setUp(self):
        self.base = Mesh(np.array([[0., 0., 0.], [10., 0., 0.], [10., 10., 0.], [0., 10., 0.]]),
                         np.array([[0, 1, 2], [0, 2, 3]]))

    def audit(self, base, result, roots, grade):
        np.testing.assert_array_equal(result.vertices[:len(base.vertices)], base.vertices)
        polygons = shapely.polygons(result.vertices[result.triangles, :2])
        self.assertTrue(shapely.coverage_is_valid(polygons))
        self.assertAlmostEqual(shapely.area(polygons).sum(), 100., places=9)
        gradient = normal_gradients(result.vertices[:, :2], result.triangles, result.vertices[:, 2])
        self.assertLessEqual(np.linalg.norm(gradient, axis=1).max(), grade * 1.0000001)
        for face, root in zip(polygons, roots):
            self.assertTrue(shapely.Polygon(base.vertices[base.triangles[root], :2]).covers(face))

    def test_shared_edge_split_both_faces_and_analytic_clamp(self):
        result, roots, report = refine_ground(self.base, lambda q: np.full(len(q), 10.), max_new_vertices=1)
        self.audit(self.base, result, roots, 2 / 3)
        self.assertEqual(len(result.triangles), 4)
        self.assertAlmostEqual(result.vertices[-1, 2], 10 / 3, places=10)
        self.assertAlmostEqual(report["maximum_vertical_LOD_difference_m"], 10 / 3, places=10)
        self.assertEqual(report["reason"], "refinement_budget_exhausted")
        pair = np.array([[2., 2.], [5., 5.], [8., 8.]])
        self.assertTrue(all(not reason for reason in SurfaceSampler(result).sample(pair).reasons))

    def test_centroids_same_point_face_cost_and_bounded_heights(self):
        result, roots, report = refine_ground(self.base, lambda q: np.full(len(q), -10.), method="centroid", max_new_vertices=1)
        self.audit(self.base, result, roots, 2 / 3)
        self.assertEqual(report["new_vertices"], 1)
        self.assertEqual(len(result.triangles), 4)
        self.assertLess(result.vertices[-1, 2], 0.)

    def test_roads_and_patch_boundaries_remain_unchanged(self):
        result, roots, report = refine_ground(self.base, lambda q: np.full(len(q), 2.), protected_faces=1,
                                              method="centroid", max_new_vertices=1)
        self.audit(self.base, result, roots, 2 / 3)
        np.testing.assert_array_equal(result.triangles[0], self.base.triangles[0])
        midpoint, _, report = refine_ground(self.base, lambda q: np.full(len(q), 2.), protected_faces=1)
        self.assertIs(midpoint, self.base)
        self.assertEqual(report["reason"], "no_unprotected_candidates")

    def test_multiple_passes_keep_exact_root_envelope_and_valid_mesh(self):
        target = lambda q: 2. * np.sin(q[:, 0]) * np.cos(q[:, 1])
        result, roots, report = refine_ground(self.base, target, method="centroid", max_new_vertices=7, max_faces=20)
        self.audit(self.base, result, roots, 2 / 3)
        self.assertEqual(report["maximum_vertical_LOD_difference_m"], float(abs(result.vertices[:, 2]).max()))
        self.assertLessEqual(report["faces"], 20)
        self.assertLessEqual(report["new_vertices"], 7)

    def test_missing_targets_and_no_gain_keep_prior_valid(self):
        for target in (lambda q: np.full(len(q), np.nan), lambda q: np.zeros(len(q))):
            result, _, report = refine_ground(self.base, target)
            self.assertIs(result, self.base)
            self.assertEqual(report["new_vertices"], 0)
        result, _, report = refine_ground(self.base, lambda q: np.full(len(q), 2.), max_new_vertices=0)
        self.assertIs(result, self.base)
        self.assertEqual(report["reason"], "refinement_budget_exhausted")

    def test_nonflat_shared_crease_rotations_and_translation(self):
        p = self.base.vertices.copy()
        p[:, 2] = p[:, 0] * .1 + p[:, 1] * .2 + 165.
        p[3, 2] += .3
        p[:, :2] += [1e6, -2e6]
        mesh = Mesh(p, self.base.triangles[:, ::-1])
        result, roots, report = refine_ground(mesh, lambda q: np.full(len(q), 200.), max_new_vertices=1)
        np.testing.assert_array_equal(result.vertices[:4], mesh.vertices)
        gradient = normal_gradients(result.vertices[:, :2], result.triangles, result.vertices[:, 2])
        self.assertLessEqual(np.linalg.norm(gradient, axis=1).max(), (2 / 3) * 1.0000001)
        self.assertGreater(report["maximum_vertical_LOD_difference_m"], 0)

    def test_invalid_grades_projection_target_and_caps_rejected(self):
        steep = self.base.vertices.copy()
        steep[:, 2] = steep[:, 0]
        with self.assertRaises(ValueError):
            refine_ground(Mesh(steep, self.base.triangles), lambda q: np.zeros(len(q)))
        for options in (dict(max_faces=1), dict(max_passes=0), dict(max_new_vertices=-1), dict(error_m=np.nan)):
            with self.assertRaises(ValueError):
                refine_ground(self.base, lambda q: np.zeros(len(q)), **options)
        with self.assertRaises(ValueError):
            refine_ground(self.base, lambda q: np.zeros((len(q), 2)))

    def test_local_quality_interval_limits_a_misleading_midpoint(self):
        def target(q):
            return np.where(np.linalg.norm(q - [5., 5.], axis=1) < .1, 3., -1.)

        result, _, report = refine_ground(self.base, target, max_new_vertices=1)
        self.assertLessEqual(result.vertices[-1, 2], 3.)
        self.assertGreater(report["quality_queries"], 0)
        gradient = normal_gradients(result.vertices[:, :2], result.triangles, result.vertices[:, 2])
        self.assertLessEqual(np.linalg.norm(gradient, axis=1).max(), (2 / 3) * 1.0000001)

    def test_vertex_morph_keeps_whole_face_grades(self):
        result, _, _ = refine_ground(self.base, lambda q: np.full(len(q), 3.), max_new_vertices=1)
        zero = SurfaceSampler(self.base).sample(result.vertices[:, :2]).points_enu_m
        for fraction in (0., .25, .5, .75, 1.):
            vertices = zero * (1 - fraction) + result.vertices * fraction
            gradient = normal_gradients(vertices[:, :2], result.triangles, vertices[:, 2])
            self.assertLessEqual(np.linalg.norm(gradient, axis=1).max(), (2 / 3) * 1.0000001)


if __name__ == "__main__":
    unittest.main()
