"""Complete domain, curve/plane, allocation caps and explicit-surface contracts."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.geometry import (
    ProjectedSurfaceBudget,
    SurfaceSampler,
    conform_triangles,
    projected_height_surface,
)
from outshine_ref.geometry.projected_surface import _weld_xy
from outshine_ref.transport import (
    close_surface,
    mesh_projected_profile,
    plan_vertical_transition,
)


def field_error(mesh, curve, origin, axis, start):
    barycentric = np.array(
        [[1 / 3] * 3, [0.5, 0.5, 0], [0.5, 0, 0.5], [0, 0.5, 0.5], [0.2, 0.3, 0.5]]
    )
    sample = np.einsum("pk,fkj->fpj", barycentric, mesh.vertices[mesh.triangles])
    station = (sample[:, :, :2].reshape(-1, 2) - origin) @ axis - start
    expected = curve.evaluate(np.clip(station, 0, curve.length_m)).reshape(
        len(mesh.triangles), -1
    )
    return float(abs(expected - sample[:, :, 2]).max())


def oriented_body(mesh):
    edges = mesh.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    _, inverse, count = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    signs = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    if np.any(count != 2) or np.any(np.bincount(inverse, weights=signs) != 0):
        raise ValueError("body not closed and consistently oriented")
    triangle = mesh.vertices[mesh.triangles] - mesh.vertices[0]
    return float(
        np.einsum(
            "ij,ij->i", triangle[:, 0], np.cross(triangle[:, 1], triangle[:, 2])
        ).sum()
        / 6
    )


class ProjectedSurfaceChecks(unittest.TestCase):
    def test_arbitrary_table_has_complete_piecewise_affine_coverage(self):
        domain = shapely.box(0, -2, 10, 2)
        table = [[0, 1], [5, 2], [10, 1]]
        mesh, report = projected_height_surface(domain, [0, 0], [1, 0], table)
        self.assertTrue(report["valid"])
        points = np.array([[0, 0], [2, 0], [5, 0], [8, 1], [10, 0]])
        sampled = SurfaceSampler(mesh).sample(points)
        np.testing.assert_allclose(sampled.points_enu_m[:, 2], [1, 1.4, 2, 1.4, 1])
        self.assertTrue(report["each_face_within_one_height_chord"])

    def test_rotated_holed_and_disconnected_domains_keep_all_parts(self):
        domain = shapely.union_all(
            [
                shapely.box(-10, -5, 310, 5).difference(shapely.box(130, -1, 170, 1)),
                shapely.box(350, 10, 360, 20),
            ]
        )
        angle = 0.71
        axis = np.array([np.cos(angle), np.sin(angle)])
        matrix = np.array([[axis[0], -axis[1]], [axis[1], axis[0]]])
        origin = np.array([1e6, -700000])
        moved = shapely.set_coordinates(
            domain, shapely.get_coordinates(domain) @ matrix.T + origin
        )
        curve, _ = plan_vertical_transition(6, 0, 300, grade=0.04)
        mesh, report = mesh_projected_profile(moved, curve, origin, axis)
        self.assertIsNotNone(mesh, report)
        triangles = shapely.polygons(mesh.vertices[mesh.triangles, :2])
        self.assertTrue(shapely.coverage_is_valid(triangles))
        self.assertLess(
            shapely.coverage_union_all(triangles).symmetric_difference(moved).area, 1e-6
        )
        self.assertLessEqual(
            field_error(mesh, curve, origin, axis, 0),
            report["whole_face_height_error_bound_m"],
        )
        self.assertAlmostEqual(
            oriented_body(close_surface(mesh, 0.35)), moved.area * 0.35, places=6
        )

    def test_cubic_and_parabolic_profiles_cover_entire_face_error(self):
        domain = shapely.box(-10, -4, 410, 4)
        for method in ("parabolic", "cubic"):
            curve, _ = plan_vertical_transition(
                150, 144, 400, grade=0.03, method=method
            )
            mesh, report = mesh_projected_profile(domain, curve, [0, 0], [1, 0])
            self.assertTrue(report["analytic_c1_height_field"])
            self.assertEqual(report["mesh_continuity"], "C0")
            self.assertLessEqual(
                field_error(mesh, curve, np.zeros(2), np.array([1, 0]), 0),
                report["whole_face_height_error_bound_m"],
            )
            for end, expected in ((-5, 150), (405, 144)):
                sample = SurfaceSampler(mesh).sample([[end, 0]])
                self.assertAlmostEqual(sample.points_enu_m[0, 2], expected)

    def test_domains_outside_curve_avoid_unneeded_profile_sampling(self):
        curve, _ = plan_vertical_transition(6, 0, 300, grade=0.04)
        for bounds, expected in (((-100, -2, -20, 2), 6), ((350, -2, 400, 2), 0)):
            mesh, report = mesh_projected_profile(
                shapely.box(*bounds),
                curve,
                [0, 0],
                [1, 0],
                budget=ProjectedSurfaceBudget(max_stations=2),
            )
            self.assertIsNotNone(mesh, report)
            np.testing.assert_array_equal(mesh.vertices[:, 2], expected)
            self.assertTrue(report["sampling"]["constant_domain"])

    def test_meshes_at_different_heights_remain_explicitly_chosen(self):
        domain = shapely.box(0, 0, 10, 10)
        low, _ = projected_height_surface(domain, [0, 0], [1, 0], [[0, 0], [10, 0]])
        high, _ = projected_height_surface(domain, [0, 0], [1, 0], [[0, 5], [10, 5]])
        self.assertEqual(SurfaceSampler(low).sample([[5, 5]]).points_enu_m[0, 2], 0)
        self.assertEqual(SurfaceSampler(high).sample([[5, 5]]).points_enu_m[0, 2], 5)
        self.assertTrue(
            np.isnan(SurfaceSampler(high).sample([[20, 20]]).points_enu_m[0, 2])
        )

    def test_budgets_return_no_incomplete_surface(self):
        domain = shapely.box(0, 0, 10, 10)
        for kwargs, expected in (
            ({"time_limit_s": 0}, "projected_time_budget"),
            ({"max_boundary_vertices": 4}, "projected_boundary_vertex_budget"),
            ({"max_faces": 1}, "projected_face_budget"),
            ({"max_vertices": 3}, "projected_vertex_budget"),
            ({"max_interface_candidates": 0}, "projected_interface_candidate_budget"),
        ):
            mesh, report = projected_height_surface(
                domain,
                [0, 0],
                [1, 0],
                [[-1, 0], [11, 1]],
                budget=ProjectedSurfaceBudget(**kwargs),
            )
            self.assertIsNone(mesh)
            self.assertFalse(report["valid"])
            self.assertEqual(report["reason"], expected)
        curve, _ = plan_vertical_transition(6, 0, 300, grade=0.04)
        mesh, report = mesh_projected_profile(
            shapely.box(0, 0, 300, 4),
            curve,
            [0, 0],
            [1, 0],
            budget=ProjectedSurfaceBudget(max_stations=2),
        )
        self.assertIsNone(mesh)
        self.assertEqual(report["reason"], "projected_station_budget")

    def test_station_cap_precedes_iteration_and_numeric_conversion(self):
        class LargeTable:
            def __len__(self):
                return 1000000

            def __iter__(self):
                raise AssertionError("oversized table must not be traversed")

        mesh, report = projected_height_surface(
            shapely.box(0, 0, 10, 10), [0, 0], [1, 0], LargeTable()
        )
        self.assertIsNone(mesh)
        self.assertEqual(report["reason"], "projected_station_budget")

    def test_weld_pair_count_precedes_pair_array_allocation(self):
        from scipy.spatial import cKDTree

        tree = cKDTree([[0, 0], [1e-9, 0], [2e-9, 0]])

        class CountOnly:
            def count_neighbors(self, _other, radius):
                return tree.count_neighbors(tree, radius)

            def query_pairs(self, *_args, **_kwargs):
                raise AssertionError("pair allocation must be prevented")

        with (
            patch("scipy.spatial.cKDTree", return_value=CountOnly()),
            self.assertRaisesRegex(ValueError, "projected_weld_pair_budget"),
        ):
            _weld_xy(
                np.array([[0, 0], [1e-9, 0], [2e-9, 0]]),
                ProjectedSurfaceBudget(max_weld_pairs=0),
            )

    def test_optional_conformance_caps_preserve_default_output(self):
        xy = np.array([[0, 0], [4, 0], [0, 4], [2, 0], [2, -2]])
        faces = np.array([[0, 1, 2], [0, 4, 3]])
        expected, note = conform_triangles(xy, faces)
        bounded, limited = conform_triangles(
            xy, faces, max_candidates=100, max_faces=10
        )
        np.testing.assert_array_equal(expected, bounded)
        self.assertEqual(note, limited)
        with self.assertRaisesRegex(ValueError, "candidate budget"):
            conform_triangles(xy, faces, max_candidates=0)
        with self.assertRaisesRegex(ValueError, "face budget"):
            conform_triangles(xy, faces, max_faces=2)

    def test_invalid_geometry_reference_or_table_is_not_certified(self):
        domain = shapely.box(0, 0, 10, 10)
        for direction, table in (
            ([0, 0], [[0, 0], [10, 1]]),
            ([2, 0], [[0, 0], [10, 1]]),
            ([1, 0], [[0, 0], [0, 1]]),
            ([1, 0], [[1, 0], [9, 1]]),
        ):
            with self.assertRaises(ValueError):
                projected_height_surface(domain, [0, 0], direction, table)
        with self.assertRaises(ValueError):
            projected_height_surface(
                shapely.Polygon([(0, 0), (10, 10), (0, 10), (10, 0)]),
                [0, 0],
                [1, 0],
                [[0, 0], [10, 0]],
            )

    def test_caller_inputs_and_curve_coefficients_are_unchanged(self):
        domain = shapely.box(-10, -4, 310, 4)
        curve, _ = plan_vertical_transition(6, 0, 300, grade=0.04)
        before = domain.wkb, curve.breaks.copy(), curve.coefficients.copy()
        origin, direction = np.zeros(2), np.array([1.0, 0])
        mesh, note = mesh_projected_profile(domain, curve, origin, direction)
        self.assertIsNotNone(mesh, note)
        self.assertEqual(before[0], domain.wkb)
        np.testing.assert_array_equal(before[1], curve.breaks)
        np.testing.assert_array_equal(before[2], curve.coefficients)
        np.testing.assert_array_equal(origin, [0, 0])
        np.testing.assert_array_equal(direction, [1, 0])


if __name__ == "__main__":
    unittest.main()
