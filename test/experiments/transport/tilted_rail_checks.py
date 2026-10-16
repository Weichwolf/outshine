"""Independent metric gauge/section/contact controls under rigid bed rotation."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware import bake_rail_sleepers
from rail_hardware_checks import closed_volume
from tilted_rail_geometry import build_tilted, plane_frame, transform_points
from tilted_rail_verify import GroundContact


class TiltedHardware(unittest.TestCase):
    def setUp(self):
        self.triangle = np.array([[0.0, 0, 10], [100, 0, 20], [0, 100, 30]])

    def test_metric_gauge_and_sleeper_dimensions_are_preserved(self):
        product, note = build_tilted([[0, 0], [40, 0]], self.triangle)
        self.assertTrue(note["valid"])
        rings = product.rails.vertices.reshape(2, -1, 12, 3)
        tangent = np.array([1.0, 0, 0.1])
        tangent /= np.linalg.norm(tangent)
        normal = np.array([-0.1, -0.2, 1.0])
        normal /= np.linalg.norm(normal)
        lateral = np.cross(normal, tangent)
        gauge = np.einsum("ij,j->i", rings[1, :, 7] - rings[0, :, 6], lateral)
        np.testing.assert_allclose(gauge, 1.435, atol=1e-12)
        matrix = product.sleeper_transforms[:, :, :3]
        np.testing.assert_allclose(
            np.linalg.norm(matrix, axis=1),
            np.tile([2.6, 0.24, 0.18], (len(matrix), 1)),
            atol=1e-12,
        )
        np.testing.assert_allclose(np.linalg.det(matrix), 2.6 * 0.24 * 0.18, atol=1e-12)
        boxes = bake_rail_sleepers(product.sleeper_transforms)
        self.assertAlmostEqual(closed_volume(boxes), len(matrix) * 2.6 * 0.24 * 0.18)
        self.assertAlmostEqual(
            closed_volume(product.rails), closed_volume(product.horizontal.rails)
        )

    def test_entire_sleeper_bottom_lies_on_the_explicit_plane(self):
        product, _ = build_tilted([[0, 0], [40, 0]], self.triangle)
        points = np.concatenate(product.sleeper_bottoms)
        expected = 10 + 0.1 * points[:, 0] + 0.2 * points[:, 1]
        np.testing.assert_allclose(points[:, 2], expected, atol=1e-12)
        # All underside faces are affine; exact vertices imply the whole cap.
        for cap in product.sleeper_bottoms:
            centre = cap.mean(axis=0)
            self.assertAlmostEqual(centre[2], 10 + 0.1 * centre[0] + 0.2 * centre[1])

    def test_scalar_and_batch_rigid_geometry_are_equivalent(self):
        a, _ = build_tilted([[0, 0], [20, 0], [40, 4]], self.triangle)
        scalar = transform_points(
            a.horizontal.rails.vertices, a.plane_origin, a.plane_frame, method="scalar"
        )
        np.testing.assert_allclose(a.rails.vertices, scalar, atol=1e-12, rtol=0)
        np.testing.assert_array_equal(a.rails.triangles, a.horizontal.rails.triangles)
        translations = transform_points(
            a.horizontal.sleeper_transforms[:, :, 3],
            a.plane_origin,
            a.plane_frame,
            method="scalar",
        )
        np.testing.assert_allclose(
            a.sleeper_transforms[:, :, 3], translations, atol=1e-12, rtol=0
        )

    def test_sheared_horizontal_gauge_is_a_real_dimension_error(self):
        # A vertical lift changes the physical transverse distance on a cross slope.
        sheared_gap = np.array([0, 1.435, 0.2 * 1.435])
        self.assertGreater(np.linalg.norm(sheared_gap) - 1.435, 0.02)

    def test_invalid_plane_and_unplanned_hardware_remain_explicit(self):
        with self.assertRaises(ValueError):
            plane_frame(np.array([[0, 0, 0], [0, 0, 1], [0, 1, 1]]))
        product, note = build_tilted([[0, 0], [0.1, 0]], self.triangle)
        self.assertIsNone(product)
        self.assertFalse(note["valid"])


class ActualContacts(unittest.TestCase):
    def test_inner_ground_peak_is_found_by_whole_actual_face_check(self):
        vertices = np.array(
            [[-2, -2, 0], [2, -2, 0], [2, 2, 0], [-2, 2, 0], [0, 0, 0.1]]
        )
        mesh = Mesh(vertices, np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]]))
        actual = GroundContact(mesh, np.ones(4, bool))
        with self.assertRaisesRegex(ValueError, "whole actual bed faces"):
            actual.verify(shapely.box(-1, -1, 1, 1), vertices[:3] * [1, 1, 0])

    def test_terrain_face_is_not_implicitly_a_chosen_rail_bed(self):
        vertices = np.array([[-2, -2, 0], [2, -2, 0], [2, 2, 0], [-2, 2, 0]])
        mesh = Mesh(vertices, np.array([[0, 1, 2], [0, 2, 3]]))
        actual = GroundContact(mesh, np.array([True, False]))
        with self.assertRaisesRegex(ValueError, "actual traffic faces"):
            actual.verify(shapely.box(-1, -1, 1, 1), vertices[:3])


if __name__ == "__main__":
    unittest.main()
