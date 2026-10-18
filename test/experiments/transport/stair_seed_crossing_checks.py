"""Joint crossing ownership, provenance and coherent levels in actual D/Q meshes."""

import copy
import unittest

import numpy as np
import shapely
from stair_seed_audit import audit
from stair_seed_checks import example
from stair_seed_crossings import crossing_point, crossings_for, inspect_crossings
from stair_seed_geometry import build
from stair_seed_level_clusters import merge_levels
from stair_seed_precision import GRID_M


def crossing_example():
    sources, rows, recipe, state, empty = example()
    for row, xy, props in (
        (rows[0], [[0, 0], [20, 0]], {"class": "minor"}),
        (rows[1], [[10, -10], [10, 10]], {"class": "path", "subclass": "footway"}),
    ):
        row.update(
            xy=np.array(xy, float),
            prior=np.array(xy) @ [0.01, 0.02] + 100,
            source_grid_m=GRID_M,
        )
        row["original"]["source"]["feature"]["properties"] = props
        sources[row["source_id"]] = {
            "xy_m": xy,
            "mvt_xy": copy.deepcopy(xy),
            "properties": props,
        }
    rows[0].update(
        width_m=5.5,
        gradient=0.1,
        crossfall_limit=0.03,
        profile_kind="unclassified",
        source_recipe="minor_ground_carriageway",
        traffic_profile={"width_m": 5.5, "clearance_m": 4.5},
    )
    rows[1].update(width_m=1.8, gradient=0.12)
    recipe.update(
        seed_grid_m=GRID_M,
        geometric_junctions=[
            {
                "source_id": "a",
                "footway_source_id": "b",
                "kind": "minor_footway_crossing",
                "modes": ["walk"],
                "access_assumption": "procedural_walk_default",
            }
        ],
    )
    return sources, rows, recipe, state, empty


class JointCrossingChecks(unittest.TestCase):
    def test_whole_joint_body_binds_both_sources_and_actual_clearance_without_fake_node(
        self,
    ):
        sources, rows, recipe, state, empty = crossing_example()
        before = copy.deepcopy(sources)
        data, _network, planning = build(
            rows, sources, empty, lambda xy: 100 + xy @ [0.01, 0.02], recipe=recipe
        )
        proof = audit(state, recipe, data, rows, planning, empty, sources=sources)
        contact = proof["geometric_junctions"][0]
        self.assertEqual(sources, before)
        self.assertEqual(planning[0], [])
        self.assertEqual(contact["procedural_axis_crossing_xy_m"], [10, 0])
        self.assertFalse(contact["access"]["original_node_connection_claimed"])
        self.assertEqual(contact["access"]["motor_cycle_contact_arcs"], 0)
        self.assertTrue(
            all(
                c["coowned_faces"] > 0 and c["area_m2"] > 9.9 for c in contact["checks"]
            )
        )
        self.assertTrue(all(c["volume_m3"] > 0 for c in proof["body"]["checks"]))
        self.assertEqual(
            proof["source_clearances"][0]["sources"][0]["required_clearance_m"], 4.5
        )
        with self.assertRaisesRegex(ValueError, "disconnected original source"):
            audit(
                state,
                dict(recipe, geometric_junctions=[]),
                data,
                rows,
                planning,
                empty,
                sources=sources,
            )
        damaged = dict(data, owners=data["owners"].copy())
        coowned = data["owners"].all(axis=1)
        damaged["owners"][coowned, 1] = False
        with self.assertRaisesRegex(ValueError, "coverage differs"):
            inspect_crossings(
                damaged, crossings_for(rows, sources, recipe), planning[1]
            )

    def test_parallel_touch_multiple_and_original_shared_nodes_remain_distinct(self):
        for xy in (
            [[1, -5], [1, 5]],
            [[10, 0], [10, 10]],
            [[5, -5], [5, 5], [15, 5], [15, -5]],
        ):
            sources, _rows, _recipe, _state, _empty = crossing_example()
            if xy[0] == [1, -5]:
                xy = [[0, 1], [20, 1]]
            sources["b"]["xy_m"] = xy
            with self.subTest(xy=xy), self.assertRaises(ValueError):
                crossing_point(sources["a"], sources["b"])
        sources, *_rest = crossing_example()
        for key in ("a", "b"):
            sources[key]["mvt_xy"].insert(1, [10, 0])
        with self.assertRaisesRegex(ValueError, "original shared nodes"):
            crossing_point(sources["a"], sources["b"])

    def test_wrong_source_kind_access_and_ground_level_are_not_geometric_permission(
        self,
    ):
        for field, value in (
            ("brunnel", "tunnel"),
            ("indoor", True),
            ("layer", 1),
            ("access", "private"),
            ("foot", "unknown"),
            ("oneway:foot", 1),
        ):
            sources, rows, recipe, *_rest = crossing_example()
            sources["b"]["properties"][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                crossings_for(rows, sources, recipe)
        for change in ("duplicate", "budget", "source", "mode", "profile"):
            sources, rows, recipe, *_rest = crossing_example()
            if change == "duplicate":
                recipe["geometric_junctions"] *= 2
            elif change == "budget":
                recipe["geometric_junctions"] *= 5
            elif change == "source":
                recipe["geometric_junctions"][0]["footway_source_id"] = "missing"
            elif change == "mode":
                recipe["geometric_junctions"][0]["modes"] = ["motor"]
            else:
                rows[0]["profile_kind"] = "footway"
            with self.subTest(change=change), self.assertRaises(ValueError):
                crossings_for(rows, sources, recipe)

    def test_touching_level_regions_merge_transitively_and_disjoint_regions_stay_separate(
        self,
    ):
        plans = [
            {
                "shape": shapely.box(x, 0, x + width, 1),
                "height_m": height,
                "mvt_xy": [x, 0],
            }
            for x, width, height in [
                (0, 1, 100),
                (2, 1, 102),
                (0.5, 2, 101),
                (5, 1, 104),
            ]
        ]
        result = merge_levels(plans)
        self.assertEqual(sorted(len(p["members"]) for p in result), [1, 3])
        merged = next(p for p in result if len(p["members"]) == 3)
        self.assertEqual(merged["height_m"], 101)
        self.assertTrue(merged["shape"].equals(shapely.box(0, 0, 3, 1)))
        with self.assertRaises(ValueError):
            merge_levels(plans * 6)


if __name__ == "__main__":
    unittest.main()
