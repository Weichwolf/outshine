"""Real corner patches preserve source positions and expose partial-window scope."""

import unittest

import numpy as np
from pool_body_experiment import FIXTURE, construct, original
from pool_native_neighborhood import load_neighborhood
from pool_region_contact import reconcile_window


class PoolRegionContact(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        neighborhood = load_neighborhood()
        data, source, _, water, _ = original(FIXTURE)
        cls.body = construct(data, source, water)
        cls.source = neighborhood.vertices.reshape(97, 97, 3)[16:25, 88:97].reshape(
            -1, 3
        )
        cls.saved = cls.source.copy()
        cls.selective = reconcile_window(cls.body, cls.source, 9)
        cls.unconditional = reconcile_window(
            cls.body, cls.source, 9, reuse_unchanged=False
        )

    def test_selective_rebuild_is_identical_to_rebuilding_all_contact_patches(self):
        for a, b in zip(self.selective[:3], self.unconditional[:3]):
            self.assertEqual(a.tobytes(), b.tobytes())
        a, b = self.selective[3], self.unconditional[3]
        self.assertLess(a["work"]["patch_calls"], b["work"]["patch_calls"])
        self.assertLess(a["work"]["edge_node_tests"], b["work"]["edge_node_tests"])

    def test_real_subset_does_not_claim_complete_body_or_outer_neighbor_qualification(
        self,
    ):
        _, _, indices, report = self.selective
        self.assertEqual(report["admission"], "partial_body_window")
        self.assertFalse(report["topology"]["complete_body_contact_covered"])
        self.assertEqual(report["topology"]["euler"], 1)
        self.assertLess(report["maximum_reconciled_seam_gap_m"], 1e-7)
        self.assertGreater(report["contact"]["compared_area_m2"], 0)
        self.assertLess(
            report["contact"]["compared_area_m2"], report["contact"]["cap_area_m2"]
        )
        self.assertEqual(len(indices), len(self.source))
        np.testing.assert_array_equal(self.source, self.saved)

    def test_cancellation_invalid_positions_and_unbounded_grid_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "cancelled"):
            reconcile_window(self.body, self.source, 9, cancelled=lambda: True)
        bad = self.source.copy()
        bad[0, 0] = np.nan
        with self.assertRaisesRegex(ValueError, "finite"):
            reconcile_window(self.body, bad, 9)
        with self.assertRaisesRegex(ValueError, "bounded square"):
            reconcile_window(self.body, self.source, 257)


if __name__ == "__main__":
    unittest.main()
