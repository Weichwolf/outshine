"""Actual whole western bridges, explicit deck order and unchanged ground defaults."""

import copy
import json
import unittest

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_branch_inputs import branch_rows, surface_sampling
from stair_context_inputs import PROFILES
from stair_corner_checks import toy
from stair_crossing_order import apply_orders, crossing_order
from stair_cycle_contacts import bindings, contacts
from stair_cycle_inputs import FIXTURE, load, original_inputs, selected_sources
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import cross_sections, layout
from stair_junction_solve import solve
from stair_landing_contacts import node_profiles
from stair_network_geometry import interactions, vertex_links
from stair_outer_geometry import outer_ends

WEST = FIXTURE.with_name("stair-west-bridges-vienna.json")
BASE = FIXTURE.with_name("stair-landings-vienna.json")


class BranchPolicy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, cls.previous, _, _, _, sample = load(BASE)
        cls.sample = staticmethod(sample)
        _, _, _, cls.sources = original_inputs()
        cls.profiles = {
            r["kind"]: r
            for t in json.loads(PROFILES.read_text())["templates"]
            for r in t.get("osm", [])
            if r.get("layer") == "streets"
        }
        cls.declared = json.loads(WEST.read_text())["exterior_source_extensions"]

    def rows_for(self, declarations):
        return branch_rows(
            {"exterior_source_extensions": declarations},
            self.sources,
            [r["source_id"] for r in self.previous],
            self.profiles,
            self.sample,
        )

    def test_explicit_bridges_keep_the_original_tags_axes_and_source_ids(self):
        rows = self.rows_for(self.declared)
        self.assertEqual(len(rows), 4)
        for i, row in enumerate(rows):
            original = self.sources[row["source_id"]]
            np.testing.assert_array_equal(row["xy"], original["xy_m"])
            self.assertEqual(
                row["original"]["source"]["feature"], original["original_feature"]
            )
            self.assertEqual(row["original"]["bridge"], i < 3)

    def test_bridge_as_ground_unknown_duplicate_and_over_budget_inputs_fail(self):
        wrong = dict(self.declared[0], recipe="exterior_cycle_path")
        old = dict(self.declared[0], source_id=self.previous[0]["source_id"])
        missing = dict(self.declared[0], source_id="unknown-original-source")
        for values in ([wrong], [old], [missing], self.declared[:1] * 9):
            with self.subTest(values=values), self.assertRaises(ValueError):
                self.rows_for(values)
        with self.assertRaisesRegex(ValueError, "ordinary exterior ground"):
            selected_sources(
                {"additional_source_ids": [self.declared[0]["source_id"]]}, self.sources
            )
        with self.assertRaisesRegex(ValueError, "one to six"):
            selected_sources(
                {"additional_source_ids": [r["source_id"] for r in self.previous[:7]]},
                self.sources,
            )

    def test_source_knots_preserve_ends_and_do_not_coarsen_the_stairs(self):
        rows, _, _ = toy()
        rows[0]["xy"] = np.array([[0, 0], [100, 0]], float)
        dense = cross_sections(rows[0])
        surface_sampling(rows, "source_knots")
        sparse = cross_sections(rows[0])
        self.assertLess(len(sparse), len(dense))
        np.testing.assert_array_equal(
            [r.interpolate(0.5, normalized=True).coords[0] for r in sparse],
            [[0, 0], [100, 0]],
        )
        surface_sampling(rows, "end_neighbourhood")
        self.assertEqual(
            [r.wkb for r in cross_sections(rows[0])], [r.wkb for r in dense]
        )
        _, current, _, _, _, _ = load(WEST)
        for i in (0, 7, 13):
            self.assertEqual(
                [r.wkb for r in cross_sections(current[i])],
                [r.wkb for r in cross_sections(self.previous[i])],
            )
            bad = dict(current[i], surface_cross_section_mode="source_knots")
            with self.assertRaisesRegex(ValueError, "cannot be coarsened"):
                cross_sections(bad)


class ExplicitOrder(unittest.TestCase):
    def test_two_bridges_need_order_and_legacy_ground_bridge_order_is_unchanged(self):
        rows, _, _ = toy()
        rows[1]["original"]["bridge"] = True
        self.assertEqual(crossing_order(rows, 0, 1), (0, 1))
        rows[0]["original"]["bridge"] = True
        self.assertIsNone(crossing_order(rows, 0, 1))
        for row in rows:
            row["original"]["source"]["feature"]["properties"]["brunnel"] = "bridge"
        original = copy.deepcopy(rows)
        notes = apply_orders(rows, [{"lower": "0", "upper": "1"}])
        self.assertEqual(crossing_order(rows, 0, 1), (0, 1))
        self.assertEqual(crossing_order(rows, 1, 0), (0, 1))
        self.assertEqual(len(notes), 1)
        for a, b in zip(rows, original, strict=True):
            self.assertEqual(a["original"], b["original"])
        rows[0]["separation_below_source_ids"] = ["1"]
        with self.assertRaisesRegex(ValueError, "contradictory"):
            crossing_order(rows, 0, 1)

    def test_duplicate_opposite_unknown_and_false_bridge_declarations_are_atomic(self):
        rows, _, _ = toy()
        for row in rows:
            row["original"]["bridge"] = True
            row["original"]["source"]["feature"]["properties"]["brunnel"] = "bridge"
        one = {"lower": "0", "upper": "1"}
        for values in (
            [one, one],
            [one, {"lower": "1", "upper": "0"}],
            [one, {"lower": "missing", "upper": "1"}],
        ):
            changed = copy.deepcopy(rows)
            with self.assertRaises(ValueError):
                apply_orders(changed, values)
            self.assertFalse(any(r.get("separation_below_source_ids") for r in changed))
        changed = copy.deepcopy(rows)
        changed[0]["original"]["source"]["feature"]["properties"].pop("brunnel")
        with self.assertRaisesRegex(ValueError, "actual bridge"):
            apply_orders(changed, [one])


class ActualWesternBridges(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, cls.rows, cls.nodes, cls.stairs, cls.context, _ = load(WEST)
        cls.plan = layout(cls.rows, "width", junctions=cls.nodes, stairs=cls.stairs)
        heights, cls.solve_note = solve(cls.plan)
        if heights is None:
            raise ValueError(
                "whole western bridge profile failed: " + str(cls.solve_note)
            )
        _, cls.data, _ = build(cls.plan, heights)
        cls.pairs, _ = interactions(cls.rows, cls.nodes)

    def test_all_previous_source_axes_priors_and_footprints_remain(self):
        _, previous, _, _, _, _ = load(BASE)
        self.assertEqual(len(self.rows), 28)
        for before, after in zip(previous, self.rows[:24], strict=True):
            self.assertEqual(before["source_id"], after["source_id"])
            self.assertEqual(before["original"], after["original"])
            np.testing.assert_array_equal(before["xy"], after["xy"])
            np.testing.assert_array_equal(before["prior"], after["prior"])
            self.assertLess(corridor(before).difference(corridor(after)).area, 1e-8)

    def test_all_forty_nine_internal_profiles_and_sixteen_pair_profiles_are_whole(self):
        result = node_profiles(self.data, self.rows, self.nodes)
        self.assertTrue(result["all_chosen_internal_profiles_whole"])
        self.assertEqual(result["complete_internal_profiles"], 49)
        self.assertEqual(
            len(contacts(self.data, self.rows, bindings(WEST, self.rows))), 16
        )
        self.assertEqual(
            outer_ends(self.rows, self.nodes),
            [(16, 0), (18, -1), (20, 0), (21, 0), (21, -1), (22, 0), (27, 0)],
        )

    def test_real_whole_bridge_over_bridge_and_previous_underpasses_pass(self):
        result = audit(
            self.data,
            [corridor(r) for r in self.rows],
            limits=[r["gradient"] for r in self.rows],
            crossings=self.pairs,
        )
        self.assertEqual(len(result["clearance"]), 11)
        self.assertEqual(len([r for r in self.pairs if r[:2] == [26, 25]]), 7)
        self.assertGreaterEqual(
            result["clearance"]["26:25"]["float32"]["minimum_clearance_m"], 2.5
        )
        self.assertEqual(sum(r["risers"] for r in result["treads"].values()), 24)
        vertex_links(self.data["body_vertices"], self.data["body_faces"])

    def test_missing_order_or_reversed_actual_clearance_cannot_be_relabelled(self):
        changed = copy.deepcopy(self.rows)
        for row in changed:
            row.pop("separation_below_source_ids", None)
        with self.assertRaisesRegex(ValueError, "equal bridge kinds"):
            interactions(changed, self.nodes)
        point = next(r[2] for r in self.pairs if r[:2] == [26, 25])
        with self.assertRaisesRegex(ValueError, "2.5-m clearance"):
            audit(
                self.data,
                [corridor(r) for r in self.rows],
                limits=[r["gradient"] for r in self.rows],
                crossings=[[25, 26, point]],
            )

    def test_dense_case_hits_unchanged_cap_and_source_knots_fit_it(self):
        rows = copy.deepcopy(self.rows)
        surface_sampling(rows, "end_neighbourhood")
        self.assertEqual(sum(len(cross_sections(r)) for r in rows), 813)
        with self.assertRaisesRegex(ValueError, "arrangement segment budget"):
            layout(rows, "width", junctions=self.nodes, stairs=self.stairs)
        self.assertEqual(sum(self.plan["note"]["source_section_counts"]), 250)
        self.assertLessEqual(self.plan["note"]["segments"], 8192)
        self.assertLessEqual(sum(map(len, self.plan["parts"])), 8192)

    def test_common_platform_hypothesis_does_not_pass_the_chosen_overpass_contract(
        self,
    ):
        nodes = copy.deepcopy(self.nodes)
        domains = [corridor(r) for r in self.rows]
        for part in shapely.get_parts(domains[25].intersection(domains[26])):
            if part.geom_type == "Polygon" and part.area > 1e-7:
                nodes.append(
                    {
                        "mvt_xy": None,
                        "xy": list(part.representative_point().coords[0]),
                        "incident": [25, 26],
                        "chosen_overlap_wkt": part.wkt,
                    }
                )
        model = layout(self.rows, "width", junctions=nodes, stairs=self.stairs)
        heights, note = solve(model)
        self.assertIsNotNone(heights, note)
        _, data, _ = build(model, heights)
        with self.assertRaisesRegex(ValueError, "lost or relabelled|2.5-m clearance"):
            audit(
                data,
                domains,
                limits=[r["gradient"] for r in self.rows],
                crossings=self.pairs,
            )


if __name__ == "__main__":
    unittest.main()
