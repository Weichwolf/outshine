"""Independent physical dimensions, topology, transforms and rejected rail cases."""

import unittest

import numpy as np
import shapely
from outshine_ref.transport.rail_hardware import RailBudget, RailRecipe
from outshine_ref.transport.rail_hardware import bake_rail_sleepers as bake_sleepers
from outshine_ref.transport.rail_hardware import build_rail_hardware as build_track
from outshine_ref.transport.rail_hardware._geometry import SECTION, SLEEPER


def closed_volume(mesh):
    a, b, c = mesh.triangles.T
    edges = np.vstack((np.c_[a, b], np.c_[b, c], np.c_[c, a]))
    canonical = np.sort(edges, axis=1)
    _, inverse, counts = np.unique(
        canonical, axis=0, return_inverse=True, return_counts=True
    )
    if not np.all(counts == 2):
        raise AssertionError("body is not a closed indexed two-manifold")
    signs = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    if np.any(np.bincount(inverse, weights=signs) != 0):
        raise AssertionError("body faces are not consistently oriented")
    xyz = mesh.vertices - mesh.vertices[0]
    return np.einsum("ij,ij->i", xyz[a], np.cross(xyz[b], xyz[c])).sum() / 6


def assert_gauge(test, xy, mesh, gauge):
    rings = mesh.vertices.reshape(2, len(xy), len(SECTION), 3)
    gap = rings[1, :, 7, :2] - rings[0, :, 6, :2]
    delta = np.diff(xy, axis=0)
    tangent = delta / np.linalg.norm(delta, axis=1)[:, None]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    for side in (gap[:-1], gap[1:]):
        np.testing.assert_allclose(
            np.einsum("ij,ij->i", side, normal), gauge, atol=2e-10
        )
    test.assertTrue(np.isfinite(mesh.vertices).all())


class RailHardware(unittest.TestCase):
    def test_straight_gauge_section_and_positive_manifold_volume(self):
        xy = np.array([[0.0, 0], [20, 0]])
        product, report = build_track(xy, 7.0)
        self.assertTrue(report["valid"])
        assert_gauge(self, xy, product.rails, 1.435)
        expected_area = float(shapely.Polygon(SECTION).area)
        self.assertAlmostEqual(
            closed_volume(product.rails), 40 * expected_area, places=11
        )
        self.assertAlmostEqual(product.rails.vertices[:, 2].max(), 7.352, places=12)
        self.assertAlmostEqual(product.rails.vertices[:, 2].min(), 7.18, places=12)

    def test_curved_miter_preserves_both_segment_gauges_and_volume(self):
        xy = np.array([[0.0, 0], [10, 0], [20, 3]])
        before = xy.copy()
        for points in (xy, xy[::-1]):
            product, report = build_track(points, -3.0)
            self.assertTrue(report["valid"])
            assert_gauge(self, points, product.rails, 1.435)
            expected = (
                2
                * shapely.Polygon(SECTION).area
                * np.linalg.norm(np.diff(xy, axis=0), axis=1).sum()
            )
            self.assertAlmostEqual(closed_volume(product.rails), expected, places=11)
        np.testing.assert_array_equal(xy, before)

    def test_rigid_xy_transform_and_nondefault_gauge(self):
        xy = np.array([[0.0, 0], [10, 0], [20, 3]])
        rotation = np.array([[0.6, -0.8], [0.8, 0.6]])
        recipe = RailRecipe(gauge_m=1.067)
        product, report = build_track(xy @ rotation.T + [2000, -3000], 0.0, recipe)
        self.assertTrue(report["valid"])
        assert_gauge(self, xy @ rotation.T + [2000, -3000], product.rails, 1.067)

    def test_sleepers_share_one_closed_template_and_exact_affine_instances(self):
        product, report = build_track([[0, 0], [30, 2]], 2.0)
        transforms = product.sleeper_transforms
        self.assertIs(product.sleeper, SLEEPER)
        self.assertFalse(transforms.flags.writeable)
        mesh = bake_sleepers(transforms)
        reference = []
        for transform in transforms:
            reference.extend(
                [
                    transform[:, :3] @ vertex + transform[:, 3]
                    for vertex in SLEEPER.vertices
                ]
            )
        np.testing.assert_allclose(mesh.vertices, reference, atol=1e-14)
        volume = len(transforms) * 2.6 * 0.24 * 0.18
        self.assertAlmostEqual(closed_volume(mesh), volume, places=10)
        self.assertEqual(report["shared_sleeper_bytes"], 336)
        self.assertLess(
            transforms.nbytes + 336, mesh.vertices.nbytes + mesh.triangles.nbytes
        )

    def test_sleepers_have_spacing_and_clear_ends(self):
        product, _ = build_track([[0, 0], [20, 0]], 0.0)
        positions = product.sleeper_transforms[:, :, 3]
        np.testing.assert_allclose(np.diff(positions[:, 0]), 0.6, atol=1e-12)
        self.assertGreaterEqual(positions[0, 0], 0.12)
        self.assertLessEqual(positions[-1, 0], 19.88)

    def test_caps_and_budget_failures_do_not_return_partial_invalid_hardware(self):
        for limits in (
            RailBudget(maximum_stations=1),
            RailBudget(maximum_sleepers=1),
            RailBudget(maximum_rail_vertices=1),
        ):
            product, report = build_track([[0, 0], [10, 0]], 0.0, budget=limits)
            self.assertIsNone(product)
            self.assertTrue(report["reason"])

    def test_closed_short_sharp_and_folded_axes_keep_explicit_reasons(self):
        cases = (
            ([[0, 0], [1, 0], [0, 0]], "rail_periodic_join_not_planned"),
            ([[0, 0], [0.1, 0]], "rail_span_too_short"),
            ([[0, 0], [1, 0], [1, 1]], "rail_horizontal_corner"),
            ([[0, 0], [0, 0]], "rail_zero_length_segment"),
            ([[0, 0], [0.01, 0], [1, 0.2]], "rail_offset_fold"),
        )
        for xy, reason in cases:
            product, report = build_track(xy, 0.0)
            self.assertIsNone(product)
            self.assertEqual(report["reason"], reason)

    def test_recipe_and_nonfinite_input_are_rejected(self):
        for args in (
            {"gauge_m": float("nan")},
            {"gauge_m": 2.4},
            {"sleeper_length_m": 1.0},
            {"sleeper_spacing_m": 0.1},
        ):
            with self.assertRaises(ValueError):
                RailRecipe(**args)
        with self.assertRaises(ValueError):
            build_track([[0, 0], [float("inf"), 1]], 0.0)
        with self.assertRaises(ValueError):
            build_track([[0, 0], [1, 1]], float("nan"))


if __name__ == "__main__":
    unittest.main()
