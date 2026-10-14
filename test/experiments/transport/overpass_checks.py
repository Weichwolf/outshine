"""Independent affine-gap, thickness, local-profile, layer and real-source checks."""

import copy
import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.transport import close_surface
from overpass_products import products
from overpass_recipe import build
from overpass_ribbon import source_frame
from surface_solid_checks import audit_closed

FIXTURE = Path(__file__).resolve().parent / "fixtures/overpass-vienna-cases.json"


def control(length=400.0):
    return {
        "id": "control",
        "upper": {
            "source_id": "upper",
            "xy": [[0.0, 0.0], [length, 0.0]],
            "end_heights_m": [0.0, 0.0],
            "width_m": 8.0,
            "crossfall": 0.015,
            "grade": 0.08,
            "properties": {"brunnel": "bridge", "layer": 1},
        },
        "lower": {
            "source_id": "lower",
            "xy": [[length / 2, -40.0], [length / 2, 40.0]],
            "end_heights_m": [0.0, 0.0],
            "width_m": 6.0,
            "crossfall": 0.015,
            "grade": 0.08,
            "properties": {"layer": 0},
        },
    }


class OverpassChecks(unittest.TestCase):
    def test_complete_synthetic_profiles_contacts_volume_and_ground_under_deck(self):
        case = control()
        for method in ("parabolic", "cubic"):
            meshes, note = build(case, method=method)
            self.assertIsNotNone(meshes)
            self.assertGreaterEqual(note["clearance"]["minimum_clearance_m"], 4.5)
            for mesh, depth in zip(meshes, (0.35, 0.25)):
                self.assertGreater(audit_closed(close_surface(mesh, depth)), 0.0)
            high = SurfaceSampler(meshes[0]).sample([[200.0, 0.0]])
            low = SurfaceSampler(meshes[1]).sample([[200.0, 0.0]])
            self.assertGreater(
                high.points_enu_m[0, 2] - low.points_enu_m[0, 2] - 0.35, 4.5
            )
            np.testing.assert_allclose(
                meshes[0].vertices[[0, 1, -2, -1], 2],
                [-0.06, 0.06, -0.06, 0.06],
                atol=1e-7,
            )
            plan, binding = products(case, meshes)
            ground = BuildingFootprint(
                "under", shapely.box(199.0, -0.1, 201.0, 0.1), HeightInterval(-0.1, 1.0)
            )
            structure = [
                r
                for r in plan.reservations
                if r.owner_id == "upper" and r.purpose == "transport_structure"
            ]
            self.assertFalse(any(r.overlaps(ground) for r in structure))
            self.assertEqual(binding["mesh_order"][1]["kind"], "ground_traffic")

    def test_short_approach_is_local_failure_and_parabolic_can_fit_extra_case(self):
        self.assertEqual(
            build(control(100.0))[1]["reason"], "insufficient_approach_length"
        )
        self.assertIsNotNone(build(control(240.0))[0])
        self.assertIsNone(build(control(240.0), method="cubic")[0])

    def test_layer_orders_but_never_multiplies_height_and_same_level_is_not_joined(
        self,
    ):
        base = control()
        changed = copy.deepcopy(base)
        changed["upper"]["properties"]["layer"] = 10
        for a, b in zip(build(base)[0], build(changed)[0]):
            np.testing.assert_array_equal(a.vertices, b.vertices)
        changed["upper"]["properties"]["layer"] = 0
        self.assertEqual(build(changed)[1]["reason"], "ambiguous_level_order")
        changed["lower"]["properties"]["subclass"] = "steps"
        self.assertEqual(build(changed)[1]["reason"], "unsupported_stair_surface")

    def test_sharp_turns_and_oversized_input_do_not_become_folded_surfaces(self):
        for xy in ([[0.0, 0.0], [10.0, 0.0], [10.0, 10.0]], np.zeros((257, 2))):
            with self.assertRaises(ValueError):
                source_frame(xy, 4.0)

    def test_saved_pairs_keep_input_and_all_failures_remain_source_bound(self):
        before = hashlib.sha256(FIXTURE.read_bytes()).hexdigest()
        inputs = json.loads(FIXTURE.read_text())
        cases = inputs["cases"]
        for source in inputs["inputs"]:
            ids = source["considered_axis_ids"] + source["unselected_axis_ids"]
            self.assertEqual(len(ids), source["axis_parts"])
            self.assertEqual(len(set(ids)), len(ids))
            self.assertEqual(
                len(source["deferred_source_ids"]), source["deferred_sources"]
            )
        self.assertEqual(len(cases), 53)
        for method, expected in (("parabolic", 11), ("cubic", 4)):
            valid = 0
            for case in cases:
                meshes, note = build(case, method=method)
                self.assertEqual(note["upper_source"], case["upper"]["source_id"])
                self.assertEqual(note["lower_source"], case["lower"]["source_id"])
                if meshes is None:
                    self.assertTrue(note["reason"])
                else:
                    valid += 1
                    self.assertLessEqual(
                        note["upper"]["maximum_normalized_grade"], 1.0000001
                    )
                    self.assertTrue(note["rigid_overlap_core_covers"])
                    self.assertGreaterEqual(
                        note["clearance"]["minimum_clearance_m"], 4.5 - 1e-7
                    )
                    for mesh, depth in zip(meshes, (0.35, 0.25)):
                        self.assertGreater(
                            audit_closed(close_surface(mesh, depth)), 0.0
                        )
            self.assertEqual(valid, expected)
        self.assertEqual(before, hashlib.sha256(FIXTURE.read_bytes()).hexdigest())


if __name__ == "__main__":
    unittest.main()
