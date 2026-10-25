"""Real population selection and conservative original-rest accounting."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
from build_building_hierarchy_growth import original_rest, population_cells


def populations():
    records = [{"cell": c} for c in (33, 2, 63, 2)]
    summaries = [
        {"supported": True, "bounds": np.array([[x, 0, 0], [x + 1, 1, 1.0]])}
        for x in (0, 2, 100)
    ] + [{"supported": False}]
    return {"records": records, "summaries": summaries}


class BuildingHierarchyGrowth(unittest.TestCase):
    def test_populations_use_evidenced_distance_instead_of_logical_cell_order(self):
        plans = populations()
        self.assertEqual(population_cells(plans, 33, 2), {33, 2})
        self.assertEqual(population_cells(plans, 33, 3), {33, 2, 63})
        self.assertEqual(population_cells(plans, 33, 1), {33})
        for seed, count in ((1, 1), (33, 0), (33, 4)):
            with self.assertRaises(ValueError):
                population_cells(plans, seed, count)

    def test_no_cell_without_a_centre_is_silently_omitted(self):
        plans = populations()
        plans["summaries"][2] = {"supported": False}
        with self.assertRaisesRegex(ValueError, "no unknown cell omitted"):
            population_cells(plans, 33, 2)

    def test_missing_parent_and_unknown_source_are_paid_not_zero_priced(self):
        plans = {"summaries": [{"supported": False} for _ in range(10)]}
        plans["summaries"][2] = {"supported": True, "faces": 12}
        plans["summaries"][3] = {"supported": True, "faces": 7}
        with tempfile.TemporaryDirectory(prefix="outshine-population-") as temporary:
            root = Path(temporary)
            np.savez(
                root / "tree.npz",
                unbounded_sources=[9],
                node_sources=[[0, 2], [2, 1]],
                source_order=[1, 2, 3],
            )
            np.savez(root / "node-0.npz", g__needs_sources=[2])
            query = {"name": "view", "nodes": [0, 1], "original_needs": 3}
            report = original_rest(plans, root, [query])[0]
            self.assertEqual(report["original_sources"], [2, 3, 9])
            self.assertEqual(report["known_source_summary_faces"], 19)
            self.assertEqual(report["unpriced_original_sources"], [9])
            with self.assertRaisesRegex(ValueError, "complete original remainder"):
                original_rest(plans, root, [query | {"original_needs": 2}])


if __name__ == "__main__":
    unittest.main()
