"""Grass detail survives demand motion, rotation and world-tile boundaries."""

import unittest
from dataclasses import replace

import numpy as np
from place_candidates import candidates
from place_scene_checks import camera


class CandidateChecks(unittest.TestCase):
    def test_near_grass_has_unique_separate_ids_and_is_rotation_invariant(self):
        a = candidates(camera(), (14, 8192, 8192))
        b = candidates(replace(camera(), bearing_deg=180), (14, 8192, 8192))
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])
        self.assertEqual(len(np.unique(a["ids"], axis=0)), len(a["ids"]))
        fine = ~a["tree"]
        self.assertGreater(fine.sum(), 1000)
        self.assertTrue((a["ids"][fine, 3:] >= 4096).all())
        self.assertTrue(a["grass"][fine].all())

    def test_moving_demand_preserves_shared_world_cells_and_shapes(self):
        a = candidates(camera(), (14, 8192, 8192))
        b = candidates(replace(camera(), longitude_deg=0.0001), (14, 8192, 8192))
        # Compare by identity, independently of the changed query row ordering.
        _, left, right = np.intersect1d(
            a["ids"][:, 3] * 100000 + a["ids"][:, 4],
            b["ids"][:, 3] * 100000 + b["ids"][:, 4],
            return_indices=True,
        )
        self.assertGreater(len(left), 192**2 + 1000)
        for key in ("latitude", "longitude", "angle", "scale", "tree", "grass"):
            np.testing.assert_array_equal(a[key][left], b[key][right])
        self.assertNotEqual(len(a["ids"]), len(b["ids"]))

    def test_remote_tile_requests_no_fine_detail(self):
        a = candidates(camera(), (14, 8200, 8200))
        self.assertEqual(len(a["ids"]), 192**2)
        self.assertTrue(a["tree"].all())


if __name__ == "__main__":
    unittest.main()
