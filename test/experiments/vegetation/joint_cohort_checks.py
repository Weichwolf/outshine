"""Independent new-rowspace, shadow/normal and pooled-measure controls."""

import unittest

import numpy as np
from joint_basis_cohort import coarsen_case, level_fields, project_modes
from joint_basis_study import view_held_queries, view_light_knots
from positive_light_basis import angular_weights


class JointCohortTests(unittest.TestCase):
    def test_new_spatial_rows_preserve_known_light_modes_without_heldout_fit(self):
        knots = np.r_[np.eye(3), -np.eye(3)]
        queries = np.r_[knots, [[1.0, 1, 1], [-1.0, 1, -1]] / np.sqrt(3)]
        modes = np.maximum(knots[:, [0, 2]], 0).T
        spatial = np.array(
            [[0.3, 0.1], [0, 0], [0.1, 0.8], [0.6, 0], [0, 0.2], [0.3, 0.5]]
        )
        target = spatial @ modes
        before = target.copy()
        truth = spatial @ np.maximum(queries[:, [0, 2]], 0).T
        mass = spatial.sum(axis=1, keepdims=True)
        cones = [angular_weights(knots, queries)] * 3
        rows, fields = project_modes(modes, target, truth, mass, cones)
        np.testing.assert_array_equal(target, before)
        for requested in (4, 8, 12):
            np.testing.assert_allclose(fields[requested], truth, atol=1e-14, rtol=0)
            np.testing.assert_array_equal(fields[requested][1], 0)
            self.assertEqual(rows[str(requested)]["actual_rank"], 2)
            self.assertEqual(rows[str(requested)]["spatial_float32_bytes"], 6 * 2 * 4)

    def test_normal_baseline_and_joint_shadow_stay_distinct_after_pooling(self):
        knots = view_light_knots(64)
        held = np.array([view_held_queries()] * 3)
        queries = np.r_[knots, held[0]]
        moments = np.zeros((3, 256, 4, 4))
        moments[:, :, :2, 0] = 0.5
        moments[:, :, 0, 1] = 0.5
        moments[:, :, 1, 3] = 0.5
        clear = np.maximum(queries[:, 2], 0) * 0.5
        blocked = np.maximum(queries[:, 0], 0) * 0.5
        case = {
            "joint": np.tile(clear[None, :, None], (3, 1, 256)),
            "unshadowed": np.tile((clear + blocked)[None, :, None], (3, 1, 256)),
            "coverage": np.ones((3, 256)),
            "normal_moments": moments,
            "directions": np.tile(queries, (3, 1, 1)),
            "knots": knots,
            "heldout": held,
        }
        for size in (16, 8, 4, 2, 1):
            _, truth, mass, _, baseline, _ = level_fields(case)
            np.testing.assert_allclose(
                truth, np.tile(clear[64:], (3 * size**2, 1)), atol=1e-14
            )
            np.testing.assert_array_equal(mass, 1)
            self.assertGreater(
                baseline["normal4_no_self_shadow"]["six_independent_lights"]["max_abs"],
                0.1,
            )
            self.assertAlmostEqual(
                baseline["normal4_no_self_shadow"]["all_eight_queries"]["max_abs"],
                baseline["exact_unshadowed"]["all_eight_queries"]["max_abs"],
                places=14,
            )
            if size > 1:
                case = coarsen_case(case, size)


if __name__ == "__main__":
    unittest.main()
