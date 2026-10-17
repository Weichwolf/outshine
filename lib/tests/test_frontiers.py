"""Discrete explicit-node topology, deterministic alternatives and budget controls."""

import unittest

from outshine_ref.transport.frontiers import discover_source_modules


def record(key, nodes, *, eligible=True, length=10):
    return {
        "source_id": key,
        "nodes": nodes,
        "length_m": length,
        "eligible": eligible,
        "reason": "" if eligible else "unsupported_level",
    }


class FrontierGraph(unittest.TestCase):
    def test_index_matches_scan_on_branches_cycles_and_input_permutations(self):
        records = [
            record("a", [(0, 0), (1, 0)]),
            record("b", [(1, 0), (2, 0)], length=20),
            record("c", [(2, 0), (2, 1)]),
            record("d", [(2, 1), (1, 0)]),
            record("e", [(1, 0), (1, -1)], eligible=False),
        ]
        roots = [{"root_id": "root", "node": (0, 0)}]
        expected = discover_source_modules(records, roots, method="scan")["proposals"]
        for rows in (records, records[::-1]):
            actual = discover_source_modules(rows, roots)["proposals"]
            for before, after in zip(expected, actual, strict=True):
                for key in ("source_ids", "connections", "length_m", "stop_reason"):
                    self.assertEqual(before[key], after[key])
        self.assertNotIn("e", expected[0]["source_ids"])

    def test_geometric_crossing_without_original_node_is_not_a_connection(self):
        rows = [
            record("a", [(-1, 0), (1, 0)]),
            record("b", [(0, -1), (0, 1)]),
            record("c", [(1, 1), (2, 1)]),
        ]
        plan = discover_source_modules(rows, [{"root_id": "root", "node": (-1, 0)}])
        self.assertEqual(plan["proposals"][0]["source_ids"], ["a"])

    def test_endpoint_to_interior_is_connected_but_root_needs_new_endpoint(self):
        rows = [record("a", [(0, 0), (1, 0)]), record("b", [(1, -1), (1, 0), (1, 1)])]
        plan = discover_source_modules(rows, [{"root_id": "root", "node": (0, 0)}])
        self.assertEqual(plan["proposals"][0]["source_ids"], ["a", "b"])
        isolated = discover_source_modules(
            rows[1:], [{"root_id": "root", "node": (1, 0)}]
        )
        self.assertFalse(isolated["proposals"][0]["source_ids"])

    def test_source_and_work_budgets_return_connected_prefixes(self):
        rows = [record(str(i), [(i, 0), (i + 1, 0)]) for i in range(20)]
        roots = [{"root_id": "root", "node": (0, 0)}]
        bounded = discover_source_modules(rows, roots)["proposals"][0]
        self.assertEqual(len(bounded["source_ids"]), 8)
        self.assertEqual(bounded["stop_reason"], "module_source_budget")
        short = discover_source_modules(rows, roots, max_visits=1)["proposals"][0]
        self.assertLessEqual(short["work"]["incidence_tests"], 1)
        self.assertEqual(short["source_ids"], ["0"])
        self.assertEqual(short["stop_reason"], "frontier_work_budget")

    def test_empty_ocean_tile_and_invalid_duplicate_sources(self):
        self.assertFalse(discover_source_modules([], [])["proposals"])
        row = record("a", [(0, 0), (1, 0)])
        with self.assertRaises(ValueError):
            discover_source_modules([row, row], [])
        with self.assertRaises(ValueError):
            discover_source_modules([dict(row, nodes=[(0.1, 0), (1, 0)])], [])


if __name__ == "__main__":
    unittest.main()
