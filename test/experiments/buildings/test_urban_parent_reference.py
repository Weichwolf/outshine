"""Centred-lot DDA matches existing independent boxes, including empty XY gaps."""

import unittest

import numpy as np
from compare_urban_parents import camera
from test_urban_patches import example
from triangle_image_reference import image_rays
from urban_parent_reference import prepare_resolved, trace_resolved
from urban_patch_trace import trace


class ParentReferenceTest(unittest.TestCase):
    def test_jittered_reference_samples_stay_in_their_pixel_strata(self):
        view = camera(16000, 240000, 0)
        box = np.array([[640, 360], [642, 362]])
        _, directions = image_rays(view, box, 2, 2, 4, jitter_seed=1113)
        _, repeated = image_rays(view, box, 2, 2, 4, jitter_seed=1113)
        np.testing.assert_array_equal(directions, repeated)
        local = directions @ view[1].T
        pixels = [640, 360] + view[2] * local[:, :2] / local[:, 2, None] * [1, -1]
        x, y = np.meshgrid(np.arange(8), np.arange(8))
        np.testing.assert_array_equal(
            np.floor((pixels - [640, 360]) * 4), np.column_stack((x.ravel(), y.ravel()))
        )

    def test_motion_and_altitude_preserve_the_declared_target_distance(self):
        target = np.array([8000, 8000, 12])
        for motion, altitude in [(0, None), (8000, None), (0, 10000)]:
            eye = camera(16000, 240000, motion, altitude)[0]
            self.assertAlmostEqual(np.linalg.norm(eye - target), 240000, places=8)
            if altitude is not None:
                self.assertEqual(eye[2], altitude)

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

    def test_explicit_shadow_containment_hits_without_relaxing_camera_guard(self):
        product = prepare_resolved(example())
        o = np.array([[8.0, 8, 3], [0.5, 8, 3], [8, 8, 6.001], [8, 8, -1]])
        d = np.tile([0.0, 0, 1], (len(o), 1))
        with self.assertRaisesRegex(ValueError, "inside occupied"):
            trace_resolved(product, o[:1], d[:1], minimum=0)
        depth, _, _, work = trace_resolved(product, o, d, minimum=0, inside_is_hit=True)
        self.assertEqual(depth[0], 0)
        np.testing.assert_array_equal(np.isinf(depth[1:3]), True)
        self.assertEqual(depth[3], 1)
        self.assertEqual(work["near_limit_hits"], 1)


if __name__ == "__main__":
    unittest.main()
