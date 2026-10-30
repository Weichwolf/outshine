"""Complete projected triangle columns, including unseen and edge-on surfaces."""

import unittest

import numpy as np
from triangle_columns import ColumnLimit, nearest, prepare, query, world_rays


def rectangles(rear=True):
    p = np.array(
        [[-0.5, -0.5, 2], [0.5, -0.5, 2], [0.5, 0.5, 2], [-0.5, 0.5, 2]], np.float32
    )
    t = np.array([[0, 1, 2], [0, 2, 3]], np.int64)
    if rear:
        p = np.r_[p, p - [0, 0, 1]].astype(np.float32)
        t = np.r_[t, t + 4]
    return {"positions": p, "indices": t}


CAPTURE = np.diag([1.0, 1.0, 0.25, 1.0])
WORLD = CAPTURE.copy()
WORLD[0] = [1, 0, -1, 1]
POINTS = np.array([[0.9, 0.9], [1.1, 0.9], [0.9, 1.1], [1.1, 1.1]])


class ColumnTests(unittest.TestCase):
    def test_rear_surface_missing_from_first_hit_capture_is_preserved(self):
        for rear in (False, True):
            field = prepare(rectangles(rear), CAPTURE, 16)
            z, owner, evaluated, work = query(field, WORLD, POINTS, [2, 2])
            self.assertTrue(evaluated.all())
            np.testing.assert_allclose(z, 0.25 if rear else 0, atol=1e-15)
            self.assertTrue((owner >= 2).all() if rear else (owner == -1).all())
            self.assertEqual(work["failed_samples"], 0)

    def test_edge_on_capture_triangle_is_not_discarded(self):
        source = {
            "positions": np.array(
                [[0, -0.5, 0.5], [0, 0.5, 0.5], [0, 0, 1.5]], np.float32
            ),
            "indices": np.array([[0, 1, 2]]),
        }
        field = prepare(source, CAPTURE, 16)
        z, owner, evaluated, _ = query(field, WORLD, np.array([[1, 1]]), [2, 2])
        self.assertTrue(evaluated.all())
        self.assertEqual(owner[0], 0)
        self.assertAlmostEqual(z[0], 0.25)

    def test_grid_does_not_change_full_geometry_results_for_random_world_samples(self):
        rng = np.random.default_rng(47)
        source = {
            "positions": rng.uniform(
                [-0.8, -0.8, 0.2], [0.8, 0.8, 3.8], (90, 3)
            ).astype(np.float32),
            "indices": np.arange(90).reshape(-1, 3),
        }
        points = rng.uniform([0, 0], [2, 2], (128, 2))
        origins, directions = world_rays(WORLD, points, np.array([2, 2]))
        expected = [
            nearest(source["positions"], source["indices"], np.arange(30), o, d)
            for o, d in zip(origins, directions, strict=True)
        ]
        for size in (4, 16, 64):
            field = prepare(source, CAPTURE, size)
            z, owner, evaluated, _ = query(field, WORLD, points, [2, 2])
            self.assertTrue(evaluated.all())
            np.testing.assert_array_equal(owner, [v[1] for v in expected])
            valid = owner >= 0
            hit = (
                origins[valid]
                + np.array([v[0] for v in expected])[valid, None] * directions[valid]
            )
            np.testing.assert_allclose(z[valid], hit[:, 2] / 4, atol=1e-15)

    def test_reference_preflight_and_query_exhaustion_preserve_unknown(self):
        with self.assertRaises(ColumnLimit):
            prepare(rectangles(), CAPTURE, 16, references_most=1)
        field = prepare(rectangles(), CAPTURE, 16)
        for options in ({"pairs_most": 0}, {"time_limit_s": 0}, {"cells_most": 1}):
            _, _, evaluated, work = query(field, WORLD, POINTS, [2, 2], **options)
            self.assertFalse(evaluated.all())
            self.assertGreater(work["failed_samples"], 0)

    def test_immutable_geometry_snapshot_survives_source_change(self):
        source = rectangles()
        field = prepare(source, CAPTURE, 16)
        source["positions"][:] = 0
        self.assertGreater(field.positions.max(), 1)
        with self.assertRaises(ValueError):
            field.positions.setflags(write=True)

    def test_incomplete_capture_and_invalid_capacities_are_refused(self):
        for options in ({"size": 1}, {"references_most": 0}):
            with self.assertRaises(ValueError):
                prepare(rectangles(), CAPTURE, **options)
        changed = CAPTURE.copy()
        changed[0, 3] = 3
        with self.assertRaises(ValueError):
            prepare(rectangles(), changed)


if __name__ == "__main__":
    unittest.main()
