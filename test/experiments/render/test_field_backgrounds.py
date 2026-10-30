"""Frozen native opponents stay source-bound and preserve actual failures."""

import copy
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from check_field_backgrounds import (
    INPUTS,
    capture_control,
    native_case,
    pixel_patch,
    read_inputs,
)


class NativeBackgroundTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest, cls.arrays, cls.parts = read_inputs(INPUTS)
        cls.patch = pixel_patch(cls.arrays)

    def test_whole_source_lineage_and_unresolved_originals_remain_explicit(self):
        self.assertEqual(len(self.arrays["source_ids"]), 1891)
        self.assertEqual(sum(len(p["indices"]) for p in self.parts), 46889)
        self.assertEqual(
            [r["source"] for r in self.manifest["source"]["unresolved_originals"]],
            [3825, 3827],
        )
        self.assertFalse({3825, 3827} & set(map(int, self.arrays["source_ids"])))

    def test_ordered_native_points_match_but_separate_averages_do_not(self):
        row, _ = native_case(
            "separated", self.manifest, self.arrays, self.parts, self.patch
        )
        self.assertEqual(row["whole_hull_order"][0], "front_before_back")
        self.assertEqual(row["joint_point_a_over_b"]["rgb_max_abs"], 0)
        self.assertEqual((row["foreground_first"], row["background_first"]), (14, 0))
        self.assertEqual(row["independently_filtered_ss16"]["coverage_max_abs"], 0.25)
        self.assertEqual(
            row["independently_filtered_ss16"]["rgb_max_abs"], 0.7466888427734375
        )

    def test_native_interleave_keeps_originals_and_both_global_orders_fail(self):
        row, _ = native_case(
            "interleaved", self.manifest, self.arrays, self.parts, self.patch
        )
        self.assertEqual(row["whole_hull_order"][0], "originals")
        self.assertEqual((row["foreground_first"], row["background_first"]), (4, 5))
        self.assertGreater(row["joint_point_a_over_b"]["rgb_max_abs"], 0)
        self.assertGreater(row["joint_point_b_over_a"]["rgb_max_abs"], 0)

    def test_changed_original_union_is_actually_rejected(self):
        arrays = dict(self.arrays)
        key = "separated-original_linear"
        arrays[key] = arrays[key].copy()
        y, x = np.argwhere(arrays[key][:, :, 3] > 0)[0]
        arrays[key][y, x, 0] += 1
        with self.assertRaisesRegex(ValueError, "union differs"):
            native_case("separated", self.manifest, arrays, self.parts, self.patch)

    def test_corrupt_native_capture_hash_is_rejected_before_geometry_loading(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-background-negative-"
        ) as directory:
            root = Path(directory)
            (root / "captures.npz").symlink_to(INPUTS / "captures.npz")
            manifest = self.manifest | {"npz_sha256": "0" * 64}
            (root / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "checksum mismatch"):
                read_inputs(root)

    def test_raw_camera_is_not_the_viewer_even_with_equal_raster_and_eye(self):
        result = capture_control(self.manifest, self.arrays)
        self.assertFalse(result["raw_and_viewer_samples_registered"])
        self.assertEqual(
            result["direct_raw_plus_viewer_background_admission"], "originals"
        )
        changed = copy.deepcopy(self.manifest)
        capture = changed["capture_camera"]
        changed["viewer"].update(
            orthographic=capture["orthographic"],
            width_px=capture["width_px"],
            height_px=capture["height_px"],
            eye=capture["eye_m"],
        )
        result = capture_control(changed, self.arrays)
        self.assertEqual(result["known_ray_grid_differences"], ["basis"])
        self.assertFalse(result["raw_and_viewer_samples_registered"])


if __name__ == "__main__":
    unittest.main()
