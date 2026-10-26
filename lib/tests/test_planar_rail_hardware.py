"""Physical dimensions, chosen planes, bounded input and owned immutable products."""

import unittest
from dataclasses import FrozenInstanceError

import numpy as np
import shapely
from outshine_ref.transport.rail_hardware import (
    RailBudget,
    RailRecipe,
    bake_rail_sleepers,
    build_planar_rail_hardware,
    rail_bed_frame,
)


class PlanarHardware(unittest.TestCase):
    def setUp(self):
        self.triangle = np.array([[0.0, 0, 10], [100, 0, 20], [0, 100, 30]])

    def test_whole_metric_gauge_and_real_sleeper_bottom_contacts(self):
        product, note = build_planar_rail_hardware([[0, 0], [40, 0]], self.triangle)
        self.assertTrue(note["valid"])
        rings = product.rails.vertices.reshape(2, -1, 12, 3)
        tangent = np.array([1.0, 0, 0.1])
        tangent /= np.linalg.norm(tangent)
        normal = np.array([-0.1, -0.2, 1])
        normal /= np.linalg.norm(normal)
        lateral = np.cross(normal, tangent)
        gauge = (rings[1, :, 7] - rings[0, :, 6]) @ lateral
        np.testing.assert_allclose(gauge, 1.435, atol=1e-12)
        np.testing.assert_allclose(
            np.linalg.norm(product.sleeper_transforms[:, :, :3], axis=1),
            np.tile([2.6, 0.24, 0.18], (len(product.sleeper_transforms), 1)),
            atol=1e-12,
        )
        np.testing.assert_allclose(
            (product.sleeper_bottoms - [0, 0, 10]) @ normal, 0, atol=1e-12
        )
        self.assertGreater(np.linalg.norm([0, 1.435, 0.2 * 1.435]) - 1.435, 0.02)

    def test_projected_footprints_cover_all_actual_3d_box_corners(self):
        product, _ = build_planar_rail_hardware([[0, 0], [12, 0]], self.triangle)
        boxes = bake_rail_sleepers(product.sleeper_transforms).vertices.reshape(
            -1, 8, 3
        )
        for corners, shape in zip(boxes, product.footprints[2:], strict=True):
            expected = shapely.MultiPoint(corners[:, :2]).convex_hull
            self.assertLess(shape.symmetric_difference(expected).area, 1e-12)

    def test_owned_readonly_input_independent_result(self):
        xy = np.array([[0.0, 0], [12, 0]])
        triangle = self.triangle.copy()
        product, _ = build_planar_rail_hardware(xy, triangle)
        old = product.rails.vertices.copy()
        xy[:] = 1000
        triangle[:] = 1000
        np.testing.assert_array_equal(product.rails.vertices, old)
        for array in [
            product.plane_origin,
            product.plane_frame,
            product.sleeper_bottoms,
            product.sleeper_transforms,
            product.rails.vertices,
            product.rails.triangles,
        ]:
            self.assertFalse(array.flags.writeable)
        with self.assertRaises(FrozenInstanceError):
            product.plane_frame = np.eye(3)

    def test_reversed_triangle_winding_still_yields_upward_metric_frame(self):
        origin, frame = rail_bed_frame(self.triangle)
        other_origin, other = rail_bed_frame(self.triangle[[0, 2, 1]])
        np.testing.assert_array_equal(origin, other_origin)
        np.testing.assert_allclose(frame, other, atol=1e-15)
        np.testing.assert_allclose(frame.T @ frame, np.eye(3), atol=1e-15)
        self.assertGreater(np.linalg.det(frame), 0.999999999999)
        self.assertGreater(frame[2, 2], 0)

    def test_station_budget_precedes_array_conversion(self):
        class Unconvertible:
            def __len__(self):
                return 3

            def __array__(self, *args, **kwargs):
                raise AssertionError("over-budget input converted")

        value, note = build_planar_rail_hardware(
            Unconvertible(), self.triangle, budget=RailBudget(maximum_stations=2)
        )
        self.assertIsNone(value)
        self.assertEqual(note["reason"], "rail_station_budget")

    def test_sleeper_budget_and_unplanned_corners_remain_explicit(self):
        value, note = build_planar_rail_hardware(
            [[0, 0], [40, 0]], self.triangle, budget=RailBudget(maximum_sleepers=2)
        )
        self.assertIsNone(value)
        self.assertEqual(note["reason"], "rail_sleeper_budget")
        value, note = build_planar_rail_hardware(
            [[0, 0], [4, 0], [3, 0]], self.triangle
        )
        self.assertIsNone(value)
        self.assertEqual(note["reason"], "rail_horizontal_corner")

    def test_invalid_or_unrepresentable_inputs_raise_without_runtime_warning(self):
        for triangle in [
            np.zeros((3, 3)),
            [[0, 0, 0], [0, 0, 1], [0, 1, 0]],
            np.full((3, 3), np.nan),
            self.triangle * 1e200,
        ]:
            with self.subTest(triangle=triangle), self.assertRaises(ValueError):
                rail_bed_frame(triangle)
        for xy in [[], [[0, 0]], [[0, 0, 1], [1, 0, 1]], [[0, 0], [np.inf, 0]]]:
            with self.subTest(xy=xy), self.assertRaises(ValueError):
                build_planar_rail_hardware(xy, self.triangle, RailRecipe())


if __name__ == "__main__":
    unittest.main()
