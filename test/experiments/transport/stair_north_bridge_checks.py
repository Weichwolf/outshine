"""Whole north branch, designated shared-cycle profiles and bounded pier coverage."""

import copy
import unittest

import numpy as np
from shared_bridge_geometry import corridor
from stair_branch_inputs import branch_rows, surface_welding
from stair_cycle_contacts import bindings, contacts
from stair_cycle_inputs import FIXTURE, load
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_landing_contacts import node_profiles
from stair_landing_pads import chosen_kinds
from stair_network_geometry import interactions, vertex_links
from stair_support_plan import station_choices, stations
from stair_west_bridge_checks import BranchPolicy

NORTH = FIXTURE.with_name("stair-north-bridges-vienna.json")


class SharedCyclePolicy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        BranchPolicy.setUpClass()

    def test_shared_roles_require_both_designations_and_keep_actual_source_tags(self):
        source = next(
            s for k, s in BranchPolicy.sources.items() if k.endswith(":1574593390:2")
        )
        key = (
            source["source_id"]
            if "source_id" in source
            else next(k for k, s in BranchPolicy.sources.items() if s is source)
        )
        original = copy.deepcopy(source)
        for props in (
            {},
            {"bicycle": "no"},
            {"foot": "no"},
            {"indoor": 1},
            {"layer": 1},
        ):
            changed = copy.deepcopy(source)
            changed["properties"].update(props)
            settings = {
                "exterior_source_extensions": [
                    {"source_id": key, "recipe": "exterior_shared_cycle_path"}
                ]
            }
            if props:
                with (
                    self.subTest(props=props),
                    self.assertRaisesRegex(ValueError, "exterior recipe"),
                ):
                    branch_rows(
                        settings,
                        {key: changed},
                        [],
                        BranchPolicy.profiles,
                        BranchPolicy.sample,
                    )
            else:
                result = branch_rows(
                    settings,
                    {key: changed},
                    [],
                    BranchPolicy.profiles,
                    BranchPolicy.sample,
                )[0]
                self.assertEqual(result["width_m"], 2)
                self.assertEqual(result["gradient"], 0.06)
                self.assertEqual(
                    result["original"]["source"]["feature"], source["original_feature"]
                )
        self.assertEqual(source, original)

    def test_unmarked_path_is_not_a_chosen_cycle_landing(self):
        _, rows, nodes, stairs, _, _ = load(NORTH)
        i = 29
        n = next(j for j, n in enumerate(nodes) if i in n["incident"])
        self.assertEqual(chosen_kinds(rows, [(i, 0)], n, stairs), "")
        rows[i].pop("source_recipe")
        self.assertEqual(
            chosen_kinds(rows, [(i, 0)], n, stairs), "unsupported_source_kind_or_level"
        )


class WholeNorthernBranch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, cls.rows, cls.nodes, cls.stairs, _, _ = load(NORTH)
        cls.plan = layout(cls.rows, "width", junctions=cls.nodes, stairs=cls.stairs)
        heights, cls.note = solve(cls.plan)
        if heights is None:
            raise ValueError(str(cls.note))
        _, cls.data, cls.geometry = build(cls.plan, heights)

    def test_whole_actual_sources_profiles_clearance_and_vertex_links_pass(self):
        self.assertEqual(len(self.rows), 32)
        pairs, _ = interactions(self.rows, self.nodes)
        proof = audit(
            self.data,
            [corridor(r) for r in self.rows],
            limits=[r["gradient"] for r in self.rows],
            crossings=pairs,
        )
        profiles = node_profiles(self.data, self.rows, self.nodes)
        self.assertTrue(profiles["all_chosen_internal_profiles_whole"])
        self.assertGreater(profiles["complete_internal_profiles"], 49)
        self.assertEqual(
            len(contacts(self.data, self.rows, bindings(NORTH, self.rows))), 16
        )
        vertex_links(self.data["body_vertices"], self.data["body_faces"])
        self.assertTrue(proof)
        for i in (29, 31):
            props = self.rows[i]["original"]["source"]["feature"]["properties"]
            self.assertEqual(props["subclass"], "path")
            self.assertEqual(self.rows[i]["gradient"], 0.06)
        self.assertLessEqual(
            self.plan["note"]["surface_weld"]["candidate_pairs"], 65536
        )
        self.assertLess(self.plan["note"]["weld_m"], 1e-7)

    def test_prior_ten_nm_recipe_rejects_the_real_pinched_body(self):
        rows = copy.deepcopy(self.rows)
        surface_welding(rows, None)
        plan = layout(rows, "width", junctions=self.nodes, stairs=self.stairs)
        heights, note = solve(plan)
        self.assertIsNotNone(heights, str(note))
        with self.assertRaisesRegex(ValueError, "closed oriented"):
            build(plan, heights)

    def test_station_cap_distributes_whole_coverage_and_preserves_every_deferred_identity(
        self,
    ):
        with self.assertRaisesRegex(ValueError, "station budget"):
            stations(self.rows)
        selected, short, deferred = station_choices(self.rows, selection="uniform")
        again = station_choices(self.rows, selection="uniform")
        self.assertEqual(len(selected), 128)
        self.assertEqual(len(deferred), 14)
        self.assertEqual(short, again[1])
        self.assertEqual(deferred, again[2])
        self.assertEqual(
            [(i, s) for i, _l, s in selected], [(i, s) for i, _l, s in again[0]]
        )
        for owner in {i for i, _l, _s in selected}:
            line = next(l for i, l, _s in selected if i == owner)
            values = np.arange(6, line.length - 2, 12)
            kept = [s for i, _l, s in selected if i == owner]
            self.assertEqual((kept[0], kept[-1]), (values[0], values[-1]))
        self.assertTrue(all(r["reason"] == "support_station_budget" for r in deferred))


if __name__ == "__main__":
    unittest.main()
