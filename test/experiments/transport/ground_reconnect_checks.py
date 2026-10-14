"""Changed connectivity must preserve roads, fixed XYZ, coverage and full grades."""

import unittest

import numpy as np
import shapely

from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from ground_reconnect import reconnect_ground
from surface_height_checks import normal_gradients


class Reconnection(unittest.TestCase):
    def setUp(self):
        points = np.array([[0., 0., 0.], [3., 0., 0.], [2., 1., .1], [0., 2., 0.]])
        self.old = Mesh(points, np.array([[1, 2, 3], [1, 3, 0]]))
        self.reference = Mesh(points, np.array([[0, 1, 2], [0, 2, 3]]))
        sampler = SurfaceSampler(self.reference)
        self.target = lambda xy: sampler.sample(xy).points_enu_m[:, 2]

    def test_both_methods_change_only_internal_diagonal(self):
        for method in ("delaunay", "fit"):
            result, report = reconnect_ground(self.old, self.target, method=method)
            self.assertEqual(report["exchanges"], 1)
            np.testing.assert_array_equal(result.vertices, self.old.vertices)
            polygons = shapely.polygons(result.vertices[result.triangles, :2])
            self.assertTrue(shapely.coverage_is_valid(polygons))
            self.assertTrue(shapely.coverage_union_all(polygons).equals(shapely.coverage_union_all(
                shapely.polygons(self.old.vertices[self.old.triangles, :2]))))
            gradient = normal_gradients(result.vertices[:, :2], result.triangles, result.vertices[:, 2])
            self.assertLessEqual(np.linalg.norm(gradient, axis=1).max(), 2 / 3)
            queries = np.array([[1., .4], [1., 1.], [2., .5]])
            np.testing.assert_allclose(SurfaceSampler(result).sample(queries).points_enu_m[:, 2], self.target(queries), atol=1e-12)
            again, _ = reconnect_ground(result, self.target, method=method)
            np.testing.assert_array_equal(again.triangles, result.triangles)
            self.assertFalse(report["root_lod_certificate"])

    def test_road_interface_cannot_flip(self):
        result, report = reconnect_ground(self.old, self.target, protected_faces=1)
        self.assertIs(result, self.old)
        self.assertEqual(report["exchanges"], 0)

    def test_probe_regression_and_missing_data_keep_prior(self):
        targets = (lambda xy: SurfaceSampler(self.old).sample(xy).points_enu_m[:, 2], lambda xy: np.full(len(xy), np.nan))
        for target in targets:
            result, report = reconnect_ground(self.old, target)
            self.assertIs(result, self.old)
            self.assertEqual(report["exchanges"], 0)

    def test_caps_prevent_target_work(self):
        def unavailable(_):
            raise AssertionError("budget exhausted before target query")
        for kw in (dict(max_passes=0), dict(max_quality_queries=59), dict(time_limit_s=0.)):
            result, report = reconnect_ground(self.old, unavailable, **kw)
            self.assertIs(result, self.old)
            self.assertEqual(report["quality_queries"], 0)
        for kw in (dict(max_faces=1), dict(max_vertices=3), dict(protected_faces=3), dict(max_passes=-1),
                   dict(method="bad"), dict(grade=np.nan), dict(time_limit_s=-1.)):
            with self.assertRaises(ValueError):
                reconnect_ground(self.old, unavailable, **kw)

    def test_cocircular_flat_tie_does_not_oscillate(self):
        mesh = Mesh(np.array([[0., 0., 0.], [1., 0., 0.], [1., 1., 0.], [0., 1., 0.]]), np.array([[0, 1, 2], [0, 2, 3]]))
        for method in ("fit", "delaunay"):
            result, report = reconnect_ground(mesh, lambda xy: np.zeros(len(xy)), method=method)
            self.assertIs(result, mesh)
            self.assertEqual(report["exchanges"], 0)

    def test_concave_quad_preserves_prior(self):
        mesh = Mesh(np.array([[0., 0., 0.], [2., 0., 0.], [.3, .3, .1], [0., 2., 0.]]), np.array([[0, 1, 2], [0, 2, 3]]))
        result, report = reconnect_ground(mesh, lambda xy: np.zeros(len(xy)))
        self.assertIs(result, mesh)
        self.assertEqual(report["exchanges"], 0)

    def test_translated_clockwise_input_has_same_solution(self):
        offset = np.array([810000., -710000., 153.])
        old = Mesh(self.old.vertices + offset, self.old.triangles[:, [0, 2, 1]])
        target = lambda xy: self.target(xy - offset[:2]) + offset[2]
        result, report = reconnect_ground(old, target)
        self.assertEqual(report["exchanges"], 1)
        np.testing.assert_array_equal(result.vertices, old.vertices)
        gradient = normal_gradients(result.vertices[:, :2], result.triangles, result.vertices[:, 2])
        self.assertLessEqual(np.linalg.norm(gradient, axis=1).max(), 2 / 3)

    def test_steep_proposal_is_rejected_before_target_query(self):
        points = self.old.vertices.copy()
        points[:, 2] = [.5474104532542898, .9956241198452893, .4385093756323071, -.7337032184671826]
        mesh = Mesh(points, self.old.triangles)
        def unavailable(_):
            raise AssertionError("proposed face exceeds whole-face grade")
        result, report = reconnect_ground(mesh, unavailable)
        self.assertIs(result, mesh)
        self.assertEqual(report["exchanges"], 0)

    def test_batch_preserves_two_disjoint_patches_and_query_cap(self):
        vertices = np.vstack((self.old.vertices, self.old.vertices + [10., 0., 0.]))
        faces = np.vstack((self.old.triangles, self.old.triangles + 4))
        reference = SurfaceSampler(Mesh(vertices, np.vstack((self.reference.triangles, self.reference.triangles + 4))))
        target = lambda xy: reference.sample(xy).points_enu_m[:, 2]
        for cap, expected in ((60, 1), (120, 2)):
            mesh, report = reconnect_ground(Mesh(vertices, faces), target, max_quality_queries=cap)
            self.assertEqual(report["exchanges"], expected)
            self.assertLessEqual(report["quality_queries"], cap)
            np.testing.assert_array_equal(mesh.vertices, vertices)
            self.assertTrue(shapely.coverage_is_valid(shapely.polygons(mesh.vertices[mesh.triangles, :2])))


if __name__ == "__main__":
    unittest.main()
