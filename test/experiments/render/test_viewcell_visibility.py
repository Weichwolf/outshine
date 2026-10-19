"""Independent geometry counterexamples for continuous camera-cell support."""

import unittest

import numpy as np
from outshine_ref.geometry.occlusion import convex_patch_umbra
from viewcell_visibility import select_support


class ViewcellVisibilityTest(unittest.TestCase):
    def setUp(self):
        self.blocker = np.array([[-1, -1, 1], [-1, 1, 1], [1, -1, 1]], float)
        self.eye = np.array([[-0.1, -0.1, 0], [0.1, 0.1, 0]])

    def support(self, target, eye=None, **kwargs):
        positions = np.concatenate((self.blocker, target))
        return select_support(
            positions,
            np.arange(6).reshape(2, 3),
            self.eye if eye is None else eye,
            max_blockers=1,
            **kwargs,
        )

    def test_hidden_for_whole_box(self):
        target = np.array([[-0.6, -0.6, 2], [-0.6, -0.5, 2], [-0.5, -0.6, 2]])
        result = self.support(target)
        np.testing.assert_array_equal(result.keep, [True, False])
        self.assertEqual(result.witness[1], 0)
        # Independently intersect eye-to-target segments with z=1, including
        # interior eyes and target barycentric points, not just the witnesses.
        for eye in ([0, 0, 0], [0.07, -0.09, 0], [-0.1, 0.1, 0]):
            for weights in ([1, 0, 0], [0, 1, 0], [0.2, 0.3, 0.5]):
                point = np.array(weights) @ target
                hit = np.array(eye) + (point - eye) / 2
                self.assertGreater(hit[0], -1)
                self.assertGreater(hit[1], -1)
                self.assertLess(hit[0] + hit[1], 0)

    def test_disocclusion_from_box_corner_is_retained(self):
        target = np.array([[0.58, -1.02, 2], [0.58, -0.98, 2], [0.62, -1.02, 2]])
        eye = np.array([[-0.5, 0, 0], [0.5, 0, 0]])
        self.assertTrue(self.support(target, eye).keep[1])
        self.assertFalse(self.support(target, np.zeros((2, 3))).keep[1])

    def test_nearer_or_coplanar_targets_are_retained(self):
        for z in (0.5, 1):
            target = np.array([[-0.3, -0.3, z], [-0.3, -0.2, z], [-0.2, -0.3, z]])
            self.assertTrue(self.support(target).keep[1])

    def test_partial_shadow_keeps_entire_target(self):
        target = np.array([[-0.6, -0.6, 2], [-0.6, 3, 2], [3, -0.6, 2]])
        self.assertTrue(self.support(target).keep[1])

    def test_crossing_occluder_plane_has_no_umbra(self):
        certificate = convex_patch_umbra(
            self.blocker, [[-0.1, -0.1, 0], [0.1, 0.1, 2]], certified_opaque=True
        )
        self.assertEqual(len(certificate.low), 0)

    def test_near_clipped_blocker_cannot_hide_target(self):
        self.blocker[:, 2] = 0.05
        target = np.array([[-0.6, -0.6, 2], [-0.6, -0.5, 2], [-0.5, -0.6, 2]])
        self.assertTrue(self.support(target).keep[1])

    def test_backface_budget_and_invalid_inputs(self):
        positions = np.concatenate((self.blocker, self.blocker[::-1]))
        result = select_support(
            positions, np.arange(6).reshape(2, 3), self.eye, max_blockers=0
        )
        np.testing.assert_array_equal(result.keep, [True, False])
        self.assertEqual(result.witness[1], -2)
        for box in ([[0, 0, 0], [-1, 0, 0]], [[0, 0, 0], [np.nan, 1, 1]]):
            with self.assertRaises(ValueError):
                select_support(positions, np.arange(6).reshape(2, 3), box)


if __name__ == "__main__":
    unittest.main()
