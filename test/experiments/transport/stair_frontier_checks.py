"""Discrete frontier controls, actual cached roots, T pads and whole headroom."""

import copy
import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_frontier_graph import discover
from stair_landing_pads import complete_landings
from stair_module_audit import traffic
from stair_module_clearance import source_clearances
from stair_module_inputs import FIXTURE, inputs
from stair_module_node_heights import plan_heights
from stair_module_roots import root_profile
from stair_southwest_checks import cached_reference


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
        expected = discover(records, roots, method="scan")["proposals"]
        for rows in (records, records[::-1]):
            actual = discover(rows, roots)["proposals"]
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
        plan = discover(rows, [{"root_id": "root", "node": (-1, 0)}])
        self.assertEqual(plan["proposals"][0]["source_ids"], ["a"])

    def test_endpoint_to_interior_is_connected_but_root_needs_new_endpoint(self):
        rows = [record("a", [(0, 0), (1, 0)]), record("b", [(1, -1), (1, 0), (1, 1)])]
        plan = discover(rows, [{"root_id": "root", "node": (0, 0)}])
        self.assertEqual(plan["proposals"][0]["source_ids"], ["a", "b"])
        isolated = discover(rows[1:], [{"root_id": "root", "node": (1, 0)}])
        self.assertFalse(isolated["proposals"][0]["source_ids"])

    def test_source_and_work_budgets_return_connected_prefixes(self):
        rows = [record(str(i), [(i, 0), (i + 1, 0)]) for i in range(20)]
        roots = [{"root_id": "root", "node": (0, 0)}]
        bounded = discover(rows, roots)["proposals"][0]
        self.assertEqual(len(bounded["source_ids"]), 8)
        self.assertEqual(bounded["stop_reason"], "module_source_budget")
        short = discover(rows, roots, max_visits=1)["proposals"][0]
        self.assertLessEqual(short["work"]["incidence_tests"], 1)
        self.assertEqual(short["source_ids"], ["0"])
        self.assertEqual(short["stop_reason"], "frontier_work_budget")

    def test_empty_ocean_tile_and_invalid_duplicate_sources(self):
        self.assertFalse(discover([], [])["proposals"])
        row = record("a", [(0, 0), (1, 0)])
        with self.assertRaises(ValueError):
            discover([row, row], [])
        with self.assertRaises(ValueError):
            discover([dict(row, nodes=[(0.1, 0), (1, 0)])], [])


class ActualInteriorRoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _r, _rows, _context, cls.sources, _sampler, _layer = inputs(FIXTURE)
        cls.state = cached_reference()
        cls.key = next(iter(cls.state["cached_module_rows"]))

    def test_actual_original_interior_cap_is_sampled_without_outer_port(self):
        recipe = {
            "port": {"source_id": self.key, "vertex": 6},
            "contact_mode": "profile",
        }
        row, line = root_profile(self.state, recipe, self.sources)
        self.assertTrue(row["root_interior"])
        np.testing.assert_array_equal(line[1, :2], self.sources[self.key]["xy_m"][6])
        self.assertTrue(np.isfinite(line).all())

    def test_interior_root_requires_original_axis_and_explicit_family(self):
        recipe = {"port": {"source_id": self.key, "vertex": 6}, "contact_mode": "flat"}
        with self.assertRaises(ValueError):
            root_profile(self.state, recipe, self.sources)
        bad = copy.deepcopy(self.state)
        bad["cached_module_rows"][self.key]["xy"][6][0] += 0.001
        with self.assertRaises(ValueError):
            root_profile(bad, dict(recipe, contact_mode="profile"), self.sources)


class SourceJunctionsAndClearance(unittest.TestCase):
    def test_t_junction_uses_actual_internal_profile_and_old_default_stays(self):
        rows = [
            {
                "source_id": str(i),
                "xy": np.array(xy, float),
                "prior": np.zeros(len(xy)),
                "width_m": 2.0,
                "original": {
                    "bridge": False,
                    "source": {
                        "feature": {
                            "properties": {"class": "path", "subclass": "footway"}
                        }
                    },
                },
            }
            for i, xy in enumerate(([[-20, 0], [0, 0], [20, 0]], [[0, 0], [0, 20]]))
        ]
        nodes = [{"incident": [0, 1], "mvt_xy": [0, 0], "xy": [0, 0]}]
        self.assertFalse(complete_landings(copy.deepcopy(rows), nodes, [])[0])
        pads, _ledger = complete_landings(rows, nodes, [], include_interiors=True)
        self.assertEqual(len(pads), 1)
        self.assertEqual(rows[0]["junction_pad_vertices"], [1])
        self.assertEqual(len(plan_heights(rows, nodes, [], include_interiors=True)), 1)

    def test_actual_headroom_is_checked_for_each_declared_profile(self):
        low = np.array([[0, 0, 0], [4, 0, 0], [4, 4, 0], [0, 4, 0]], float)
        faces = np.array([[0, 1, 2], [0, 2, 3]])
        data = {
            "surface_vertices": low,
            "surface_faces": faces,
            "owners": np.ones((2, 1), bool),
        }
        upper = Mesh(low + [0, 0, 3], faces)
        row = {
            "source_id": "foot",
            "original": {
                "source": {"feature": {"properties": {"subclass": "footway"}}}
            },
        }
        footprint = shapely.box(0, 0, 4, 4)
        proof = source_clearances(data, [row], [footprint], [], upper)
        self.assertTrue(all(p["sources"][0]["minimum_clearance_m"] == 3 for p in proof))
        with self.assertRaisesRegex(ValueError, "headroom below declared profile"):
            source_clearances(
                data,
                [dict(row, traffic_profile={"clearance_m": 4.5})],
                [footprint],
                [],
                upper,
            )
        with self.assertRaisesRegex(ValueError, "intersects an existing bridge"):
            traffic(data, upper, [])


if __name__ == "__main__":
    unittest.main()
