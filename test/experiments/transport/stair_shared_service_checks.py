"""Shared physical lane, independent opposing body sweeps and unchanged old recipes."""

import copy
import unittest

import numpy as np
from outshine_ref.transport.horizontal import plan_horizontal_axis
from stair_module_inputs import reference_profiles
from stair_module_roads import road_profile
from stair_road_vehicle import lane_directions
from stair_seed_audit import audit
from stair_seed_checks import example
from stair_seed_geometry import build
from stair_service_checks import straight_example
from stair_service_vehicle import inspect_service


def shared_example():
    row, data = straight_example()
    props = row["original"]["source"]["feature"]["properties"]
    del props["oneway"]
    row.update(
        road_profile(
            {"properties": props},
            reference_profiles()["shared_service"],
            "shared_service",
        )
    )
    return row, data


class SharedService(unittest.TestCase):
    def test_one_physical_space_preserves_source_and_restricted_access(self):
        row, data = shared_example()
        props = row["original"]["source"]["feature"]["properties"]
        traffic = row["traffic_profile"]
        self.assertNotIn("oneway", props)
        self.assertEqual(props["access"], "private")
        self.assertEqual(traffic["direction_origin"], "procedural_default")
        self.assertIsNone(traffic["source_oneway"])
        self.assertFalse(traffic["public_routing_admitted"])
        self.assertEqual(traffic["physical_lanes"], 1)
        self.assertEqual(lane_directions(traffic), [(0, False), (0, True)])
        proof = inspect_service(data, [row])
        self.assertEqual(len(proof["checks"]), 4)
        self.assertEqual(
            {(r["float32"], r["reverse"]) for r in proof["checks"]},
            {(False, False), (False, True), (True, False), (True, True)},
        )

    def test_explicit_zero_direction_is_distinct_from_procedural_default(self):
        result = road_profile(
            {"properties": {"class": "service", "oneway": 0}},
            reference_profiles()["shared_service"],
            "shared_service",
        )
        self.assertEqual(
            result["traffic_profile"]["direction_origin"], "supplied_two_way"
        )
        self.assertEqual(result["traffic_profile"]["source_oneway"], 0)

    def test_oneway_malformed_conditional_and_non_ground_inputs_reject(self):
        for extra in (
            {"oneway": 1},
            {"oneway": -1},
            {"oneway": True},
            {"oneway": None},
            {"oneway": "0"},
            {"oneway:conditional": "yes @ (Mo-Fr)"},
            {"class": "minor"},
            {"subclass": "footway"},
            {"brunnel": "bridge"},
            {"layer": 1},
            {"level": -1},
            {"indoor": 1},
        ):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                road_profile(
                    {"properties": {"class": "service", **extra}},
                    reference_profiles()["shared_service"],
                    "shared_service",
                )

    def test_exclusive_space_contract_cannot_be_removed_or_doubled(self):
        original, data = shared_example()
        for key, value in (
            ("physical_lanes", 2),
            ("opposing_traffic_policy", "simultaneous"),
            ("lane_use", "separate"),
            ("lane_width_m", 1.75),
        ):
            row = copy.deepcopy(original)
            row["traffic_profile"][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                inspect_service(data, [row])

    def test_asymmetric_body_extents_swap_at_original_ends(self):
        row, data = shared_example()
        checks = inspect_service(data, [row])["checks"]
        np.testing.assert_allclose(
            checks[0]["source_end_rear_axle_insets_m"], [1.15, 3.35], rtol=0, atol=1e-14
        )
        np.testing.assert_allclose(
            checks[1]["source_end_rear_axle_insets_m"], [3.35, 1.15], rtol=0, atol=1e-14
        )
        for check in checks:
            sweep = check["straight_sweeps"][0]
            self.assertTrue(sweep["fits_surface"])
            direction = np.asarray(sweep["end_xy_m"]) - sweep["begin_xy_m"]
            self.assertEqual(direction[0] < 0, check["reverse"])

    def test_two_compact_bodies_cannot_be_misrepresented_as_two_half_lanes(self):
        row, data = shared_example()
        compact = inspect_service(data, [row])["curves"]["vehicle"]
        vehicle = dict(compact, widthM=2 * compact["widthM"])
        with self.assertRaisesRegex(ValueError, "leaves actual cap"):
            inspect_service(data, [row], vehicle=vehicle)

    def test_bent_shared_seed_checks_both_whole_bodies_and_original_topology(self):
        row, _data = shared_example()
        row["xy"] = np.array([[0, 0], [40, 0], [40, 40]], float)
        row["prior"] = np.full(3, 100.0)
        original = row["xy"].copy()
        row["surface_axis_xy_m"], row["horizontal_curve_plan"] = plan_horizontal_axis(
            row["xy"],
            {
                "model": "clothoid",
                "radius_m": 6.0,
                "error_m": 0.025,
                "end_guard_m": 4.0,
                "maximum_shift_m": 6.0,
            },
        )
        sources = {
            "service": {
                "mvt_xy": original.astype(int).tolist(),
                "xy_m": original.tolist(),
            }
        }
        untouched = copy.deepcopy(sources)
        _s, _r, recipe, state, empty = example()
        recipe.update(source_ids=["service"], seed={"source_id": "service", "end": 0})
        data, _network, planning = build(
            [row], sources, empty, lambda xy: np.full(len(xy), 100.0)
        )
        proof = audit(state, recipe, data, [row], planning, empty, sources=sources)
        self.assertEqual(sources, untouched)
        np.testing.assert_array_equal(row["xy"], original)
        curves = proof["service_lane_bodies"]["curves"]["checks"]
        self.assertEqual(len(curves), 4)
        self.assertEqual(
            {(c["side"], c["reverse"]) for c in curves}, {(0, False), (0, True)}
        )
        self.assertTrue(all(c["fits_surface"] for c in curves))
        self.assertTrue((data["surface_materials"] == 8).all())


if __name__ == "__main__":
    unittest.main()
