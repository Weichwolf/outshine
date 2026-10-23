import copy
import hashlib
import json
import math
import unittest
from itertools import pairwise

from mvt_stair_native_crosscuts import FIXTURE, cross_section, inspect


class NativeCrosscutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case = json.loads(FIXTURE.read_text())

    def first(self):
        return copy.deepcopy(self.case["cuts"][0])

    def section(self, cut):
        return cross_section(
            cut["triangles"], cut["center_xy_m"], cut["tangent_xy"], cut["half_span_m"]
        )

    def test_original_context_and_station_are_retained(self):
        parent = FIXTURE.with_name("mvt_stair_boundary_neighbours.json")
        self.assertEqual(
            hashlib.sha256(parent.read_bytes()).hexdigest(),
            self.case["provenance"]["parent_input_sha256"],
        )
        for cut in self.case["cuts"]:
            remaining = cut["source_station_m"]
            axis = cut["original_source_xy_m"]
            for a, b in pairwise(axis):
                length = math.dist(a, b)
                if remaining <= length:
                    expected = [
                        a[i] + remaining * (b[i] - a[i]) / length for i in range(2)
                    ]
                    self.assertLess(math.dist(expected, cut["center_xy_m"]), 1e-12)
                    break
                remaining -= length
            else:
                self.fail("recorded station is outside the original source")

    def test_all_four_actual_bands_include_the_shoulders(self):
        profiles = inspect(self.case)
        self.assertEqual(len(profiles), 4)
        for profile, width in zip(profiles, [2.025, 2.025, 2.0516, 2.025], strict=True):
            self.assertEqual(len(profile["segments"]), 6)
            self.assertAlmostEqual(profile["width_m"], width, delta=1e-4)
            self.assertLess(profile["maximum_seam_offset_error_m"], 1e-10)
            self.assertLess(profile["maximum_seam_height_error_m"], 1e-10)
        for profile in profiles[2:]:
            self.assertGreater(
                profile["height_bounds_m"][1] - profile["height_bounds_m"][0], 0.3
            )

    def test_missing_internal_support_is_rejected(self):
        cut = self.first()
        del cut["triangles"][2]
        with self.assertRaisesRegex(ValueError, "coverage gap"):
            self.section(cut)

    def test_intersections_match_the_independent_original_face_planes(self):
        for cut in self.case["cuts"]:
            lookup = {(t["page"], t["triangle"]): t for t in cut["triangles"]}
            along = cut["tangent_xy"]
            for segment in self.section(cut)["segments"]:
                a, b, c = lookup[(segment["page"], segment["triangle"])]["enu_m"]
                u, v = [b[i] - a[i] for i in range(3)], [c[i] - a[i] for i in range(3)]
                normal = [
                    u[1] * v[2] - u[2] * v[1],
                    u[2] * v[0] - u[0] * v[2],
                    u[0] * v[1] - u[1] * v[0],
                ]
                for offset, height in segment["offset_height_m"]:
                    x = cut["center_xy_m"][0] - offset * along[1]
                    y = cut["center_xy_m"][1] + offset * along[0]
                    expected = (
                        a[2]
                        - (normal[0] * (x - a[0]) + normal[1] * (y - a[1])) / normal[2]
                    )
                    self.assertLess(abs(expected - height), 1e-10)

    def test_duplicate_support_is_rejected(self):
        cut = self.first()
        cut["triangles"].append(copy.deepcopy(cut["triangles"][2]))
        with self.assertRaisesRegex(ValueError, "overlapping support"):
            self.section(cut)

    def test_real_height_step_is_rejected(self):
        cut = self.first()
        for point in cut["triangles"][-1]["enu_m"]:
            point[2] += 0.001
        with self.assertRaisesRegex(ValueError, "height discontinuity"):
            self.section(cut)

    def test_narrow_query_cannot_claim_the_complete_band(self):
        cut = self.first()
        cut["half_span_m"] = 0.5
        with self.assertRaisesRegex(ValueError, "complete surface band"):
            self.section(cut)

    def test_coplanar_and_vertical_faces_are_not_flattened(self):
        for xyz, reason in [
            ([[0, -1, 0], [0, 1, 0], [0, 0, 1]], "coplanar"),
            ([[1, 0, 0], [-1, 0, 0], [0, 0, 2]], "vertical"),
        ]:
            triangle = {"page": "synthetic", "triangle": 0, "enu_m": xyz}
            with self.assertRaisesRegex(ValueError, reason):
                cross_section([triangle], [0, 0], [1, 0])

    def test_invalid_and_nonfinite_cuts_are_rejected(self):
        cut = self.first()
        for tangent in ([0, 0], [math.nan, 1]):
            with self.assertRaises(ValueError):
                cross_section(cut["triangles"], cut["center_xy_m"], tangent)


if __name__ == "__main__":
    unittest.main()
