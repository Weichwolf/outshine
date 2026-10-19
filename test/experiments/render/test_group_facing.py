"""Grouped exclusions preserve exact facing and original triangle identities."""

import collections
import unittest

import numpy as np
from group_facing import bake_groups, select_groups
from test_static_facing import ENABLED, determinant, retained


class GroupFacing(unittest.TestCase):
    def setUp(self):
        base = np.array([[-1.0, -1, 0], [1, -1, 0], [0, 1, 0]])
        self.triangles = np.tile(np.stack((base, base[::-1])), (64, 1, 1))

    def test_opposing_interleaved_triangles_keep_their_original_positions(self):
        for partition in ("faces6", "octants8"):
            record = bake_groups(self.triangles, [0, 0, 0, 2], partition)
            members = [mask for mask, _ in record["groups"]]
            self.assertEqual(len(members), 2)
            self.assertEqual(members[0] | members[1], (1 << 128) - 1)
            self.assertEqual(members[0] & members[1], 0)
            selected = select_groups(record, [0, 0, 100], **ENABLED)
            np.testing.assert_array_equal(
                np.flatnonzero(retained(selected, 128)), np.arange(0, 128, 2)
            )

    def test_unknown_deforming_two_sided_mirrored_and_inside_keep_every_member(self):
        record = bake_groups(self.triangles, [0, 0, 0, 2])
        for camera in ([0, 0, 0], [0, 0, 2], [np.nan, 0, 0]):
            self.assertEqual(select_groups(record, camera, **ENABLED), (1 << 128) - 1)
        for enabled in (
            {},
            dict(ENABLED, static=False),
            dict(ENABLED, one_sided=False),
            dict(ENABLED, model=np.diag([-1, 1, 1, 1])),
            dict(ENABLED, model=np.diag([0, 1, 1, 1])),
        ):
            self.assertEqual(
                select_groups(record, [0, 0, 100], **enabled), (1 << 128) - 1
            )

    def test_membership_and_query_costs_include_masks_and_rank_work(self):
        record = bake_groups(self.triangles, [0, 0, 0, 2])
        work = collections.Counter()
        select_groups(record, [0, 0, 100], counters=work, **ENABLED)
        self.assertEqual(work["directory_bytes"], 8)
        self.assertEqual(work["cone_payload_read_bytes"], 16)
        self.assertEqual(work["membership_read_bytes"], 16)
        self.assertEqual(work["visibility_write_bytes"], 16)
        self.assertEqual(work["rank_popcount_upper_bound"], 512)
        self.assertEqual(work["compacted_index_write_bytes"], 64 * 12)

    def test_random_actual_dyadic_signs_with_seams_near_far_and_full_orbit(self):
        rng = np.random.default_rng(6258590034)
        triangles = rng.uniform(-1, 1, (29, 3, 3)).astype(np.float32).astype(float)
        for partition in ("faces6", "octants8"):
            record = bake_groups(triangles, [0, 0, 0, 2], partition)
            for angle in np.linspace(0, 2 * np.pi, 73):
                for distance in (0, 2.000001, 1e9):
                    camera = distance * np.array([np.cos(angle), np.sin(angle), 0.3])
                    visible = np.array([determinant(t, camera) >= 0 for t in triangles])
                    keep = retained(
                        select_groups(record, camera, **ENABLED), len(triangles)
                    )
                    self.assertFalse((visible & ~keep).any())

    def test_degenerate_and_unproved_sphere_are_local_safe_fallbacks(self):
        triangles = self.triangles.copy()
        triangles[0] = 0
        record = bake_groups(triangles, [0, 0, 0, 2])
        self.assertTrue(select_groups(record, [0, 0, 100], **ENABLED) & 1)
        record = bake_groups(triangles, [0, 0, 0, 0.01])
        self.assertEqual(select_groups(record, [0, 0, 100], **ENABLED), (1 << 128) - 1)


if __name__ == "__main__":
    unittest.main()
