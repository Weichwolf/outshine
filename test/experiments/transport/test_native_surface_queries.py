import copy
import hashlib
import json
import unittest

import numpy as np
from compare_native_surface_queries import compare, sample_requests
from mvt_native_cut_profiles import cut_profiles
from mvt_native_surface_queries import query_profiles
from mvt_t_connection_inputs import FIXTURE, source_rows
from mvt_t_connections import plan_connections


class NativeSurfaceQueries(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = json.loads(
            FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
        )
        cls.crop_bytes = FIXTURE.with_name(
            "mvt_stair_native_surface_crop.json"
        ).read_bytes()
        cls.crop = json.loads(cls.crop_bytes)
        cls.case = json.loads(
            FIXTURE.with_name("mvt_stair_boundary_neighbours.json").read_text()
        )
        cls.proposal = plan_connections(source_rows(cls.case["ways"]))
        cls.seed = next(iter({c["source_id"] for c in cls.proposal["connections"]}))

    def test_complete_crop_matches_original_export_and_controls(self):
        self.assertEqual(
            hashlib.sha256(self.crop_bytes).hexdigest(),
            self.native["provenance"]["complete_cropped_export_sha256"],
        )
        queries = [
            {k: cut[k] for k in ("native_way", "source_station_m")}
            for cut in self.native["cuts"]
        ]
        before = copy.deepcopy((self.native, self.crop))
        _case, profiles, notes = query_profiles(self.native, self.crop, queries)
        self.assertTrue(all(n["status"] == "complete_geometric_profile" for n in notes))
        for actual, control in zip(profiles, cut_profiles(self.native), strict=True):
            self.assertEqual(actual["native_faces"], control["native_faces"])
            np.testing.assert_allclose(
                actual["profile_xyz_m"], control["profile_xyz_m"], atol=1e-10, rtol=0
            )
        self.assertEqual((self.native, self.crop), before)

    def test_tight_windows_do_not_discard_extra_native_surfaces(self):
        queries = sample_requests(self.case, self.proposal, self.seed, (2,))
        _case, profiles, notes = query_profiles(self.native, self.crop, queries)
        self.assertEqual(len(profiles), 2)
        rejected = [n for n in notes if n["status"] == "declined"]
        self.assertEqual(len(rejected), 2)
        _case, profiles, notes = query_profiles(
            self.native, self.crop, rejected, half_span_m=4
        )
        self.assertFalse(profiles)
        self.assertTrue(all("overlapping support" in n["reason"] for n in notes))

    def test_query_scope_and_work_caps_remain_explicit(self):
        cut = self.native["cuts"][0]
        queries = [{k: cut[k] for k in ("native_way", "source_station_m")}]
        with self.assertRaises(ValueError):
            query_profiles(self.native, self.crop, queries * 65)
        corrupt = copy.deepcopy(self.crop)
        corrupt["anchor"][0] += 1
        with self.assertRaises(ValueError):
            query_profiles(self.native, corrupt, queries)
        corrupt = copy.deepcopy(self.crop)
        corrupt["pages"][0]["triangles"].pop()
        with self.assertRaises(ValueError):
            query_profiles(self.native, corrupt, queries)
        _case, profiles, notes = query_profiles(
            self.native, self.crop, queries, half_span_m=128
        )
        self.assertFalse(profiles)
        self.assertIn("outside observed", notes[0]["reason"])

    def test_no_endpoint_or_unknown_source_becomes_a_free_boundary(self):
        cut = self.native["cuts"][0]
        queries = [
            {"native_way": cut["native_way"], "source_station_m": -1},
            {"native_way": -1, "source_station_m": 1},
        ]
        _case, profiles, notes = query_profiles(self.native, self.crop, queries)
        self.assertFalse(profiles)
        self.assertTrue(all(n["status"] == "declined" for n in notes))

    def test_new_outer_ports_form_a_valid_body_without_relaxing_the_fit(self):
        candidates, outer, products, report = compare(
            self.case, self.native, self.crop, (2, 4, 8, 16)
        )
        self.assertEqual(len(candidates["cuts"]), 11)
        self.assertEqual(len(outer["cuts"]), 4)
        self.assertFalse(report["bounded_closest_ports"]["valid"])
        self.assertTrue(report["bounded_outer_ports"]["valid"])
        self.assertIsNotNone(products)
        self.assertEqual(report["outer_body_float32"]["invalid_float32_faces"], 0)
        control_keys = {
            (c["native_way"], c["source_station_m"]) for c in self.native["cuts"]
        }
        actual_keys = {(c["native_way"], c["source_station_m"]) for c in outer["cuts"]}
        self.assertTrue(actual_keys.isdisjoint(control_keys))


if __name__ == "__main__":
    unittest.main()
