"""One physical lane follows supplied direction without creating public access."""

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
from stair_service_vehicle import inspect_service


def straight_example(direction=1):
    props = {"class": "service", "oneway": direction, "access": "private"}
    row = {
        "source_id": "service",
        "xy": np.array([[0, 0], [40, 0]], float),
        "prior": np.full(2, 100.0),
        "width_m": 3.5,
        "gradient": 0.12,
        "original": {"bridge": False, "source": {"feature": {"properties": props}}},
        **road_profile(
            {"properties": props}, reference_profiles()["service"], "service"
        ),
    }
    data = {
        "surface_vertices": np.array(
            [[0, -1.75, 100], [40, -1.75, 100], [40, 1.75, 100], [0, 1.75, 100]], float
        ),
        "surface_faces": np.array([[0, 1, 2], [0, 2, 3]]),
        "owners": np.ones((2, 1), bool),
    }
    return row, data


class OneWayService(unittest.TestCase):
    def test_private_source_has_one_centered_lane_in_only_its_supplied_direction(self):
        for direction, expected in ((1, [(0, False)]), (-1, [(0, True)])):
            row, _data = straight_example(direction)
            self.assertEqual(lane_directions(row["traffic_profile"]), expected)
            self.assertEqual(row["traffic_profile"]["lane_width_m"], 3.5)
            self.assertFalse(row["traffic_profile"]["public_routing_admitted"])
            self.assertEqual(
                row["original"]["source"]["feature"]["properties"]["access"], "private"
            )

    def test_unsupplied_or_ambiguous_direction_is_not_invented(self):
        for direction in (None, 0, True, "1", 2):
            with self.subTest(direction=direction), self.assertRaises(ValueError):
                road_profile(
                    {"properties": {"class": "service", "oneway": direction}},
                    reference_profiles()["service"],
                    "service",
                )

    def test_mislabeled_non_ground_sources_and_minor_aliases_are_rejected(self):
        for extra in (
            {"class": "minor"},
            {"brunnel": "tunnel"},
            {"level": 1},
            {"indoor": 1},
        ):
            props = {"class": "service", "oneway": 1, **extra}
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                road_profile(
                    {"properties": props}, reference_profiles()["service"], "service"
                )
        with self.assertRaises(ValueError):
            road_profile(
                {"properties": {"class": "service", "oneway": 1}},
                reference_profiles()["unclassified"],
                "unclassified",
            )

    def test_existing_two_lane_direction_choices_remain_exact(self):
        for direction, expected in (
            (0, [(-1, False), (1, True)]),
            (1, [(1, False), (-1, False)]),
            (-1, [(1, True), (-1, True)]),
        ):
            props = {"class": "minor", "oneway": direction}
            profile = road_profile(
                {"properties": props},
                reference_profiles()["unclassified"],
                "unclassified",
            )
            self.assertEqual(profile["source_recipe"], "minor_ground_carriageway")
            self.assertEqual(lane_directions(profile["traffic_profile"]), expected)
            self.assertNotIn("public_routing_admitted", profile["traffic_profile"])

    def test_whole_straight_bodies_pass_both_precisions_without_consuming_end_stubs(
        self,
    ):
        for direction in (1, -1):
            row, data = straight_example(direction)
            proof = inspect_service(data, [row])
            self.assertEqual([c["float32"] for c in proof["checks"]], [False, True])
            for check in proof["checks"]:
                self.assertEqual(check["lane_offset_m"], 0)
                sweep = check["straight_sweeps"][0]
                self.assertTrue(sweep["fits_surface"])
                self.assertEqual(
                    np.sign(sweep["end_xy_m"][0] - sweep["begin_xy_m"][0]), direction
                )
                self.assertTrue(
                    all(x > 0 for x in check["source_end_rear_axle_insets_m"])
                )

    def test_an_overwide_body_cannot_pass_from_centerline_containment(self):
        row, data = straight_example()
        vehicle = {"widthM": 4.0, "frontFromRearAxleM": 3.3, "rearOverhangM": 1.1}
        with self.assertRaisesRegex(ValueError, "leaves actual cap"):
            inspect_service(data, [row], vehicle=vehicle)

    def test_short_source_is_explained_instead_of_extending_its_geometry(self):
        row, data = straight_example()
        row["xy"][1] = [2, 0]
        with self.assertRaisesRegex(ValueError, "complete interior vehicle interval"):
            inspect_service(data, [row])

    def test_bent_axes_need_a_declared_vehicle_curve(self):
        row, data = straight_example()
        row["xy"] = np.array([[0, 0], [20, 0], [20, 20]], float)
        with self.assertRaisesRegex(ValueError, "explicit horizontal curve plan"):
            inspect_service(data, [row])

    def test_curved_seed_retains_original_source_and_actual_centered_vehicle_checks(
        self,
    ):
        row, _data = straight_example()
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
        _s, _r, recipe, state, empty = example()
        recipe.update(source_ids=["service"], seed={"source_id": "service", "end": 0})
        untouched = copy.deepcopy(sources)
        data, _network, planning = build(
            [row], sources, empty, lambda xy: np.full(len(xy), 100.0)
        )
        proof = audit(state, recipe, data, [row], planning, empty, sources=sources)
        self.assertEqual(sources, untouched)
        np.testing.assert_array_equal(row["xy"], original)
        curves = proof["service_lane_bodies"]["curves"]["checks"]
        self.assertEqual(len(curves), 2)
        self.assertTrue(
            all(
                c["side"] == 0 and not c["reverse"] and c["fits_surface"]
                for c in curves
            )
        )
        vehicle = dict(
            proof["service_lane_bodies"]["curves"]["vehicle"],
            minimumRearAxleRadiusM=100.0,
        )
        with self.assertRaisesRegex(ValueError, "rear-axle radius"):
            inspect_service(data, [row], vehicle=vehicle)


if __name__ == "__main__":
    unittest.main()
