"""Saved source-category and unchanged original geometry controls."""

import unittest

import numpy as np
from compare_far_height_profiles import FIELDS, combine, selected_profiles, subset
from far_height_profiles import plan_source
from probe_far_profile_fields import hit_codes
from test_far_height_profiles import source


class ProfileFieldControls(unittest.TestCase):
    def test_categories_come_from_original_corners_not_interpolated_uv(self):
        mesh = {
            "uv": np.array(
                [[-793, 0], [-793, 1], [-793, 2], [1024, 0], [1025, 1], [1026, 2]],
                np.float32,
            ),
            "indices": np.array([[0, 1, 2], [3, 4, 5]], np.uint32),
        }
        np.testing.assert_array_equal(
            hit_codes(mesh, np.array([0, 1, -1])), [-793, 4, np.iinfo(np.int64).min]
        )
        broken = dict(mesh, indices=np.array([[0, 1, 4]], np.uint32))
        with self.assertRaisesRegex(ValueError, "Categorical material seam"):
            hit_codes(broken, np.array([0]))

    def test_unknown_or_near_geometric_error_keeps_original_source(self):
        surface = source()
        surface["bounds"] = [[-3, -2, 0], [3, 2, 12]]
        plans = {17: plan_source(17, surface)}
        surfaces = {17: surface}
        view = (np.array([0, 0, 5000]), np.eye(3), 700.0, 1280, 720)
        for error in [None, -1, np.nan, np.inf]:
            selected, limits = selected_profiles(
                plans, surfaces, {17: {"shell_error_m": error}}, view
            )
            self.assertEqual(selected, set())
            self.assertIsNone(limits[17])
        metadata = {17: {"shell_error_m": 0.25}}
        self.assertEqual(selected_profiles(plans, surfaces, metadata, view)[0], {17})
        near = (np.array([0, 0, 6]), *view[1:])
        self.assertEqual(selected_profiles(plans, surfaces, metadata, near)[0], set())

    def test_whole_fallback_preserves_all_original_words_and_anonymous_sources(self):
        rng = np.random.default_rng(1045)
        mesh = {
            key: rng.random((6, size)).astype(np.float32)
            for key, size in [("positions", 3), ("uv", 2), ("colour", 4), ("metres", 2)]
        }
        mesh["normal"] = np.arange(6, dtype=np.uint32)
        mesh["indices"] = np.array([[2, 0, 1], [5, 4, 3]], np.uint32)
        mesh["binding"] = np.array(
            [[11, 5, 0, 1, 0, 0], [12, 17, 0, 1, 0, 0]], np.int32
        )
        original = subset(mesh, np.ones(2, bool))
        result = combine([original])
        for field in [*FIELDS, "indices", "source_id", "native_binding"]:
            np.testing.assert_array_equal(result[field], original[field])
        np.testing.assert_array_equal(result["source_id"], [5, 17])
        for field in FIELDS:
            np.testing.assert_array_equal(
                result[field][result["indices"]], mesh[field][mesh["indices"]]
            )


if __name__ == "__main__":
    unittest.main()
