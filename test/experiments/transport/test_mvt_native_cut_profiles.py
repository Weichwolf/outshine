import copy
import json
import unittest

import numpy as np
from mvt_native_cut_profiles import canonical_profile, cut_profiles
from mvt_stair_native_crosscuts import FIXTURE, cross_section
from stair_test_transform import assert_shared_vertices, rigid_coordinates


class NativeCutProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.case = json.loads(FIXTURE.read_text())

    def test_complete_native_segments_and_shoulders_are_retained(self):
        for cut, profile in zip(
            self.case["cuts"], cut_profiles(self.case), strict=True
        ):
            raw = cross_section(
                cut["triangles"],
                cut["center_xy_m"],
                cut["tangent_xy"],
                cut["half_span_m"],
            )
            self.assertEqual(profile["raw_section"], raw)
            self.assertEqual(len(profile["profile_xyz_m"]), 8)
            self.assertEqual(len(profile["native_faces"]), 6)
            self.assertGreater(raw["width_m"], 2)
            self.assertEqual(profile["offsets_m"][profile["axis_vertex"]], 0)
            self.assertLess(
                np.linalg.norm(
                    np.array(profile["profile_xyz_m"])[profile["axis_vertex"], :2]
                    - cut["center_xy_m"]
                ),
                1e-12,
            )

    def test_piecewise_heights_match_each_original_face_plane(self):
        for cut in self.case["cuts"]:
            profile = canonical_profile(cut)
            xyz = np.array(profile["profile_xyz_m"])
            offset = np.array(profile["offsets_m"])
            for segment in profile["raw_section"]["segments"]:
                a, b = segment["offset_height_m"]
                middle = (a[0] + b[0]) / 2
                self.assertAlmostEqual(
                    float(np.interp(middle, offset, xyz[:, 2])),
                    (a[1] + b[1]) / 2,
                    delta=1e-10,
                )

    def test_triangle_order_and_orientation_do_not_change_canonical_points(self):
        for original in self.case["cuts"]:
            cut = copy.deepcopy(original)
            cut["triangles"].reverse()
            for triangle in cut["triangles"]:
                triangle["enu_m"].reverse()
            expected, actual = canonical_profile(original), canonical_profile(cut)
            np.testing.assert_array_equal(
                expected["profile_xyz_m"], actual["profile_xyz_m"]
            )
            self.assertEqual(expected["native_faces"], actual["native_faces"])

    def test_rotated_translated_mesh_keeps_its_native_profile(self):
        coordinates, rotation = rigid_coordinates(0.437, np.array([37.0, -81.0]))
        for original in self.case["cuts"]:
            cut = copy.deepcopy(original)
            for triangle in cut["triangles"]:
                triangle["enu_m"] = coordinates(triangle["enu_m"])
            cut["center_xy_m"] = coordinates(cut["center_xy_m"])
            cut["original_source_xy_m"] = coordinates(cut["original_source_xy_m"])
            cut["tangent_xy"] = (np.array(cut["tangent_xy"]) @ rotation.T).tolist()
            assert_shared_vertices(self, [original], [cut])
            expected = coordinates(canonical_profile(original)["profile_xyz_m"])
            np.testing.assert_allclose(
                canonical_profile(cut)["profile_xyz_m"], expected, rtol=0, atol=1e-10
            )

    def test_input_arrays_and_returned_metadata_are_independent(self):
        cut = copy.deepcopy(self.case["cuts"][0])
        cut["tangent_xy"] = np.array(cut["tangent_xy"]) * 7
        before = cut["tangent_xy"].copy()
        profile = canonical_profile(cut)
        np.testing.assert_array_equal(cut["tangent_xy"], before)
        profile["center_xy_m"][0] += 1
        self.assertEqual(cut["center_xy_m"], self.case["cuts"][0]["center_xy_m"])

    def test_near_equal_edges_are_not_promoted_to_mesh_identity(self):
        cut = copy.deepcopy(self.case["cuts"][0])
        cut["triangles"][2]["enu_m"][0][2] += 1e-10
        cross_section(
            cut["triangles"], cut["center_xy_m"], cut["tangent_xy"], cut["half_span_m"]
        )
        with self.assertRaisesRegex(ValueError, "exact shared-edge chain"):
            canonical_profile(cut)

    def test_duplicate_ports_and_bounded_work_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "unique source/cut"):
            cut_profiles({"cuts": [self.case["cuts"][0]] * 2})
        with self.assertRaisesRegex(ValueError, "bounded nonempty"):
            cut_profiles({"cuts": []})
        cut = copy.deepcopy(self.case["cuts"][0])
        cut["triangles"] *= 22
        with self.assertRaisesRegex(ValueError, "bounded native cut face"):
            canonical_profile(cut)
