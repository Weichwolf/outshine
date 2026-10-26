"""Keep the original native counterexample and the incomplete-shore distinction."""

import copy
import json
import unittest
from pathlib import Path

import numpy as np
from native_bank_slice import audit

FIXTURE = Path(__file__).with_name("fixtures") / "native-bank-contact" / "input.json"


class NativeBankSlice(unittest.TestCase):
    def setUp(self):
        self.data = json.loads(FIXTURE.read_text())

    def test_original_four_node_membership_and_actual_native_bed_plane(self):
        report, _, _ = audit(self.data)
        nodes = report["native_membership_replay"]
        self.assertEqual([len(n["hits"]) for n in nodes], [0, 0, 0, 1])
        hit = nodes[3]["hits"][0]
        self.assertEqual(hit["source_face_id"], nodes[3]["native_decided_by"])
        self.assertEqual(hit["source_face_id"], 5317)
        self.assertAlmostEqual(
            hit["constant_two_m_bed_up_m"],
            self.data["water"]["points"][3]["bedUpM"],
            places=12,
        )
        self.assertEqual(report["shore_role"].split(":")[0], "unknown")

    def test_ordered_originals_and_native_bindings_are_not_modified(self):
        before = copy.deepcopy(self.data)
        _, triangles, xy = audit(self.data)
        self.assertEqual(self.data, before)
        np.testing.assert_array_equal(
            triangles,
            [r["vertices"] for r in self.data["water"]["sourceSurface344Faces"]],
        )
        np.testing.assert_array_equal(
            xy, [[r["eastM"], r["northM"]] for r in self.data["water"]["points"]]
        )

    def test_facet_permutation_preserves_source_witness_instead_of_array_slot(self):
        self.data["water"]["sourceSurface344Faces"].reverse()
        report, _, _ = audit(self.data)
        self.assertEqual(
            report["native_membership_replay"][3]["hits"][0]["source_face_id"], 5317
        )

    def test_bad_or_oversized_slice_is_not_given_an_empty_valid_body(self):
        for fault in ("empty", "oversized", "nonfinite", "reversed"):
            data = copy.deepcopy(self.data)
            faces = data["water"]["sourceSurface344Faces"]
            if fault == "empty":
                faces.clear()
            elif fault == "oversized":
                faces *= 100
            elif fault == "nonfinite":
                faces[0]["vertices"][0][0] = np.nan
            else:
                faces[0]["vertices"].reverse()
            with self.assertRaises(ValueError):
                audit(data)


if __name__ == "__main__":
    unittest.main()
