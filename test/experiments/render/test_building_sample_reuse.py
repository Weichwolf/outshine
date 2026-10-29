"""Actual frame/crop bindings and independent same-source integration controls."""

import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from building_sample_inputs import (
    capture_fields,
    digest,
    full_buffer_digest,
    readbacks,
    same_sources,
)
from compare_building_sample_reuse import (
    DATA,
    aligned_blocks,
    analyse,
    crop_view,
    occupied_support,
)
from field_view_sampling import FieldGrid, ImageView


class NativeSampleReuse(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.primary = readbacks(DATA)
        cls.canonical = readbacks(DATA / "same-rays")
        cls.high = readbacks(DATA / "high-sampling")
        cls.grid, cls.fields = capture_fields(*cls.canonical)

    def test_streaming_full_checksum_retains_crop_clear_and_float_bits(self):
        crop, raster = np.array([2, 1, 5, 3]), np.array([7, 5])
        clear = [-0.0, 0.25, 0.0, 0.0]
        values = np.arange(24, dtype="<f4").reshape(2, 3, 4)
        full = np.empty((5, 7, 4), "<f4")
        full[:] = clear
        full[1:3, 2:5] = values
        self.assertEqual(
            full_buffer_digest(values, clear, raster, crop),
            hashlib.sha256(full.tobytes()).hexdigest(),
        )
        self.assertNotEqual(
            full_buffer_digest(values, [0, 0.25, 0, 0], raster, crop),
            hashlib.sha256(full.tobytes()).hexdigest(),
        )

    def test_modified_pixels_cannot_be_rebound_only_by_changing_npz_checksum(self):
        manifest, arrays = self.canonical
        with tempfile.TemporaryDirectory(prefix="outshine-sample-reuse-") as temporary:
            target = Path(temporary)
            modified = {k: v.copy() for k, v in arrays.items()}
            modified["case0_original_linear"][0, 0, 0] += 1
            np.savez_compressed(target / "captures.npz", **modified)
            manifest = copy.deepcopy(manifest)
            manifest.update(
                npz_bytes=(target / "captures.npz").stat().st_size,
                npz_sha256=digest(target / "captures.npz"),
            )
            (target / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "full native buffer checksum"):
                readbacks(target)

    def test_actual_capture_preserves_points_and_source_inputs(self):
        manifest, arrays = self.canonical
        original = arrays["case0_samples_linear"].copy()
        self.assertEqual(self.fields["samples"].shape, (128, 128, 4))
        self.assertEqual(int(self.fields["samples"][:, :, 3].sum()), 2687)
        np.testing.assert_array_equal(self.fields["samples"][16:112], original)
        np.testing.assert_array_equal(arrays["case0_samples_linear"], original)
        self.assertEqual(manifest["raster"], [1280, 720])

    def test_actual_capture_mask_matches_original_without_area_filtering(self):
        np.testing.assert_array_equal(
            self.fields["original"][:, :, 3], self.fields["samples"][:, :, 3]
        )
        self.assertEqual(set(np.unique(self.fields["samples"][:, :, 3])), {0, 1})

    def test_native_pitch_and_clip_changes_cannot_silently_move_the_source(self):
        for key in ("far_m", "x_mag_m", "y_mag_m"):
            manifest = copy.deepcopy(self.canonical[0])
            manifest["cases"][0]["query"]["capture"][key] += 0.01
            with (
                self.subTest(key=key),
                self.assertRaisesRegex(ValueError, "registration"),
            ):
                capture_fields(manifest, self.canonical[1])

    def test_canonical_source_capacity_is_checked_before_field_allocation(self):
        manifest = copy.deepcopy(self.canonical[0])
        manifest["cases"][0]["draws"]["samples"]["image_storage_bytes"] = 32 * 1024**2
        with (
            patch(
                "building_sample_inputs.np.zeros",
                side_effect=AssertionError("allocation"),
            ),
            self.assertRaisesRegex(ValueError, "bounded"),
        ):
            capture_fields(manifest, self.canonical[1])

    def test_original_capture_driver_light_and_frame_bindings_remain_distinct(self):
        for key in (
            "atlas_sha256",
            "original_geometry_files",
            "bounds",
            "lighting",
            "native_driver_sha256",
        ):
            changed = copy.deepcopy(self.primary[0])
            changed[key] = "changed"
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, key):
                same_sources(self.primary[0], changed)

    def test_native_crop_preserves_full_frame_pixel_rays_and_focal_length(self):
        manifest = self.primary[0]
        low = np.array(manifest["crop"][:2])
        for case in manifest["cases"]:
            query = case["query"]
            view = crop_view(manifest, query)
            points = np.array([[0.5, 0.5], [127.5, 95.5], [64.5, 48.5]])
            local = (points - [view.width / 2, view.height / 2]) / view.focal_px
            full = (points + low - [640, 360]) / query["focal_px"]
            np.testing.assert_array_equal(local, full)

    def test_noncentred_crop_is_not_a_new_camera(self):
        manifest = copy.deepcopy(self.primary[0])
        manifest["crop"][0] -= 1
        manifest["crop"][2] -= 1
        with self.assertRaisesRegex(ValueError, "unchanged rays"):
            crop_view(manifest, manifest["cases"][0]["query"])

    def test_candidate_mass_outside_reference_crop_is_never_omitted(self):
        query = self.primary[0]["cases"][0]["query"]
        view = ImageView(
            tuple(query["eye"]),
            tuple(map(tuple, query["basis"])),
            query["focal_px"],
            32,
            24,
        )
        with self.assertRaisesRegex(ValueError, "omit occupied"):
            occupied_support(self.fields["samples"], self.grid, view)

    def test_independent_block_means_preserve_hdr_and_transparent_cells(self):
        field = np.zeros((8, 8, 4), float)
        field[::2, ::2] = [100, 2, 3, 1]
        grid = FieldGrid((0, 0, 0), (0, 0, 1), 1)
        view = ImageView((0, 0, 8), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 16, 8, 6)
        actual, stride = aligned_blocks(field, grid, view)
        self.assertEqual(stride, 2)
        expected = np.zeros((6, 8, 4))
        expected[1:5, 2:6] = [25, 0.5, 0.75, 0.25]
        np.testing.assert_array_equal(actual, expected)

    def test_nonparallel_or_shifted_views_do_not_claim_exact_block_alignment(self):
        query = self.primary[0]["cases"][-1]["query"]
        self.assertIsNone(
            aligned_blocks(
                self.fields["samples"], self.grid, crop_view(self.primary[0], query)
            )
        )
        query = copy.deepcopy(self.primary[0]["cases"][2]["query"])
        query["eye"][0] += 0.1
        self.assertIsNone(
            aligned_blocks(
                self.fields["samples"], self.grid, crop_view(self.primary[0], query)
            )
        )

    def test_actual_prefix_matches_direct_points_without_admitting_source_quality(self):
        result, _images = analyse(self.primary, self.canonical, self.high)
        self.assertEqual(result["canonical_coverage_mask_mismatches"], 0)
        self.assertEqual(result["reference_sample_axes"], [4, 64])
        self.assertEqual(len(result["rows"]), 5)
        for row in result["rows"]:
            direct = row["direct_same_source_blocks"]
            if direct["available"]:
                self.assertLess(direct["maximum_rgba_absolute_error"], 1e-10)
        high = result["rows"][-1]
        self.assertEqual(high["query"]["name"], "px32-yaw0")
        self.assertEqual(
            high["declared_whole_source_flattening_bound"]["reason"],
            "geometric_displacement_only",
        )
        self.assertLess(
            high["declared_whole_source_flattening_bound"]["screen_l1_upper_px"],
            0.23,
        )
        self.assertGreater(
            high["native_original_vs_stored_samples"]["rgb_relative_l1"], 0.20
        )
        self.assertGreater(
            high["native_original_vs_held_radiance_prefix"]["rgb_relative_l1"], 0.20
        )


if __name__ == "__main__":
    unittest.main()
