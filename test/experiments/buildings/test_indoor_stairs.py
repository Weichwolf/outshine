"""Discrete steps, real openings, union-envelope clearance and actual raw ports."""

import copy
import math
import unittest
from dataclasses import replace
from itertools import pairwise

import numpy as np
import shapely
from indoor_floor_geometry import produce_floor_contacts
from indoor_floor_inputs import load_inputs
from indoor_floor_plan import FloorLimits, FloorSurface
from indoor_stair_contacts import bind_stairs
from indoor_stair_geometry import stair_mesh
from indoor_stair_recipe import StairLimits, profile, schedule
from indoor_stair_site import build_stair, source_clearance
from test_indoor_floors import DATA, FRAME, request, volume


def chosen_site(length=18.5854):
    part = volume("body", shapely.box(-6, -4, length + 6, 4))
    low = FloorSurface("low", FRAME, 0, part.polygon, 0, 0.2, ("body",))
    high = replace(low, identifier="high", level=1, lower_z_m=6.5, upper_z_m=6.7)
    queries = (
        request("low-way", [(-5, 0), (0, 0)]),
        request("high-way", [(length, 0), (length + 5, 0)], 1),
    )
    floors = {
        "floors": (low, high),
        "adjusted_volumes": (part,),
        "programmes": [{"source_ids": ("body",), "base_z_m": 0}],
        "choices": [
            {"source_id": q.identifier, "status": "ready", "floor_id": f.identifier}
            for q, f in zip(queries, (low, high), strict=True)
        ],
    }
    stair = request("stair", [(0, 0), (length, 0)])
    stair = replace(
        stair, original={"width_m": 1.5}, purpose="unplanned_indoor_vertical_connector"
    )
    binding = {
        "reason": "",
        "request": stair,
        "ends": [{"floor_id": f.identifier, "z_m": f.upper_z_m} for f in (low, high)],
    }
    return binding, floors, queries


class StairConstruction(unittest.TestCase):
    def test_finite_search_rescues_real_length_and_matches_independent_family(self):
        limits = StairLimits()
        length, height = 18.5854, 6.5
        self.assertIsNone(schedule(length, height, limits, "greedy")[0])
        recipe, _ = schedule(length, height, limits)
        candidates = []
        for count in range(1, 65):
            for flights in range(1, 9):
                if count <= flights or math.ceil(count / flights) > 16:
                    continue
                rise = height / count
                tread = (length - 1.5 * (flights + 1)) / (count - flights)
                if (
                    0.14 <= rise <= 0.18
                    and 0.25 <= tread <= 0.35
                    and 0.60 <= 2 * rise + tread <= 0.66
                ):
                    candidates.append(
                        (
                            (
                                flights,
                                abs(rise - 0.17) + abs(2 * rise + tread - 0.63),
                                count,
                            ),
                            count,
                            flights,
                        )
                    )
        expected = min(candidates)
        self.assertEqual((recipe["risers"], recipe["flights"]), expected[1:])
        self.assertLessEqual(max(recipe["flight_risers"]), 16)
        self.assertAlmostEqual(
            (recipe["risers"] - recipe["flights"]) * recipe["tread_m"]
            + (recipe["flights"] + 1) * 1.5,
            length,
        )

    def test_limits_and_impossible_profiles_fail_explicitly(self):
        for changes in (
            {"landing_m": float("nan")},
            {"waist_m": 0.1},
            {"max_risers": True},
        ):
            with self.assertRaises(ValueError):
                StairLimits(**changes)
        for height in (0, -1, float("nan")):
            with self.assertRaises(ValueError):
                schedule(18, height, StairLimits())
        self.assertIsNone(schedule(3, 8, StairLimits())[0])

    def test_actual_rotated_emitted_body_and_discrete_top_levels(self):
        limits = StairLimits()
        recipe, _ = schedule(18.5854, 6.5, limits)
        top, _soffit, outline = profile(18.5854, 6.5, recipe, limits)
        self.assertTrue(outline.is_valid)
        for first, last in pairwise(top):
            if first[0] == last[0]:
                continue
            for fraction in (0.001, 0.5, 0.999):
                at = first[0] + fraction * (last[0] - first[0])
                section = outline.intersection(shapely.LineString([(at, -5), (at, 12)]))
                self.assertGreaterEqual(section.length, 0.2 - 128 * np.spacing(20))
        self.assertEqual(
            sum(a[0] == b[0] and b[1] > a[1] for a, b in pairwise(top)),
            recipe["risers"],
        )
        axis = shapely.LineString(
            [(4200, -300), (4200 + 18.5854 * 0.6, -300 + 18.5854 * 0.8)]
        )
        mesh, surfaces, audit = stair_mesh(axis, 1.5, 160.2, 166.7, recipe, limits)
        self.assertEqual(len({s["z_m"] for s in surfaces}), recipe["risers"] + 1)
        self.assertLessEqual(
            audit["volume_error_m3"], audit["volume_arithmetic_bound_m3"]
        )
        self.assertLessEqual(
            audit["emitted_tread_area_error_m2"],
            audit["tread_arithmetic_area_bound_m2"],
        )
        self.assertGreater(len(mesh["faces"]), 0)

    def test_whole_landing_connections_and_real_open_floor_plate(self):
        binding, floors, queries = chosen_site()
        original = [f.polygon.wkb for f in floors["floors"]]
        result = build_stair(binding, floors, queries)
        self.assertEqual(result["status"], "ready", result)
        self.assertEqual(len(result["openings"]), 1)
        self.assertGreater(result["openings"][0]["area_m2"], 0)
        self.assertEqual(result["frame"], FRAME)
        self.assertEqual(len(result["reservations"]), len(result["surfaces"]))
        for floor, query in zip(result["floors"], queries, strict=True):
            self.assertTrue(floor.polygon.covers(query.corridor))
        self.assertEqual([f.polygon.wkb for f in floors["floors"]], original)
        self.assertEqual(result["source_audit"]["source_height_bands"], 1)

    def test_stairwell_cannot_cut_another_chosen_walkway(self):
        binding, floors, queries = chosen_site()
        crossing = request("crossing", [(16, -3), (16, 3)], 1)
        floors["choices"].append(
            {"source_id": crossing.identifier, "status": "ready", "floor_id": "high"}
        )
        result = build_stair(binding, floors, (*queries, crossing))
        self.assertEqual(result["reason"], "stairwell_conflicts_with_chosen_walkway")
        self.assertNotIn("mesh", result)

    def test_union_cancels_internal_planes_but_retains_actual_gaps_and_roof(self):
        square = shapely.box(0, 0, 2, 2)
        parts = (volume("bottom", square, 0, 4), volume("top", square, 4, 13))
        surfaces = [{"z_m": 3.0, "polygon": square}]
        checked = source_clearance(parts, 0, surfaces, 2.1, StairLimits())
        self.assertEqual(checked["whole_prism_band_coverage_tests"], 2)
        repeated = source_clearance(
            parts, 0, surfaces * 3, 2.1, StairLimits(), "repeated"
        )
        reused = source_clearance(parts, 0, surfaces * 3, 2.1, StairLimits())
        self.assertEqual(
            repeated["whole_prism_band_coverage_tests"],
            reused["whole_prism_band_coverage_tests"],
        )
        self.assertEqual(
            repeated["source_unions_built"], reused["source_unions_built"] * 3
        )
        for bad in (
            (parts[0], replace(parts[1], lower_m=4.01)),
            (replace(parts[0], polygon=shapely.box(0, 0, 1, 2)), parts[1]),
            (volume("roof", square, 0, 5.2),),
        ):
            with self.assertRaisesRegex(ValueError, "outside_source_envelopes"):
                source_clearance(bad, 0, surfaces, 2.1, StairLimits())

    def test_reversed_source_retains_the_same_chosen_body(self):
        binding, floors, queries = chosen_site()
        forward = build_stair(binding, floors, queries)
        reverse = dict(
            binding,
            request=replace(
                binding["request"], axis=shapely.reverse(binding["request"].axis)
            ),
            ends=binding["ends"][::-1],
        )
        backward = build_stair(reverse, floors, queries)
        np.testing.assert_array_equal(
            forward["mesh"]["vertices"], backward["mesh"]["vertices"]
        )
        self.assertEqual(backward["openings"], forward["openings"])


class ActualStairContacts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inputs = load_inputs(DATA)
        original = {r.identifier: r for r in cls.inputs.requests}
        cls.requests = tuple(
            original.get(r.identifier, r)
            for r in cls.inputs.constraints
            if r.purpose == "flat_indoor_walkway"
        )
        cls.floors = produce_floor_contacts(
            cls.inputs.parts,
            cls.requests,
            cls.inputs.ground,
            method="adapted",
            constraints=cls.inputs.constraints,
            limits=FloorLimits(max_requests=128),
        )
        cls.bindings = bind_stairs(cls.inputs, cls.floors)

    def test_real_source_accounting_and_equal_heights_stay_pending(self):
        self.assertEqual(len(self.requests), 85)
        self.assertEqual(
            sum(r["status"] == "ready" for r in self.floors["choices"]), 73
        )
        self.assertEqual(len(self.bindings), 64)
        equal = [
            b for b in self.bindings if b["reason"] == "chosen_stair_end_heights_equal"
        ]
        self.assertEqual(len(equal), 3)
        for binding in equal:
            result = build_stair(binding, self.floors, self.requests)
            self.assertEqual(result["status"], "pending")
            self.assertNotIn("mesh", result)

    def test_actual_complete_stair_preserves_all_old_xyz_and_request_originals(self):
        inputs = self.inputs
        originals = copy.deepcopy([r.original for r in inputs.requests])
        old = produce_floor_contacts(
            inputs.parts,
            inputs.requests,
            inputs.ground,
            method="adapted",
            constraints=inputs.constraints,
        )
        new = {r["source_id"]: r for r in self.floors["choices"]}
        for choice in old["choices"]:
            if choice["status"] == "ready":
                np.testing.assert_array_equal(
                    choice["xyz"], new[choice["source_id"]]["xyz"]
                )
        binding = next(
            b
            for b in self.bindings
            if b["source_id"] == "openfreemap:14/8939/5680:transportation:4040280470:2"
        )
        result = build_stair(binding, self.floors, self.requests)
        self.assertEqual(result["status"], "ready", result)
        self.assertEqual([r.original for r in inputs.requests], originals)
        self.assertEqual(len(result["openings"]), 1)
        self.assertGreater(result["source_audit"]["whole_prism_band_coverage_tests"], 0)
        final = {f.identifier: f for f in result["floors"]}
        for request0 in self.requests:
            choice = new[request0.identifier]
            if choice["status"] == "ready":
                self.assertTrue(
                    final[choice["floor_id"]].polygon.covers(request0.corridor)
                )


if __name__ == "__main__":
    unittest.main()
