"""Current native geometry/frame linkage, without constructing replacement heights."""

import unittest

import numpy as np
from shore_contact_observation import FIXTURE, observe, plane_at


class NativeShoreObservation(unittest.TestCase):
    def test_affine_plane_and_unsupported_curvature_are_explicit(self):
        np.testing.assert_array_equal(
            plane_at([0, 0, 10, 2, -3, 0], [[0, 0], [2, 1]]), [10, 11]
        )
        with self.assertRaises(ValueError):
            plane_at([0, 0, 10, 2, -3, 0.1], [[0, 0]])

    def test_native_edge_and_whole_selected_basin_geometry_bind(self):
        result, _ = observe(FIXTURE, "539")
        self.assertTrue(result["page_witness_words_match"])
        self.assertGreater(result["water_basin_faces_checked"], 1)
        np.testing.assert_allclose(
            result["water_minus_basin_vertex_m_range"], [2, 2], atol=1e-8, rtol=0
        )
        self.assertEqual(result["basin_ring_covers"], [True, False])
        self.assertEqual(result["corridor_ring_covers"], [False, False])
        self.assertIsNone(result["corridor_profile"])
        self.assertAlmostEqual(
            result["basin_wants_plane_m"][0],
            result["observed_final_local_m"][0],
            places=8,
        )
        self.assertGreater(
            abs(
                result["corridor_wants_plane_m"][1]
                - result["observed_final_local_m"][1]
            ),
            0.01,
        )
        self.assertGreater(result["corridor_key_stamps"], 1)
        with self.assertRaises(ValueError):
            observe(FIXTURE, "537")


if __name__ == "__main__":
    unittest.main()
