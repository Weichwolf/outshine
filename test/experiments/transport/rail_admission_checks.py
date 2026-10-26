"""Real clipped-axis conflict and separation of volumetric overlap from support."""

import json
import unittest
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import shapely
from outshine_ref.transport.rail_hardware import build_rail_hardware
from rail_piece_admission import admit_pieces


class RailAdmission(unittest.TestCase):
    def test_real_tokyo_fragments_keep_one_hardware_body_and_both_sources(self):
        fixture = json.loads(
            (Path(__file__).parent / "fixtures/rail-overlap-tokyo.json").read_text()
        )
        products, rows = [], []
        for case in fixture["cases"]:
            product, report = build_rail_hardware(case["xy"], 0.0)
            self.assertTrue(report["valid"])
            products.append(product)
            rows.append(
                {
                    "piece": case["original_piece"],
                    "sources": case["sources"],
                    "valid": True,
                }
            )
        before = products[0].rails.vertices.copy()
        selected, audit = admit_pieces(products, rows)
        self.assertEqual(len(selected), 1)
        self.assertEqual(audit["rejected_conflicting_pieces"], 1)
        self.assertTrue(rows[1]["retained_existing_bed"])
        self.assertEqual(rows[1]["sources"], fixture["cases"][1]["sources"])
        np.testing.assert_array_equal(selected[0].rails.vertices, before)

    def test_rail_feet_on_a_separately_owned_sleeper_are_contacts(self):
        rails = SimpleNamespace(
            footprints=(shapely.box(0, -0.85, 10, -0.7), shapely.box(0, 0.7, 10, 0.85))
        )
        sleeper = SimpleNamespace(
            footprints=(
                shapely.Polygon(),
                shapely.Polygon(),
                shapely.box(4.9, -1.3, 5.1, 1.3),
            )
        )
        rows = [{"piece": 0, "valid": True}, {"piece": 1, "valid": True}]
        selected, audit = admit_pieces([rails, sleeper], rows)
        self.assertEqual(len(selected), 2)
        self.assertEqual(audit["conflict_pairs"], 0)

    def test_overlapping_sleeper_volumes_are_not_support_contacts(self):
        sleeper = SimpleNamespace(
            footprints=(
                shapely.Polygon(),
                shapely.Polygon(),
                shapely.box(4.9, -1.3, 5.1, 1.3),
            )
        )
        rows = [{"piece": 0, "valid": True}, {"piece": 1, "valid": True}]
        selected, audit = admit_pieces([sleeper, sleeper], rows)
        self.assertEqual(len(selected), 1)
        self.assertEqual(audit["conflict_pairs"], 1)
        self.assertEqual(rows[1]["reason"], "rail_hardware_piece_conflict")


if __name__ == "__main__":
    unittest.main()
