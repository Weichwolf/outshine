import unittest

import numpy as np

from guarded_source_faces import facing_in_ball, wall_planes


class GuardedSourceFaces(unittest.TestCase):
    def test_winding_and_courtyard(self):
        ring = np.array([[0, 0], [2, 0], [2, 2], [0, 2]])
        for points in [ring, ring[::-1], np.vstack([ring, ring[0]])]:
            for hole in [False, True]:
                starts, normals = wall_planes(points, hole)
                centre = facing_in_ball(starts, normals, np.array([1, 1]), 0)
                self.assertTrue(np.all(centre == hole))
                self.assertEqual(
                    facing_in_ball(starts, normals, np.array([-2, 1]), 0).sum(),
                    3 if hole else 1,
                )

    def test_tangent_ball_and_normal_scale(self):
        starts = np.array([[3.0, 0.0]])
        for scale in [0.1, 1, 100]:
            normals = np.array([[scale, 0.0]])
            self.assertTrue(facing_in_ball(starts, normals, np.zeros(2), 3)[0])
            self.assertFalse(facing_in_ball(starts, normals, np.zeros(2), 2.99)[0])

    def test_support_is_attained_and_outside_requires_new_selection(self):
        starts = np.array([[4.0, 0.0], [0.0, 5.0]])
        normals = np.array([[3.0, 0.0], [0.0, 2.0]])
        radius = 3
        selected = facing_in_ball(starts, normals, np.zeros(2), radius)
        self.assertFalse(selected.any())
        for normal in normals:
            extreme = radius * normal / np.linalg.norm(normal)
            self.assertFalse(facing_in_ball(starts, normals, extreme, 0).any())
        self.assertTrue(facing_in_ball(starts, normals, np.array([4.1, 0]), 0)[0])

    def test_degenerate_edges_are_retained(self):
        self.assertTrue(
            facing_in_ball(np.zeros((1, 2)), np.zeros((1, 2)), np.zeros(2), 0)[0]
        )
        for radius in [-1, float("nan"), float("inf")]:
            with self.assertRaises(ValueError):
                facing_in_ball(np.zeros((1, 2)), np.zeros((1, 2)), np.zeros(2), radius)


if __name__ == "__main__":
    unittest.main()
