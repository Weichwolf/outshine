"""Sphere coverage, original-address partitions and projection controls (#952)."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
from compare_surface_points import circles
from surface_point_hierarchy import (
    PointHierarchy,
    build,
    cut,
    incident_radii,
    load,
    merge_ball,
    verify_partition,
)


class SurfacePointTests(unittest.TestCase):
    def setUp(self):
        self.points = np.array([[0.0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]])
        self.faces = np.array([[0, 1, 2], [0, 2, 3]], np.uint32)
        self.view = np.array([0.0, 0, -100]), np.eye(3), 100.0, 1280, 720

    def test_incident_vertex_balls_cover_acute_obtuse_and_rotated_triangles(self):
        random = np.random.default_rng(952)
        bary = np.r_[np.eye(3), random.dirichlet(np.ones(3), 10000)]
        for points in (
            np.array([[0.0, 0, 0], [1, 0, 0], [0.5, np.sqrt(3) / 2, 0]]),
            np.array([[0.0, 0, 0], [2, 0, 0], [0.5, 0.01, 0]]),
            np.array([[3.0, 4, 5], [4, 5, 6], [3.5, 6, 5]]),
        ):
            radius, bad = incident_radii(points, np.array([[0, 1, 2]]))
            distance = np.linalg.norm((bary @ points)[:, None] - points[None], axis=2)
            self.assertTrue((np.min(distance - radius[None], axis=1) <= 1e-14).all())
            self.assertEqual(len(bad), 0)

    def test_degenerate_source_faces_have_no_surface_radius(self):
        radius, bad = incident_radii(
            np.array([[0.0, 0, 0], [1, 0, 0], [2, 0, 0]]), np.array([[0, 1, 2]])
        )
        np.testing.assert_array_equal(radius, 0)
        np.testing.assert_array_equal(bad, [0])

    def test_merged_ball_encloses_disjoint_nested_and_coincident_children(self):
        for a, ar, b, br in (
            ([0.0, 0, 0], 1, [4.0, 0, 0], 1),
            ([0.0, 0, 0], 5, [1.0, 0, 0], 1),
            ([0.0, 0, 0], 1, [0.0, 0, 0], 2),
        ):
            a, b = np.array(a), np.array(b)
            centre, radius = merge_ball(a, ar, b, br)
            self.assertGreaterEqual(radius, np.linalg.norm(centre - a) + ar)
            self.assertGreaterEqual(radius, np.linalg.norm(centre - b) + br)

    def test_coincident_points_keep_every_original_address(self):
        points = np.r_[self.points, self.points[:1]]
        faces = np.r_[self.faces, [[4, 1, 2]]]
        tree, report = build(points, faces)
        self.assertEqual(report["geometric_leaves"], 4)
        self.assertEqual(report["coincident_extra_vertex_addresses"], 1)
        np.testing.assert_array_equal(np.sort(tree.order), np.arange(5))
        for budget in (0, 1, 200000):
            rows, _ = cut(tree, self.view, node_budget=budget)
            self.assertEqual(verify_partition(tree, rows), 5)

    def test_every_internal_ball_contains_its_children(self):
        tree, _ = build(self.points, self.faces)
        for i, children in enumerate(tree.children):
            if children[0] >= 0:
                excess = (
                    np.linalg.norm(tree.centres[children] - tree.centres[i], axis=1)
                    + tree.radii[children]
                    - tree.radii[i]
                )
                self.assertLessEqual(excess.max(), 0)

    def single_ball(self, centre, radius):
        return PointHierarchy(
            np.array([centre], float),
            np.array([radius]),
            np.array([[-1, -1]], np.int32),
            np.array([0], np.uint32),
            np.array([1], np.uint32),
            np.array([0], np.uint32),
        )

    def test_projected_circle_contains_independent_sphere_points_off_axis(self):
        centre, radius = np.array([2.0, 1, 10]), 1.0
        tree = self.single_ball(centre, radius)
        view = np.zeros(3), np.eye(3), 1000.0, 1280, 720
        rows, _ = cut(tree, view)
        random = np.random.default_rng(952)
        delta = random.normal(size=(10000, 3))
        delta *= radius / np.linalg.norm(delta, axis=1)[:, None]
        points = centre + delta
        xy = [640, 360] + 1000 * points[:, :2] / points[:, 2:] * [1, -1]
        offset = np.linalg.norm(xy - rows["screen"][0, :2], axis=1)
        self.assertLessEqual(offset.max(), rows["screen"][0, 2])

    def test_oversized_leaf_is_not_accepted_as_below_target(self):
        tree = self.single_ball([0, 0, 100], 1.0)
        rows, work = cut(tree, (np.zeros(3), np.eye(3), 100.0, 1280, 720))
        self.assertEqual(work["oversized_leaves"], 1)
        self.assertEqual(work["necessarily_oversized_leaves"], 1)
        self.assertGreater(rows["minimum_sphere_diameter"][0], 1)
        self.assertEqual(verify_partition(tree, rows), 1)

    def test_minor_axis_lower_diameter_has_independent_tangent_rays(self):
        centre, radius, focal = np.array([2.0, 1, 10]), 1.0, 1000.0
        rows, _ = cut(
            self.single_ball(centre, radius), (np.zeros(3), np.eye(3), focal, 1280, 720)
        )
        divisor = centre[2] ** 2 - radius**2
        ellipse_centre = focal * centre[2] * centre[:2] / divisor
        perpendicular = np.array([-centre[1], centre[0]]) / np.linalg.norm(centre[:2])
        half = focal * radius / np.sqrt(divisor)
        endpoints = ellipse_centre + np.array([-1, 1])[:, None] * half * perpendicular
        rays = np.c_[endpoints / focal, np.ones(2)]
        discriminant = (rays @ centre) ** 2 - np.sum(rays * rays, axis=1) * (
            centre @ centre - radius**2
        )
        np.testing.assert_allclose(discriminant, 0, rtol=0, atol=1e-12)
        self.assertAlmostEqual(
            np.linalg.norm(endpoints[1] - endpoints[0]),
            rows["minimum_sphere_diameter"][0],
            delta=1e-12,
        )

    def test_eye_inside_leaf_retains_explicit_unprojectable_need(self):
        tree = self.single_ball([0, 0, 0], 1.0)
        rows, work = cut(tree, (np.zeros(3), np.eye(3), 100.0, 1280, 720))
        self.assertEqual(work["unprojectable_leaves"], 1)
        self.assertEqual(verify_partition(tree, rows), 1)

    def test_frustum_cull_and_work_budget_preserve_full_partition(self):
        tree, _ = build(self.points, self.faces)
        rows, work = cut(tree, (np.array([0, 0, 100]), np.eye(3), 100.0, 1280, 720))
        self.assertEqual(work["splats"], 0)
        self.assertEqual(verify_partition(tree, rows), 4)
        pending, work = cut(tree, self.view, node_budget=0)
        self.assertEqual(work["node_tests"], 0)
        self.assertEqual(work["unresolved_nodes"], 1)
        self.assertEqual(verify_partition(tree, pending), 4)

    def test_stored_hierarchy_query_needs_no_original_mesh(self):
        tree, _ = build(self.points, self.faces)
        expected, _ = cut(tree, self.view)
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.surface-points."
        ) as tmp:
            path = Path(tmp) / "points.npz"
            np.savez_compressed(path, **tree.arrays())
            rows, _ = cut(load(path), self.view)
            for key, value in rows.items():
                np.testing.assert_array_equal(value, expected[key])

    def test_circle_mask_and_empty_circle_list(self):
        box = np.array([[0, 0], [1, 1]])
        full, _ = circles(np.array([[0.5, 0.5, 1.0]]), box)
        empty, _ = circles(np.empty((0, 3)), box)
        np.testing.assert_array_equal(full, 1)
        np.testing.assert_array_equal(empty, 0)

    def test_invalid_profile_and_input_are_explicit(self):
        with self.assertRaises(ValueError):
            build(self.points, np.array([[0, 1, 4]]))
        tree, _ = build(self.points, self.faces)
        with self.assertRaises(ValueError):
            cut(tree, self.view, diameter_px=0)
        with self.assertRaises(ValueError):
            cut(tree, (self.view[0], np.eye(3) * 2, *self.view[2:]))


if __name__ == "__main__":
    unittest.main()
