"""Actual neighbour input and the distinction between a join and an overpass."""

import copy
import json
import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, vertical_clearance
from shared_bridge_geometry import corridor
from stair_context_experiment import crossings
from stair_context_inputs import load, validate
from stair_junction_audit import audit, crossing_faces
from stair_junction_body import build
from stair_junction_layout import inputs, layout
from stair_junction_solve import solve


class StairContext(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case, cls.rows, cls.junctions, cls.fixture = load()
        cls.plan = layout(cls.rows, "width", junctions=cls.junctions)
        cls.heights, cls.note = solve(cls.plan)
        cls.body, cls.data, cls.geometry = build(cls.plan, cls.heights)
        parts = shapely.get_parts(
            shapely.LineString(cls.rows[6]["xy"]).intersection(
                shapely.LineString(cls.rows[2]["xy"])
            )
        )
        cls.crosspoint = next(
            np.array(p.coords[0])
            for p in parts
            if p.distance(shapely.Point(cls.rows[2]["xy"][0])) > 1e-4
        )

    def test_real_neighbours_retain_separate_source_and_dem_origins(self):
        self.assertEqual(len(self.rows), 7)
        self.assertEqual(len(self.fixture["sources"]), 81)
        self.assertEqual(len(self.junctions), 4)
        self.assertTrue(self.fixture["vector"]["full_payload_matches_native"])
        self.assertTrue(self.fixture["vector"]["native_original_features_match"])
        self.assertEqual(len(self.fixture["frontier"]), 3)

    def test_saved_source_crossing_records_survive_json_round_trip(self):
        records = crossings(self.rows)
        self.assertEqual(len(records), 2)
        self.assertEqual(json.loads(json.dumps(records)), records)

    def test_both_whole_crossings_meet_actual_float32_clearance(self):
        proof = audit(
            self.data,
            self.plan["domains"],
            limits=[r["gradient"] for r in self.rows],
            crossings=[(1, 2), (6, 2, self.crosspoint)],
        )
        for value in proof["clearance"].values():
            self.assertGreater(value["float32"]["minimum_clearance_m"], 2.5)
        self.assertEqual(self.geometry["risers"], 12)

    def test_a_real_join_is_not_mistaken_for_an_underpass(self):
        owners = self.data["face_sources"]
        v = self.data["body_vertices"]
        naive = vertical_clearance(
            Mesh(v, self.data["top_faces"][(owners & 64) != 0]),
            Mesh(v, self.data["bottom_faces"][(owners & 4) != 0]),
            seconds=5,
            max_faces=4096,
        )
        self.assertTrue(naive["valid"])
        self.assertLess(naive["minimum_clearance_m"], 0)
        low, high = crossing_faces(
            self.data, self.plan["domains"], 6, 2, self.crosspoint
        )
        actual = vertical_clearance(
            Mesh(v, low), Mesh(v, high), seconds=5, max_faces=4096
        )
        self.assertGreater(actual["minimum_clearance_m"], 2.5)

    def test_overpass_cannot_hide_behind_a_shared_contact_label(self):
        data = dict(self.data)
        data["face_sources"] = data["face_sources"].copy()
        p = shapely.Point(self.crosspoint)
        regions = [
            g
            for g in shapely.get_parts(
                self.plan["domains"][6].intersection(self.plan["domains"][2])
            )
            if g.covers(p)
        ]
        xyz = data["body_vertices"][data["top_faces"], :2]
        hit = (data["face_sources"] & 64) != 0
        hit &= (
            shapely.area(shapely.intersection(shapely.polygons(xyz), regions[0])) > 1e-8
        )
        data["face_sources"][np.flatnonzero(hit)[0]] |= 4
        with self.assertRaisesRegex(ValueError, "relabelled as a contact"):
            crossing_faces(data, self.plan["domains"], 6, 2, self.crosspoint)

    def test_original_heights_are_not_replaced_by_new_dem_samples(self):
        from stair_context_inputs import FIXTURE

        with np.load(FIXTURE.with_name(self.fixture["elevation"]["window_file"])) as d:
            raster = dict(d)
        value = copy.deepcopy(self.fixture)
        value["sources"][0]["native_source_height_prior_m"] = value["sources"][0][
            "new_dem_prior_m"
        ]
        with self.assertRaisesRegex(ValueError, "height prior"):
            validate(value, raster, self.case)

    def test_false_full_native_payload_identity_is_rejected(self):
        from stair_context_inputs import FIXTURE

        with np.load(FIXTURE.with_name(self.fixture["elevation"]["window_file"])) as d:
            raster = dict(d)
        value = copy.deepcopy(self.fixture)
        value["vector"]["sha256"] = (
            "002c1f8dc546f45aa3188969d41dbbacc7e1dafd958b1bd6ee7285b63684d132"
        )
        value["vector"]["full_payload_matches_native"] = True
        with self.assertRaisesRegex(ValueError, "false native identity"):
            validate(value, raster, self.case)

    def test_same_source_geometry_has_one_original_part_owner(self):
        domains = [corridor(r) for r in self.rows]
        for actual, expected in zip(self.plan["domains"], domains, strict=True):
            self.assertEqual(actual.symmetric_difference(expected).area, 0)

    def test_far_source_contact_requires_rebuilding_original_bridge(self):
        _case, rows = inputs()
        isolated = layout(rows, "width")
        heights, note = solve(isolated)
        self.assertTrue(note["valid"])
        old = Mesh(np.c_[isolated["xy"], heights], isolated["parts"][2])
        new = Mesh(np.c_[self.plan["xy"], self.heights], self.plan["parts"][2])
        point = [rows[2]["xy"][0]]
        previous = SurfaceSampler(old).sample(point).points_enu_m[0, 2]
        current = SurfaceSampler(new).sample(point).points_enu_m[0, 2]
        self.assertGreater(current - previous, 1.0)


if __name__ == "__main__":
    unittest.main()
