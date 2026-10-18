"""Cyclic seams, retained holes and rejection of repeated/self-crossing axes."""

import copy
import unittest

import numpy as np
import shapely
from stair_module_roots import root_profile
from stair_seed_audit import audit
from stair_seed_axes import axis_form
from stair_seed_checks import example
from stair_seed_geometry import build, seed_contract


def ring_example():
    sources, rows, recipe, state, empty = example()
    xy = np.array([[0, 0], [20, 0], [20, 20], [0, 20], [0, 0]], float)
    sources = {"a": {"mvt_xy": xy.astype(int).tolist(), "xy_m": xy.tolist()}}
    row = rows[0]
    row.update(xy=xy, prior=np.full(len(xy), 100.0))
    recipe["source_ids"] = ["a"]
    return sources, [row], recipe, state, empty


class ClosedGroundSeeds(unittest.TestCase):
    def test_closed_original_axis_keeps_its_ring_and_has_no_outer_port(self):
        sources, rows, recipe, state, empty = ring_example()
        original = copy.deepcopy(sources)
        seed_contract(recipe, sources)
        data, _network, planning = build(
            rows, sources, empty, lambda xy: 100 + 0.01 * xy[:, 0]
        )
        proof = audit(state, recipe, data, rows, planning, empty)
        self.assertEqual(sources, original)
        self.assertEqual(proof["profiles"]["outer_profile_coverage"], [])
        seams = proof["profiles"]["closed_seam_profiles"]
        self.assertEqual(len(seams), 1)
        self.assertEqual([r["float32"] for r in seams[0]["checks"]], [False, True])
        surface = shapely.union_all(
            shapely.polygons(data["surface_vertices"][data["surface_faces"], :2])
        )
        self.assertEqual(surface.geom_type, "Polygon")
        self.assertEqual(len(surface.interiors), 1)
        self.assertFalse(surface.intersects(shapely.box(2, 2, 18, 18)))
        self.assertEqual(axis_form(sources["a"]), "closed")
        with self.assertRaisesRegex(ValueError, "no outer endpoint"):
            root_profile({}, {"port": {"source_id": "a", "end": 0}}, sources)

    def test_repeated_interior_self_crossing_and_false_closure_are_refused(self):
        for xy in (
            [[0, 0], [2, 0], [2, 2], [2, 0], [0, 2]],
            [[0, 0], [2, 2], [0, 2], [2, 0], [0, 0]],
            [[0, 0], [2, 0], [0, 0]],
            [[0, 0], [0, 0], [2, 2], [0, 2], [0, 0]],
        ):
            with self.subTest(xy=xy), self.assertRaises(ValueError):
                axis_form({"mvt_xy": xy, "xy_m": xy})
        source = {"mvt_xy": [[0, 0], [2, 0], [0, 2], [0, 0]]}
        for xy in (
            [[0, 0], [2, 0], [0, 2], [0, 0.1]],
            [[0, 0], [2, 0], [0, 2], [np.nan, 0]],
        ):
            with self.assertRaises(ValueError):
                axis_form(dict(source, xy_m=xy))


if __name__ == "__main__":
    unittest.main()
