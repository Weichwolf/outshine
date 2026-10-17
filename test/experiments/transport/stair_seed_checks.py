"""Whole independent seed profiles, real clearance and original-graph counterexamples."""

import copy
import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_local_assets import road_window
from stair_local_physics import bridge_checks
from stair_module_family import verify_sources
from stair_seed_audit import audit
from stair_seed_geometry import build, seed_contract


def example():
    sources, rows = {}, []
    for key, xy in (("a", [[0, 0], [10, 0]]), ("b", [[10, 0], [10, 10]])):
        sources[key] = {"xy_m": copy.deepcopy(xy), "mvt_xy": copy.deepcopy(xy)}
        rows.append(
            {
                "source_id": key,
                "xy": np.array(xy, float),
                "prior": np.full(2, 100.0),
                "width_m": 2.0,
                "gradient": 0.15,
                "original": {
                    "bridge": False,
                    "source": {"feature": {"properties": {"subclass": "footway"}}},
                },
            }
        )
    recipe = {
        "contact_mode": "seed",
        "junction_mode": "level_box",
        "source_ids": ["a", "b"],
        "seed": {"source_id": "a", "end": 0},
    }
    road, ground, _keys, _rows, _work = road_window(
        [], shapely.box(-10, -10, 20, 20), allow_empty=True
    )
    state = {"road": road, "road_ground": ground, "piers": []}
    empty = Mesh(np.empty((0, 3)), np.empty((0, 3), int))
    return sources, rows, recipe, state, empty


class IndependentGroundSeeds(unittest.TestCase):
    def test_empty_window_is_explicit_and_still_cannot_hide_an_old_cap(self):
        with self.assertRaisesRegex(ValueError, "working budget"):
            road_window([], shapely.box(0, 0, 1, 1))
        road, ground, keys, rows, note = road_window(
            [], shapely.box(0, 0, 1, 1), allow_empty=True
        )
        self.assertEqual(road["body_vertices"].shape, (0, 3))
        self.assertEqual(road["face_sources"].dtype, np.dtype("uint64"))
        self.assertEqual(
            (keys, rows, len(ground), note["local_sources"]), ([], {}, 0, 0)
        )
        empty = Mesh(road["body_vertices"], road["body_faces"])
        field = Mesh(np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]]), np.array([[0, 1, 2]]))
        self.assertTrue(
            all(
                p["reason"] == "no_local_bridge_facets"
                for p in bridge_checks(field, empty)
            )
        )

    def test_two_source_seed_has_complete_internal_profiles_and_closed_body(self):
        sources, rows, recipe, state, empty = example()
        seed_contract(recipe, sources)
        data, _network, planning = build(
            rows, sources, empty, lambda xy: np.full(len(xy), 100.0)
        )
        proof = audit(state, recipe, data, rows, planning, empty)
        source = verify_sources(rows, sources, recipe, [], None, planning[0])
        self.assertEqual(source["original_shared_nodes"], 1)
        self.assertEqual(len(proof["profiles"]["new_internal_profiles"]), 2)
        self.assertEqual(len(proof["profiles"]["outer_profile_coverage"]), 2)
        self.assertTrue(all(p["volume_m3"] > 0 for p in proof["body"]["checks"]))
        self.assertTrue(
            all(
                p["minimum_old_ground_distance_m"] is None
                for p in proof["old_ground_separation"]
            )
        )

    def test_actual_old_ground_overlap_requires_a_real_contact_recipe(self):
        sources, rows, recipe, state, empty = example()
        data, _network, planning = build(
            rows, sources, empty, lambda xy: np.full(len(xy), 100.0)
        )
        state["road"] = {
            "body_vertices": data["body_vertices"],
            "top_faces": data["surface_faces"],
        }
        state["road_ground"] = np.ones(len(data["surface_faces"]), bool)
        with self.assertRaisesRegex(ValueError, "touches old ground"):
            audit(state, recipe, data, rows, planning, empty)

    def test_real_overhead_overlap_keeps_required_whole_source_headroom(self):
        sources, rows, recipe, state, empty = example()
        data, _network, planning = build(
            rows, sources, empty, lambda xy: np.full(len(xy), 100.0)
        )
        upper = Mesh(data["surface_vertices"] + [0, 0, 3], data["surface_faces"])
        self.assertTrue(
            audit(state, recipe, data, rows, planning, upper)["source_clearances"]
        )
        with self.assertRaisesRegex(ValueError, "headroom below"):
            audit(
                state,
                recipe,
                data,
                rows,
                planning,
                Mesh(upper.vertices - [0, 0, 1], upper.triangles),
            )

    def test_geometric_coincidence_does_not_replace_original_source_graph(self):
        sources, rows, recipe, _state, empty = example()
        _data, _network, planning = build(
            rows, sources, empty, lambda xy: np.full(len(xy), 100.0)
        )
        sources = copy.deepcopy(sources)
        sources["b"]["mvt_xy"][0] = [11, 0]
        with self.assertRaisesRegex(ValueError, "original source contacts"):
            verify_sources(rows, sources, recipe, [], None, planning[0])
        recipe["port"] = {"source_id": "old", "end": 0}
        with self.assertRaisesRegex(ValueError, "without cached contacts"):
            seed_contract(recipe, sources)


if __name__ == "__main__":
    unittest.main()
