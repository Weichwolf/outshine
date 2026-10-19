"""Independent analytic plane/void controls for layered novel-view traversal."""

import unittest

import numpy as np
from source_far_assets import capture_view
from source_far_reprojection import intersect_field, intersect_source
from test_source_far_assets import source_planes


class ReprojectionTests(unittest.TestCase):
    def setUp(self):
        self.source = source_planes()
        # Oct16 encoding of +Z, within the established quantisation error.
        self.source["normal"][:] = np.uint32(0x80008000)
        self.data, _ = capture_view(
            self.source, [0, 0, -1], [0, 1, 0], resolution=8, layers=3
        )

    def test_front_depth_and_parallel_outside_bounds(self):
        origin = np.array([[0.33, 0.44, 2], [1.1, 0.44, 2], [0.6, 0.6, 0.5]])
        direction = np.tile([0, 0, -1.0], (3, 1))
        t, slots, work = intersect_field(self.data, origin, direction)
        np.testing.assert_allclose(t[[0, 2]], [2, 0.5], atol=1e-4)
        self.assertTrue(np.isinf(t[1]))
        self.assertEqual(slots[1], -1)
        self.assertGreater(work["valid_plane_tests"], 0)

    def test_missing_front_cell_exposes_second_layer(self):
        # A hole is explicit missing data. Do not fill it by interpolating neighbours.
        self.data["depth"][:64] = np.nan
        origin = np.array([[0.33, 0.44, 2]])
        t, slots, _ = intersect_field(self.data, origin, np.array([[0.2, 0, -1.0]]))
        np.testing.assert_allclose(t, [3], atol=1e-4)
        self.assertGreaterEqual(slots[0], 64)

    def test_oblique_and_corner_crossing_against_exact_planes(self):
        origin = np.array([[0.12, 0.13, 2], [0.11, 0.11, 2], [-0.3, 0.4, 2]])
        direction = np.array([[0.25, 0.1, -1], [0.2, 0.2, -1], [0.3, 0, -1]])
        t, _, work = intersect_field(self.data, origin, direction)
        exact, _ = intersect_source(self.source, origin, direction)
        np.testing.assert_allclose(t, exact, atol=1e-4)
        self.assertLessEqual(work["visited_ray_cells"], len(origin) * 18)


if __name__ == "__main__":
    unittest.main()
