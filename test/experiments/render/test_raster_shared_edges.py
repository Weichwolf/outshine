"""A planar square cannot lose its diagonal when either face rotates its corner order."""

import unittest

import numpy as np
from current_visibility import raster


class RasterSharedEdges(unittest.TestCase):
    def rectangle(self, low, high, *, rotations=(0, 0), reverse=False):
        p = np.array(
            [[low, low, 0.05], [low, high, 0.05], [high, high, 0.05], [high, low, 0.05]]
        )
        faces = np.array(
            [np.roll([2, 0, 1], rotations[0]), np.roll([0, 2, 3], rotations[1])]
        )
        if reverse:
            faces = faces[::-1]
        return raster(p, np.full(4, 2.0), faces, 32, 32, with_faces=True)

    def test_whole_rectangle_has_no_shared_diagonal_holes_for_cyclic_corner_orders(
        self,
    ):
        for first in range(3):
            for second in range(3):
                for reverse in (False, True):
                    depth, owner = self.rectangle(
                        -1.1, 33.2, rotations=(first, second), reverse=reverse
                    )
                    self.assertTrue((owner >= 0).all())
                    self.assertTrue((depth > 0).all())
                    self.assertLessEqual(abs(depth - 0.05).max(), 4 * np.spacing(0.05))

    def test_coverage_is_not_expanded_to_external_pixel_centres(self):
        depth, _ = self.rectangle(0.6, 30.4)
        expected = np.zeros((32, 32), dtype=bool)
        expected[1:30, 1:30] = True
        np.testing.assert_array_equal(depth > 0, expected)

    def test_no_epsilon_makes_a_pixel_just_outside_the_rectangle_covered(self):
        depth, _ = self.rectangle(0.5 + 2**-30, 32.0)
        self.assertFalse(depth[0].any())
        self.assertFalse(depth[:, 0].any())
        self.assertTrue((depth[1:, 1:] > 0).all())


if __name__ == "__main__":
    unittest.main()
