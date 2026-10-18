"""Ground crossing provenance, immutable footways and real carriageway constraints."""

import copy
import unittest

import numpy as np
import shapely
from stair_crossing_paving import paving_crossing
from stair_shared_headroom import binding, declarations
from stair_shared_headroom_checks import scene
from stair_shared_paving import inspect_paving
from stair_shared_paving_checks import square
from stair_surface_access import contact_access


def crossing():
    sources = {
        "road": {"xy_m": [[-5, 0], [-2, 0], [5, 0]], "properties": {"class": "minor"}},
        "foot": {
            "xy_m": [[0, -5], [0, 5]],
            "properties": {"class": "path", "subclass": "footway"},
        },
        "root": {"properties": {"class": "minor"}},
    }
    state = {
        "cached_module_rows": {"foot": {"xy": sources["foot"]["xy_m"], "width_m": 1.8}}
    }
    recipe = {
        "port": {"source_id": "root"},
        "source_profiles": {"road": "unclassified"},
    }
    chosen = {
        "source_id": "road",
        "cached_source_id": "foot",
        "kind": "minor_footway_overlap",
        "modes": ["walk"],
        "access_assumption": "procedural_walk_default",
    }
    return state, sources, recipe, chosen


class GeometricCrossingChecks(unittest.TestCase):
    def test_actual_axis_intersection_is_procedural_without_original_node_claim(self):
        state, sources, recipe, chosen = crossing()
        before = copy.deepcopy(sources)
        result = paving_crossing(state, recipe, sources, chosen)
        self.assertEqual(
            result, {"walk_width_m": 1.8, "procedural_axis_crossing_xy_m": [0, 0]}
        )
        access = contact_access(
            chosen,
            sources["road"],
            sources["foot"],
            state["cached_module_rows"]["foot"],
        )
        self.assertFalse(access["original_node_connection_claimed"])
        self.assertEqual(access["motor_cycle_contact_arcs"], 0)
        self.assertEqual(sources, before)

    def test_parallel_endpoint_multiple_and_nonsimple_crossings_are_refused(self):
        for xy in (
            [[1, -5], [1, 5]],
            [[0, 0], [5, 0]],
            [[-5, -2], [5, -2], [5, 2], [-5, 2]],
            [[-5, -2], [5, 2], [-5, 2], [5, -2]],
        ):
            state, sources, recipe, chosen = crossing()
            sources["road"]["xy_m"] = xy
            with self.subTest(xy=xy), self.assertRaises(ValueError):
                paving_crossing(state, recipe, sources, chosen)

    def test_stale_axis_width_profile_and_false_root_are_refused(self):
        for change in ("axis", "width", "profile", "root", "detour"):
            state, sources, recipe, chosen = crossing()
            if change == "axis":
                state["cached_module_rows"]["foot"]["xy"] = [[0, -5], [0, 6]]
            elif change == "width":
                state["cached_module_rows"]["foot"]["width_m"] = 0.1
            elif change == "profile":
                recipe["source_profiles"]["road"] = "footway"
            elif change == "root":
                recipe["port"]["source_id"] = "foot"
            else:
                recipe["road_axis"] = {}
            with self.subTest(change=change), self.assertRaises(ValueError):
                paving_crossing(state, recipe, sources, chosen)

    def test_non_ground_private_and_oneway_walk_never_become_public_crossings(self):
        for props in (
            {"layer": 1},
            {"brunnel": "tunnel"},
            {"indoor": True},
            {"access": "private"},
            {"foot": "unknown"},
            {"oneway:foot": 1},
        ):
            state, sources, _recipe, chosen = crossing()
            sources["foot"]["properties"].update(props)
            with self.subTest(properties=props), self.assertRaises(ValueError):
                contact_access(
                    chosen,
                    sources["road"],
                    sources["foot"],
                    state["cached_module_rows"]["foot"],
                )

    def test_new_contact_cannot_skip_actual_borrowed_car_crossfall(self):
        row = {
            "source_id": "road",
            "xy": np.array([[-5, 0], [5, 0]]),
            "gradient": 0.1,
            "crossfall_limit": 0.03,
        }
        region = shapely.box(-5, -1, 5, 1)
        state = {
            "module_contacts": [],
            "surface_contacts": [
                {
                    "owner": 0,
                    "kind": "minor_footway_overlap",
                    "cap": square(-1, 1, grade=(0, 0.04)),
                }
            ],
        }
        with self.assertRaisesRegex(ValueError, "road profile frame"):
            inspect_paving(state, [row], [region], square(-5, -4))
        state["surface_contacts"][0]["cap"] = square(-1, 1, grade=(0, 0.01))
        result = inspect_paving(state, [row], [region], square(-5, -4))
        self.assertEqual(len(result[0]["borrowed_caps"]), 2)

    def test_overlap_carries_full_car_headroom_and_refuses_shortened_clearance(self):
        new, old, contact, *_ = scene()
        contact["access"]["kind"] = "minor_footway_overlap"
        report = {
            "valid_local_module": True,
            "proof": {"module": {"surface_contacts": [contact]}},
        }
        self.assertEqual(len(declarations(report, new.identity)), 1)
        self.assertEqual(binding(new, old, contact)["required_m"], 4.5)
        new.rows[0]["traffic_profile"]["clearance_m"] = 2.5
        with self.assertRaisesRegex(ValueError, "full new carriageway headroom"):
            binding(new, old, contact)


if __name__ == "__main__":
    unittest.main()
