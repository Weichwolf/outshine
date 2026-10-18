"""Whole walking dependencies, refusal budgets and canonical closed source nodes."""

import copy
import unittest

import numpy as np
from stair_frontier_dispositions import source_dispositions
from stair_module_node_heights import plan_heights
from stair_seed_dependencies import expand, prepare, recipes_with_dependencies


def example():
    sources = {}
    for key, xy, kind in (
        ("a", [[-10, 0], [10, 0]], "road"),
        ("b", [[0, -10], [0, 10]], "foot"),
        ("c", [[-10, 5], [10, 5]], "road"),
        ("d", [[100, 100], [101, 101]], "foot"),
    ):
        sources[key] = {
            "xy_m": xy,
            "mvt_xy": copy.deepcopy(xy),
            "properties": {"class": "minor"}
            if kind == "road"
            else {"class": "path", "subclass": "footway"},
        }
    records = [
        {"source_id": key, "nodes": s["mvt_xy"], "eligible": True}
        for key, s in sources.items()
    ]
    return sources, records


class SeedDependencies(unittest.TestCase):
    def test_transitive_whole_sources_and_contacts_match_index_and_scan(self):
        sources, records = example()
        before = copy.deepcopy(sources)
        proofs = [
            expand(["a"], prepare(records, sources, m)) for m in ("index", "scan")
        ]
        for proof in proofs:
            self.assertEqual(proof["source_ids"], ["a", "b", "c"])
            self.assertEqual(proof["reason"], "")
            self.assertEqual(len(proof["geometric_junctions"]), 2)
            self.assertTrue(
                all(c["modes"] == ["walk"] for c in proof["geometric_junctions"])
            )
        for key in ("source_ids", "geometric_junctions", "rejected_pairs", "reason"):
            self.assertEqual(proofs[0][key], proofs[1][key])
        self.assertLess(
            proofs[0]["work"]["candidate_visits"], proofs[1]["work"]["candidate_visits"]
        )
        self.assertEqual(sources, before)

    def test_source_contact_and_work_limits_refuse_the_whole_recipe(self):
        sources, records = example()
        prepared = prepare(records, sources, "index")
        for budget, reason in (
            ({"max_sources": 2}, "source"),
            ({"max_contacts": 1}, "contact"),
            ({"max_visits": 1}, "work"),
        ):
            self.assertEqual(
                expand(["a"], prepared, **budget)["reason"],
                "seed_dependency_" + reason + "_budget",
            )
        records = [dict(r, eligible=r["source_id"] != "c") for r in records]
        self.assertEqual(
            expand(["a"], prepare(records, sources, "index"))["source_ids"], ["a", "b"]
        )

    def test_private_conditional_and_non_ground_contacts_are_not_invented(self):
        for properties in (
            {"access": "private"},
            {"foot:conditional": "yes @ (Mo-Fr)"},
            {"layer": 1},
            {"indoor": True},
            {"brunnel": "tunnel"},
        ):
            sources, records = example()
            sources["b"]["properties"].update(properties)
            proof = expand(["a"], prepare(records, sources, "index"))
            self.assertEqual(proof["source_ids"], ["a"])
            self.assertEqual(proof["geometric_junctions"], [])
            self.assertEqual(len(proof["rejected_pairs"]), 1)

    def test_original_shared_nodes_and_multiple_crossings_are_not_geometric_shortcuts(
        self,
    ):
        for road, foot in (
            ([[-10, 0], [0, 0], [10, 0]], [[0, -10], [0, 0], [0, 10]]),
            ([[-10, 0], [10, 0]], [[-5, -10], [-5, 10], [5, 10], [5, -10]]),
        ):
            sources, records = example()
            for key, xy in (("a", road), ("b", foot)):
                sources[key].update(xy_m=xy, mvt_xy=copy.deepcopy(xy))
            proof = expand(["a"], prepare(records[:2], sources, "index"))
            self.assertEqual(proof["source_ids"], ["a"])
            self.assertEqual(len(proof["rejected_pairs"]), 1)

    def test_recipe_and_physical_dispositions_include_dependencies(self):
        sources, records = example()
        recipe = {"source_ids": ["a"], "seed": {"source_id": "a", "end": 0}}
        recipes, ledger = recipes_with_dependencies(
            [recipe, recipe], records, sources, "index"
        )
        self.assertEqual(len(recipes), 1)
        self.assertEqual(len(ledger), 2)
        self.assertEqual(
            recipes[0]["source_profiles"], {"a": "unclassified", "c": "unclassified"}
        )
        attempts = [{"index": 0, "source_ids": recipes[0]["source_ids"]}]
        dispositions = source_dispositions(
            {"proposals": [{"source_ids": ["a"]}]}, attempts, 0, recipes
        )
        self.assertEqual(set(dispositions), {"a", "b", "c"})
        self.assertTrue(all(r["candidate_geometry"] for r in dispositions.values()))

    def test_canonical_closed_node_preserves_two_profiles_and_consistent_priors(self):
        xy = np.array([[0, 0], [20, 0], [20, 20], [0, 20], [0, 0]], float)
        rows = [
            {"xy": xy, "prior": np.full(5, 100.0), "width_m": 5.5},
            {
                "xy": np.array([[-20, 0], [0, 0]], float),
                "prior": np.full(2, 100.0),
                "width_m": 5.5,
            },
        ]
        node = {"xy": [0, 0], "mvt_xy": [0, 0], "incident": [0, 1]}
        self.assertEqual(
            plan_heights(rows, [node], [], include_interiors=True)[0]["height_m"], 100
        )
        rows[0]["prior"][-1] += 0.01
        with self.assertRaisesRegex(ValueError, "one original source sample"):
            plan_heights(rows, [node], [], include_interiors=True)
        rows[0]["prior"][-1] = 100
        rows[0]["xy"] = xy[[0, 1, 0, 3, 0]]
        with self.assertRaisesRegex(ValueError, "one original source sample"):
            plan_heights(rows, [node], [], include_interiors=True)


if __name__ == "__main__":
    unittest.main()
