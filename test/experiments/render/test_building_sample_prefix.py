"""Real native operands and an independent impulse response of repeated filtering."""

import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from building_sample_inputs import digest, readbacks
from building_sample_source import read_source
from compare_building_sample_prefix import (
    analyse,
    filtered_image,
    integrated_image,
    jittered_view,
)
from compare_building_sample_reuse import DATA
from field_view_integrals import summed_field
from field_view_sampling import FieldGrid, ImageView


class PrefixFilterControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = read_source(DATA / "source-512", DATA / "high-sampling")
        cls.high = readbacks(DATA / "high-sampling")

    def test_actual_source_radiance_and_native_opponents_are_bound(self):
        manifest, arrays, _grid, view = self.source
        self.assertEqual(manifest["covered_source_samples"], 43049)
        self.assertEqual(view.width, 128)
        self.assertEqual(view.height, 96)
        self.assertEqual(view.focal_px, 720)
        self.assertEqual(arrays["source_lit_rgba"].nbytes, 4194304)
        self.assertEqual(manifest["query"]["filter_axis_samples"], 64)

    def test_native_full_frame_hash_rejects_rebound_crop_pixels(self):
        manifest = copy.deepcopy(self.source[0])
        arrays = {k: v.copy() for k, v in self.source[1].items()}
        arrays["prefix_linear"][20, 30, 0] += 1
        manifest["arrays"]["prefix_linear"]["sha256"] = hashlib.sha256(
            arrays["prefix_linear"].tobytes()
        ).hexdigest()
        with tempfile.TemporaryDirectory(prefix="outshine-prefix-filter-") as temp:
            directory = Path(temp) / "source-512"
            directory.mkdir()
            np.savez_compressed(directory / "samples.npz", **arrays)
            manifest.update(
                npz_bytes=(directory / "samples.npz").stat().st_size,
                npz_sha256=digest(directory / "samples.npz"),
            )
            (directory / "manifest.json").write_text(json.dumps(manifest))
            (Path(temp) / "original-geometry").symlink_to(
                DATA / "original-geometry", target_is_directory=True
            )
            with self.assertRaisesRegex(ValueError, "Full native"):
                read_source(directory, DATA / "high-sampling")

    def test_source_cannot_be_mixed_with_another_original_query(self):
        with tempfile.TemporaryDirectory(prefix="outshine-prefix-query-") as temp:
            directory = Path(temp)
            manifest = copy.deepcopy(self.high[0])
            manifest["cases"][0]["query"]["eye"][0] += 1
            (directory / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "binding"):
                read_source(DATA / "source-512", directory)

    def test_existing_integral_and_native_normal_abruf_agree_without_ss_blur(self):
        report, _ = analyse(self.source, self.high, [16])
        self.assertEqual(report["independent_source_points_per_pixel_axis"], 16)
        self.assertLess(report["single_vs_independent_block_mean_maximum_rgba"], 1e-10)
        self.assertLess(
            report["native_single_vs_python_single"]["rgb_relative_l1"], 0.0005
        )
        self.assertLess(report["original_vs_native_single"]["rgb_relative_l1"], 0.06)
        self.assertGreater(report["original_vs_native_ss64"]["rgb_relative_l1"], 0.14)
        jitter = report["double_filter_controls"][0]
        self.assertLess(
            jitter["native_ss64_vs_python_double_filtered"]["rgb_relative_l1"], 0.0005
        )
        self.assertLess(jitter["maximum_rectangle_excess_ratio"], 4e-5)

    def test_double_filter_impulse_matches_independent_separable_triangle(self):
        grid = FieldGrid((0.5, -0.5, 0), (0, 0, 1), 0.5)
        view = ImageView((0, 0, 1e6), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 1e6, 8, 8)
        field = summed_field(np.ones((1, 1, 4)), dtype=np.float64)
        single, _ = integrated_image(field, grid, view)
        self.assertAlmostEqual(single[4, 4, 3], 1)
        double, _ = filtered_image(field, grid, view, 4)
        expected = np.zeros((8, 8))
        expected[3:6, 3:6] = np.outer([0.125, 0.75, 0.125], [0.125, 0.75, 0.125])
        np.testing.assert_allclose(double[:, :, 3], expected, rtol=0, atol=1e-9)
        self.assertLess(double[4, 4, 3], single[4, 4, 3])

    def test_one_jitter_is_identical_and_larger_axes_are_bounded_before_work(self):
        _, arrays, grid, view = self.source
        field = summed_field(arrays["source_lit_rgba"], dtype=np.float64)
        single, _ = integrated_image(field, grid, view)
        double, _ = filtered_image(field, grid, view, 1)
        np.testing.assert_allclose(single, double, rtol=0, atol=1e-10)
        with patch(
            "compare_building_sample_prefix.integrated_image",
            side_effect=AssertionError("work"),
        ):
            for axis in (0, 65, 2.5, True):
                with self.assertRaisesRegex(ValueError, "bounded"):
                    filtered_image(field, grid, view, axis)

    def test_native_camera_rotation_retains_an_orthonormal_basis(self):
        view = self.source[3]
        rotated = jittered_view(view, 0.49, -0.49)
        np.testing.assert_allclose(
            np.asarray(rotated.basis) @ np.asarray(rotated.basis).T,
            np.eye(3),
            atol=1e-12,
        )
        self.assertEqual(rotated.eye, view.eye)
        self.assertEqual(rotated.focal_px, view.focal_px)


if __name__ == "__main__":
    unittest.main()
