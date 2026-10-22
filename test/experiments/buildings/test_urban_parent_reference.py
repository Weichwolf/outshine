"""Centred-lot DDA matches existing independent boxes, including empty XY gaps."""

import unittest

import numpy as np
from test_urban_patches import example
from urban_parent_reference import prepare_resolved, trace_resolved
from urban_patch_trace import trace


class ParentReferenceTest(unittest.TestCase):
    def test_inset_lots_match_independent_box_geometry_in_random_views(self):
        product = example()
        rng = np.random.default_rng(1113)
        origins = rng.uniform(-64, 96, (4096, 3))
        origins[:, 2] = rng.choice([-24, 64], len(origins))
        directions = rng.uniform(-8, 56, origins.shape) - origins
        directions[:50, 0] = 0
        directions[50:100, 1] = 0
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        expected = trace(product, origins, directions, mode="boxes")
        actual = trace_resolved(prepare_resolved(product), origins, directions)
        np.testing.assert_array_equal(np.isfinite(actual[0]), np.isfinite(expected[0]))
        hit = np.isfinite(actual[0])
        np.testing.assert_allclose(actual[0][hit], expected[0][hit], rtol=0, atol=1e-11)
        np.testing.assert_array_equal(actual[1][hit], expected[3][hit])

    def test_gap_at_the_side_of_an_occupied_cell_stays_open(self):
        product = prepare_resolved(example())
        origins = [[0.5, -10, 3], [8, -10, 3], [8, 8, 50]]
        directions = [[0, 1, 0], [0, 1, 0], [0, 0, -1]]
        depth, _, _, work = trace_resolved(product, origins, directions)
        self.assertFalse(np.isfinite(depth[0]))
        self.assertAlmostEqual(depth[1], 12)
        self.assertAlmostEqual(depth[2], 44)
        self.assertGreater(work["cell_ray_visits"], 0)

    def test_shadow_ray_keeps_a_blocker_inside_the_camera_near_distance(self):
        product = prepare_resolved(example())
        depth, _, _, _ = trace_resolved(product, [[8, 1.99, 3]], [[0, 1, 0]], minimum=0)
        self.assertAlmostEqual(depth[0], 0.01)


if __name__ == "__main__":
    unittest.main()
