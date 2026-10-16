"""Whole real corner profiles and rejection of unchosen or incompatible junctions."""

import copy
import unittest

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_corner_geometry import apply_pads
from stair_cycle_contacts import bindings, contacts, profile
from stair_cycle_inputs import FIXTURE, load
from stair_junction_audit import audit
from stair_junction_body import build
from stair_junction_layout import layout
from stair_junction_solve import solve
from stair_network_geometry import interactions, vertex_links
from stair_outer_geometry import outer_ends

CORNER = FIXTURE.with_name("stair-cycle-corner-vienna.json")
CORRIDORS = FIXTURE.with_name("stair-cycle-corridors-vienna.json")


def toy():
    rows = [
        {
            "source_id": str(i),
            "xy": np.array(xy, float),
            "width_m": width,
            "original": {
                "bridge": False,
                "source": {"feature": {"properties": {"subclass": "cycleway"}}},
            },
        }
        for i, (xy, width) in enumerate(
            [([[-10, 0], [0, 0]], 2), ([[0, 0], [0, 10]], 4)]
        )
    ]
    nodes = [{"xy": [0, 0], "mvt_xy": [1, 2], "incident": [0, 1]}]
    declaration = {"profiles": [["0", -1], ["1", 0]], "method": "profile_hull"}
    return rows, nodes, declaration


class CornerControls(unittest.TestCase):
    def test_unequal_right_angle_profiles_are_whole_and_axes_unchanged(self):
        rows, nodes, declaration = toy()
        original = copy.deepcopy(rows)
        before = shapely.union_all([corridor(r) for r in rows])
        note = apply_pads(rows, nodes, [declaration])[0]
        shared = corridor(rows[0]).intersection(corridor(rows[1]))
        for i, end in ((0, -1), (1, 0)):
            self.assertTrue(shared.covers(shapely.LineString(profile(rows[i], end))))
            self.assertTrue(corridor(rows[i]).covers(corridor(original[i])))
            np.testing.assert_array_equal(rows[i]["xy"], original[i]["xy"])
            self.assertEqual(rows[i]["original"], original[i]["original"])
        after = shapely.union_all([corridor(r) for r in rows])
        self.assertAlmostEqual(after.difference(before).area, 1.0)
        self.assertAlmostEqual(note["added_union_area_m2"], 1.0)

    def test_box_and_hull_meet_the_same_profiles_with_different_footprints(self):
        areas = []
        for method in ("profile_hull", "profile_box"):
            rows, nodes, declaration = toy()
            declaration["method"] = method
            note = apply_pads(rows, nodes, [declaration])[0]
            common = corridor(rows[0]).intersection(corridor(rows[1]))
            for i, end in ((0, -1), (1, 0)):
                self.assertTrue(
                    common.covers(shapely.LineString(profile(rows[i], end)))
                )
            areas.append(note["added_union_area_m2"])
        self.assertLess(areas[0], areas[1])

    def test_incompatible_source_semantics_and_unchosen_nodes_are_rejected(self):
        cases = [
            {"indoor": 1},
            {"level": -1},
            {"layer": 1},
            {"brunnel": "tunnel"},
            {"subclass": "steps"},
        ]
        for properties in cases:
            rows, nodes, declaration = toy()
            rows[1]["original"]["source"]["feature"]["properties"].update(properties)
            with self.subTest(properties=properties), self.assertRaises(ValueError):
                apply_pads(rows, nodes, [declaration])
            self.assertNotIn("junction_pads_xy_m", rows[0])
        for change in ("bridge", "nearby", "unchosen", "width"):
            rows, nodes, declaration = toy()
            if change == "bridge":
                rows[1]["original"]["bridge"] = True
            elif change == "nearby":
                rows[1]["xy"][0, 0] += 0.01
            elif change == "width":
                rows[1]["width_m"] = float("nan")
            else:
                nodes[0]["mvt_xy"] = None
            with self.subTest(change=change), self.assertRaises(ValueError):
                apply_pads(rows, nodes, [declaration])
            self.assertNotIn("junction_pads_xy_m", rows[0])

    def test_duplicate_and_degenerate_declarations_do_not_partially_apply(self):
        rows, nodes, declaration = toy()
        with self.assertRaisesRegex(ValueError, "duplicate"):
            apply_pads(rows, nodes, [declaration, declaration])
        self.assertFalse(any("junction_pads_xy_m" in r for r in rows))
        rows[1]["xy"] = np.array([[0, 0], [10, 0]], float)
        with self.assertRaisesRegex(ValueError, "nondegenerate"):
            apply_pads(rows, nodes, [declaration])
        self.assertFalse(any("junction_pads_xy_m" in r for r in rows))

    def test_injected_remote_nonconvex_and_nonfinite_pad_coordinates_fail(self):
        for points in (
            [[100, 100], [101, 100], [101, 101]],
            [[0, 0], [1, 0], [0.1, 0.1], [0, 1]],
            [[0, 0], [1, 0], [0, float("nan")]],
        ):
            rows, _, _ = toy()
            rows[0]["junction_pads_xy_m"] = [points]
            with self.subTest(points=points), self.assertRaises(ValueError):
                corridor(rows[0])


class RealCorner(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, cls.rows, cls.nodes, stairs, cls.context, _ = load(CORNER)
        p = layout(cls.rows, "width", junctions=cls.nodes, stairs=stairs)
        heights, note = solve(p)
        if heights is None:
            raise ValueError("actual corner profile failed: " + str(note))
        _, cls.data, _ = build(p, heights)

    def test_all_previous_axes_priors_originals_and_whole_source_bands_remain(self):
        _, previous, _, _, _, _ = load(CORRIDORS)
        self.assertEqual(len(self.rows), 24)
        for before, after in zip(previous, self.rows[:23], strict=True):
            self.assertEqual(before["source_id"], after["source_id"])
            self.assertEqual(before["original"], after["original"])
            np.testing.assert_array_equal(before["xy"], after["xy"])
            np.testing.assert_array_equal(before["prior"], after["prior"])
            self.assertLess(corridor(before).difference(corridor(after)).area, 1e-9)
        self.assertLess(self.context["junction_pads"][0]["added_union_area_m2"], 0.382)

    def test_twelve_whole_double_float32_caps_and_seven_true_frontiers(self):
        proof = contacts(self.data, self.rows, bindings(CORNER, self.rows))
        self.assertEqual(len(proof), 12)
        for cap in proof:
            self.assertEqual(len(cap["whole_cap_checks"]), 2)
            self.assertTrue(
                all(r["uncovered_length_m"] == 0 for r in cap["whole_cap_checks"])
            )
        self.assertEqual(
            outer_ends(self.rows, self.nodes),
            [(16, 0), (18, -1), (20, 0), (21, 0), (21, -1), (22, 0), (23, -1)],
        )

    def test_all_original_underpasses_stairs_and_closed_vertex_links_remain(self):
        pairs, _ = interactions(self.rows, self.nodes)
        _, previous, nodes, _, _, _ = load(CORRIDORS)
        before, _ = interactions(previous, nodes)
        self.assertEqual(pairs, before)
        proof = audit(
            self.data,
            [corridor(r) for r in self.rows],
            limits=[r["gradient"] for r in self.rows],
            crossings=pairs,
        )
        self.assertEqual(len(proof["clearance"]), 10)
        self.assertEqual(sum(r["risers"] for r in proof["treads"].values()), 24)
        vertex_links(self.data["body_vertices"], self.data["body_faces"])


if __name__ == "__main__":
    unittest.main()
