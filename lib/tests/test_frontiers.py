"""Discrete explicit-node topology, deterministic alternatives and budget controls."""

import copy
import unittest

from outshine_ref.transport.frontiers import connected_prefixes, discover_source_modules


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


class ConnectedPrefixes(unittest.TestCase):
    def proposal(self, count=8):
        rows = [record(str(i), [(i, 0), (i + 1, 0)]) for i in range(count)]
        return discover_source_modules(rows, [{"root_id": "r", "node": (0, 0)}])[
            "proposals"
        ][0]

    def test_complete_connected_prefixes_and_owned_immutable_result(self):
        proposal = self.proposal()
        before = copy.deepcopy(proposal)
        choices = connected_prefixes(proposal)
        self.assertEqual([len(p) for p in choices], [8, 4, 2, 1])
        for choice in choices:
            seen = set()
            for key in choice:
                parent = proposal["connections"][key]["source_id"]
                self.assertTrue(parent is None or parent in seen)
                seen.add(key)
        self.assertEqual(proposal, before)
        proposal["source_ids"][0] = "changed"
        self.assertEqual(choices[-1], ("0",))

    def test_multiple_branches_share_one_root_and_cycle_witness_is_valid(self):
        rows = [
            record("a", [(0, 0), (1, 0)]),
            record("b", [(0, 0), (-1, 0)]),
            record("c", [(1, 0), (-1, 0)]),
        ]
        proposal = discover_source_modules(rows, [{"root_id": "r", "node": (0, 0)}])[
            "proposals"
        ][0]
        self.assertEqual([len(v) for v in connected_prefixes(proposal)], [3, 1])

    def test_invalid_parent_order_disconnected_roots_and_duplicate_are_rejected(self):
        for change in ("parent", "root", "duplicate", "node"):
            proposal = self.proposal(4)
            if change == "parent":
                proposal["connections"]["0"]["source_id"] = "3"
            elif change == "root":
                proposal["connections"]["1"] = {"source_id": None, "node": (99, 0)}
            elif change == "duplicate":
                proposal["source_ids"][1] = "0"
            else:
                proposal["connections"]["0"]["node"] = (0.5, 0)
            with self.subTest(change=change), self.assertRaises(ValueError):
                connected_prefixes(proposal)
        self.assertEqual(connected_prefixes(self.proposal(0)), ())


if __name__ == "__main__":
    unittest.main()
