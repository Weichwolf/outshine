"""Physical floor/plate controls, actual input frames and stable demand subsets."""

import copy
import json
import math
import unittest
from dataclasses import replace
from itertools import pairwise, product
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from indoor_floor_checks import floor_clearance
from indoor_floor_geometry import mesh_floor, produce_floor_contacts
from indoor_floor_inputs import load_inputs
from indoor_floor_plan import (
    FloorLimits,
    FloorRequest,
    FloorSurface,
    Volume,
    plan_floors,
)
from outshine_ref.contracts import LocalFrame
from outshine_ref.places.camera import axes, ecef

FRAME = LocalFrame("synthetic-bound-floor-ENU")
DATA = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/buildings/indoor_floor_requests"
)


def volume(name, polygon, low=0.0, high=13.0):
    return Volume(name, FRAME, polygon, low, high, (0, 0), {"original": name})


def request(name, points, level=0):
    axis = shapely.LineString(points)
    return FloorRequest(
        name,
        FRAME,
        axis,
        axis.buffer(0.3, cap_style="flat"),
        level,
        {"id": name, "points": points},
    )


def ground(parts):
    return np.full(len(parts), 160.0)


class FloorControls(unittest.TestCase):
    def setUp(self):
        self.parts = (
            volume("root", shapely.box(0, 0, 10, 5)),
            volume("raised", shapely.box(5, 0, 10, 5), 5, 13),
        )
        self.requests = (
            request("ground", [(1, 2), (9, 2)]),
            request("upper", [(6, 2), (9, 2)], 1),
        )

    def test_source_bands_produce_closed_metric_floors_not_level_metres(self):
        result = produce_floor_contacts(self.parts, self.requests, ground)
        self.assertTrue(all(r["status"] == "ready" for r in result["choices"]))
        heights = {r["source_id"]: r["xyz"][0, 2] for r in result["choices"]}
        self.assertAlmostEqual(heights["ground"], 160.2)
        self.assertAlmostEqual(heights["upper"], 165.2)
        self.assertEqual(len(result["meshes"]), 2)
        old = produce_floor_contacts(
            self.parts, self.requests, ground, method="uniform"
        )
        self.assertEqual(old["choices"][0]["status"], "ready")
        self.assertEqual(old["choices"][1]["status"], "pending")

    def test_demand_subset_and_part_order_do_not_change_chosen_floor(self):
        all_ways = produce_floor_contacts(self.parts, self.requests, ground)
        subset = produce_floor_contacts(self.parts[::-1], self.requests[1:], ground)
        first = all_ways["choices"][1]
        second = subset["choices"][0]
        self.assertEqual(first["floor_id"], second["floor_id"])
        np.testing.assert_array_equal(first["xyz"], second["xyz"])
        for field in ("vertices", "faces", "parts"):
            np.testing.assert_array_equal(
                all_ways["meshes"][first["floor_id"]][field],
                subset["meshes"][second["floor_id"]][field],
            )
        self.assertEqual(all_ways["programmes"], subset["programmes"])

    def test_missing_ground_is_local_and_never_becomes_zero_height(self):
        parts = (
            volume("bad", shapely.box(0, 0, 10, 5)),
            volume("good", shapely.box(20, 0, 30, 5)),
        )
        queries = (
            request("bad-query", [(1, 2), (9, 2)]),
            request("good-query", [(21, 2), (29, 2)]),
        )

        def measured(group):
            return np.full(len(group), np.nan if group[0].identifier == "bad" else 160)

        result = produce_floor_contacts(parts, queries, measured)
        self.assertEqual(
            result["choices"][0]["reason"], "missing_bound_building_elevation"
        )
        self.assertNotIn("xyz", result["choices"][0])
        self.assertEqual(result["choices"][1]["status"], "ready")

    def test_component_budget_does_not_discard_unrelated_valid_parts(self):
        isolated = volume("isolated", shapely.box(20, 0, 30, 5))
        queries = self.requests[:1] + (request("island", [(21, 2), (29, 2)]),)
        result = produce_floor_contacts(
            self.parts + (isolated,), queries, ground, limits=FloorLimits(max_parts=1)
        )
        self.assertEqual(result["choices"][0]["reason"], "building_component_budget")
        self.assertEqual(result["choices"][1]["status"], "ready")
        self.assertEqual(result["work"]["ground_samples"], 1)

    def test_low_source_plane_removes_whole_headroom_region(self):
        beam = volume("beam", shapely.box(5, 0, 10, 5), 1.5, 12)
        queries = (
            request("under", [(6, 2), (9, 2)]),
            request("clear", [(1, 2), (4, 2)]),
        )
        result = produce_floor_contacts((self.parts[0], beam), queries, ground)
        self.assertEqual(result["choices"][0]["status"], "pending")
        self.assertEqual(result["choices"][1]["status"], "ready")
        false_floor = FloorSurface(
            "unclipped", FRAME, 0, self.parts[0].polygon, 160, 160.2, ("body", "beam")
        )
        mesh, _ = mesh_floor(false_floor)
        with self.assertRaisesRegex(ValueError, "source_height_clearance"):
            floor_clearance(
                false_floor, mesh, (self.parts[0], beam), 160, FloorLimits()
            )

    def test_courtyard_and_point_touching_plates_do_not_create_passages(self):
        outer = shapely.box(0, 0, 10, 10)
        ring = outer.difference(shapely.box(4, 4, 6, 6))
        query = request("courtyard", [(1, 5), (9, 5)])
        result = produce_floor_contacts((volume("ring", ring),), (query,), ground)
        self.assertEqual(result["choices"][0]["status"], "pending")
        self.assertTrue(
            all(v["top_domain_error_m2"] == 0 for v in result["mesh_audits"].values())
        )
        pieces = shapely.MultiPolygon(
            [shapely.box(0, 0, 2, 2), shapely.box(2, 2, 4, 4)]
        )
        floor = FloorSurface("separate", FRAME, 0, pieces, 160, 160.2, ("two",))
        mesh, audit = mesh_floor(floor)
        self.assertEqual(audit["closed_plate_parts"], 2)
        self.assertEqual(len(np.unique(mesh["parts"])), 2)
        parts = tuple(
            volume(str(i), p) for i, p in enumerate(shapely.get_parts(pieces))
        )
        crossed = produce_floor_contacts(
            parts, (request("point-only", [(1, 1), (3, 3)]),), ground
        )
        self.assertEqual(crossed["choices"][0]["status"], "pending")

    def test_frame_level_and_mesh_failure_are_explicit(self):
        with self.assertRaises(ValueError):
            plan_floors(
                self.parts,
                (replace(self.requests[0], frame=LocalFrame("other")),),
                ground,
            )
        result = produce_floor_contacts(
            self.parts, (replace(self.requests[0], level=True),), ground
        )
        self.assertEqual(result["choices"][0]["reason"], "unplanned_semantic_level")
        with self.assertRaisesRegex(ValueError, "Unique source"):
            plan_floors(
                self.parts,
                self.requests,
                ground,
                method="constrained",
                constraints=self.requests * 2,
            )
        with patch(
            "indoor_floor_geometry.mesh_floor", side_effect=ValueError("invalid_plate")
        ):
            result = produce_floor_contacts(self.parts, self.requests, ground)
        self.assertTrue(
            all(
                r["reason"] == "invalid_plate" and "xyz" not in r
                for r in result["choices"]
            )
        )

    def test_static_constraint_programme_matches_small_exhaustive_family(self):
        result = produce_floor_contacts(
            self.parts,
            self.requests,
            ground,
            method="constrained",
            constraints=self.requests,
        )
        programme = result["programmes"][0]
        candidates = programme["selection"]["candidate_bands_m"]
        best = None
        for remaining in product(
            (None, *candidates), repeat=len(programme["bands_m"]) - 1
        ):
            path = (0.0, *remaining)
            present = [
                (level, height)
                for level, height in enumerate(path)
                if height is not None
            ]
            if any(b[1] - a[1] < 2.3 for a, b in pairwise(present)):
                continue
            upper = path[1]
            served = upper is not None and (upper >= 5 or upper + 0.2 + 2.1 <= 5)
            objective = (
                1 + int(served),
                len(present),
                -sum(level for level, _ in present),
                -sum(candidates.index(height) for _, height in present),
            )
            best = objective if best is None else max(best, objective)
        self.assertEqual(tuple(programme["selection"]["objective"]), best)
        single = produce_floor_contacts(
            self.parts[::-1],
            self.requests[1:],
            ground,
            method="constrained",
            constraints=self.requests,
        )
        self.assertEqual(programme, single["programmes"][0])
        self.assertEqual(
            result["choices"][1]["floor_id"], single["choices"][0]["floor_id"]
        )
        with self.assertRaises(ValueError):
            produce_floor_contacts(
                self.parts, self.requests, ground, method="constrained"
            )

    def test_explicit_bounded_outline_addition_preserves_original(self):
        part = replace(
            self.parts[0], source={"coordinate_step_m": 0.4, "original": "root"}
        )
        entry = request("entry", [(1, 2), (10.1, 2)])
        before = part.polygon.wkb
        result = produce_floor_contacts(
            (part,), (entry,), ground, method="adapted", constraints=(entry,)
        )
        self.assertEqual(result["choices"][0]["status"], "ready")
        self.assertEqual(part.polygon.wkb, before)
        changed = result["adjusted_volumes"][0]
        self.assertTrue(changed.polygon.covers(entry.corridor))
        self.assertEqual(
            (changed.lower_m, changed.upper_m), (part.lower_m, part.upper_m)
        )
        record = result["outline_adaptation"]["adjustments"][0]
        self.assertLessEqual(record["maximum_added_vertex_distance_m"], 0.2)
        disabled = produce_floor_contacts(
            (part,),
            (entry,),
            ground,
            method="adapted",
            constraints=(entry,),
            limits=FloorLimits(outline_adjustment_m=0),
        )
        self.assertEqual(disabled["choices"][0]["status"], "pending")
        far = request("far", [(1, 2), (10.5, 2)])
        declined = produce_floor_contacts(
            (part,), (far,), ground, method="adapted", constraints=(far,)
        )
        self.assertEqual(declined["choices"][0]["status"], "pending")

    def test_outline_adaptation_cannot_fill_courtyard_or_unknown_precision(self):
        ring = shapely.box(0, 0, 10, 10).difference(shapely.box(4, 4, 6, 6))
        part = replace(volume("court", ring), source={"coordinate_step_m": 0.4})
        through = request("court-way", [(1, 5), (9, 5)])
        result = produce_floor_contacts(
            (part,), (through,), ground, method="adapted", constraints=(through,)
        )
        self.assertEqual(result["choices"][0]["status"], "pending")
        self.assertEqual(result["adjusted_volumes"][0].polygon.wkb, ring.wkb)
        for unknown in (None, float("nan"), 0.0):
            p = replace(self.parts[0], source={"coordinate_step_m": unknown})
            way = request("outside", [(1, 2), (10.1, 2)])
            result = produce_floor_contacts(
                (p,), (way,), ground, method="adapted", constraints=(way,)
            )
            self.assertEqual(result["choices"][0]["status"], "pending")


class ActualFloorInputs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inputs = load_inputs(DATA)

    def test_original_bytes_requests_and_projection_are_preserved(self):
        manifest = json.loads((DATA / "manifest.json").read_text())
        self.assertEqual(
            [r.original for r in self.inputs.requests], manifest["requests"]
        )
        self.assertEqual(len(self.inputs.requests), 31)
        self.assertEqual(
            len(self.inputs.parts) + len(self.inputs.metadata["rejected_parts"]), 4174
        )
        origin = np.deg2rad(self.inputs.origin)
        radius = 40075016.68557849 / (2 * math.pi)
        request0 = self.inputs.requests[0]
        x, y = np.asarray(request0.original["xy"])[0]
        mercator_x = x / math.cos(origin[0]) + radius * origin[1]
        mercator_y = y / math.cos(origin[0]) + radius * math.asinh(math.tan(origin[0]))
        lat = math.degrees(2 * math.atan(math.exp(mercator_y / radius)) - math.pi / 2)
        lon = math.degrees(mercator_x / radius)
        expected = (ecef(lat, lon, 0) - ecef(*self.inputs.origin, 0)) @ axes(
            *self.inputs.origin
        ).T
        np.testing.assert_allclose(
            np.asarray(request0.axis.coords)[0], expected[:2], atol=1e-7, rtol=0
        )
        self.assertGreater(np.linalg.norm(expected[:2] - [x, y]), 0.1)

    def test_actual_closed_plates_and_all_original_responses_survive(self):
        before = copy.deepcopy([r.original for r in self.inputs.requests])
        result = produce_floor_contacts(
            self.inputs.parts, self.inputs.requests, self.inputs.ground
        )
        self.assertEqual(len(result["choices"]), 31)
        self.assertTrue(any(r["status"] == "ready" for r in result["choices"]))
        self.assertFalse(result["mesh_failures"])
        self.assertEqual([r.original for r in self.inputs.requests], before)
        self.assertEqual([r["original"] for r in result["choices"]], before)
        for audit in result["mesh_audits"].values():
            self.assertEqual(audit["top_domain_error_m2"], 0)
            self.assertGreater(audit["signed_volume_m3"], 0)
            clearance = audit["source_height_clearance"]
            self.assertLessEqual(
                clearance["unsupported_top_area_m2"],
                clearance["arithmetic_area_bound_m2"],
            )
            self.assertLessEqual(
                clearance["obstructed_top_area_m2"],
                clearance["arithmetic_area_bound_m2"],
            )

    def test_all_actual_flat_ways_have_bound_contacts_but_steps_stay_explicit(self):
        data = self.inputs
        before = [p.polygon.wkb for p in data.parts]
        result = produce_floor_contacts(
            data.parts,
            data.requests,
            data.ground,
            method="adapted",
            constraints=data.constraints,
        )
        self.assertEqual(len(data.constraints), 219)
        self.assertEqual([p.polygon.wkb for p in data.parts], before)
        self.assertFalse(result["mesh_failures"])
        for request0, row in zip(data.requests, result["choices"], strict=True):
            if request0.purpose == "flat_indoor_walkway":
                self.assertEqual(row["status"], "ready", row)
            else:
                self.assertEqual(row["reason"], "unplanned_indoor_contact_kind")
                self.assertNotIn("xyz", row)
        self.assertEqual(
            len(result["reservations"]),
            sum(r["status"] == "ready" for r in result["choices"]),
        )
        upper = next(
            r
            for r, c in zip(data.requests, result["choices"], strict=True)
            if r.level == 1 and c["status"] == "ready"
        )
        one = produce_floor_contacts(
            data.parts[::-1],
            (upper,),
            data.ground,
            method="adapted",
            constraints=data.constraints,
        )
        old = next(c for c in result["choices"] if c["source_id"] == upper.identifier)
        np.testing.assert_array_equal(one["choices"][0]["xyz"], old["xyz"])
        self.assertEqual(one["choices"][0]["floor_id"], old["floor_id"])


if __name__ == "__main__":
    unittest.main()
