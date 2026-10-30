"""Capture calibration, fixed-coverage view control and bounded source registration."""

import copy
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from building_sample_inputs import readbacks
from building_sample_pbr import energy_tables, point_responses, source_points
from building_sample_source import read_source
from compare_building_sample_pbr import analyse
from compare_building_sample_reuse import DATA
from field_view_sampling import FieldGrid, ImageView


class SourcePbrControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tables, cls.table_manifest = energy_tables(DATA.parent / "material-filter")
        cls.source = read_source(DATA / "source-512", DATA / "high-sampling")
        cls.high = readbacks(DATA / "high-sampling")

    def test_source_centres_and_reverse_z_heights_have_independent_coordinates(self):
        grid = FieldGrid((10, 20, 30), (0, 0, 1), 6)
        depth = np.zeros((3, 3), np.float32)
        depth[0, 0], depth[2, 2] = 0.25, 0.75
        points = source_points(grid, depth, depth > 0)
        np.testing.assert_array_equal(points, [[6, 24, 24], [14, 16, 36]])

    def test_source_capacity_precedes_coordinate_expansion(self):
        with (
            patch(
                "building_sample_pbr.np.nonzero", side_effect=AssertionError("expanded")
            ),
            self.assertRaisesRegex(ValueError, "Bounded"),
        ):
            source_points(
                self.source[2], np.zeros((513, 513)), np.ones((513, 513), bool)
            )

    def test_invalid_depth_registration_and_zero_coverage_are_rejected(self):
        depth = np.full((3, 3), 0.5)
        for mask in (np.ones((3, 3), int), np.zeros((3, 3), bool)):
            with self.assertRaisesRegex(ValueError, "registered"):
                source_points(self.source[2], depth, mask)
        for value in (np.nan, -0.1, 1.1):
            invalid = depth.copy()
            invalid[0, 0] = value
            with self.assertRaisesRegex(ValueError, "registered"):
                source_points(self.source[2], invalid, invalid > 0)

    def test_existing_table_binding_rejects_modified_archive(self):
        actual = DATA.parent / "material-filter"
        with tempfile.TemporaryDirectory(prefix="outshine-source-pbr-") as temp:
            directory = Path(temp)
            for name in ("energy-tables.npz", "energy-tables.reference.json"):
                (directory / name).write_bytes((actual / name).read_bytes())
            with (directory / "energy-tables.npz").open("ab") as output:
                output.write(b"changed")
            with self.assertRaisesRegex(ValueError, "binding changed"):
                energy_tables(directory)

    def test_same_outgoing_direction_keeps_model_radiance_identical(self):
        grid = FieldGrid((0, 0, 0), (0, 2**-0.5, 2**-0.5), 1)
        depth = np.zeros((3, 3), np.float32)
        depth[1, 1] = 0.5
        normal = np.zeros((3, 3, 3), np.float32)
        normal[:, :, 1] = 1
        arrays = {
            "source_depth": depth,
            "source_normal": normal,
            "source_base_colour": np.full((3, 3, 3), 0.4),
            "source_roughness": np.full((3, 3), 0.72),
            "source_metalness": np.zeros((3, 3)),
        }
        right, up = grid.axes()
        view = ImageView(
            tuple(10 * np.asarray(grid.toward_eye)),
            (tuple(right), tuple(up), tuple(-np.asarray(grid.toward_eye))),
            720,
        )
        fields, work = point_responses(arrays, grid, view, self.tables)
        np.testing.assert_array_equal(fields["capture"], fields["perspective"])
        self.assertGreater(fields["capture"][1, 1, :3].sum(), 0)
        self.assertEqual(work["ggx_evaluations"], 2)

    def test_unsupported_material_does_not_become_a_dielectric_approximation(self):
        arrays = self.source[1].copy()
        for channel, value in (
            ("source_metalness", 0.5),
            ("source_roughness", -0.1),
            ("source_base_colour", np.nan),
        ):
            arrays[channel] = np.full_like(self.source[1][channel], value)
            with self.assertRaisesRegex(ValueError, "opaque dielectric"):
                point_responses(arrays, *self.source[2:], self.tables)
            arrays[channel] = self.source[1][channel]

    def test_zero_roughness_retains_the_existing_owned_endpoint(self):
        arrays = self.source[1].copy()
        arrays["source_roughness"] = np.zeros_like(arrays["source_roughness"])
        fields, work = point_responses(arrays, *self.source[2:], self.tables)
        self.assertEqual(work["source_points"], 43049)
        for field in fields.values():
            self.assertTrue(np.isfinite(field).all())
            self.assertTrue((field >= 0).all())
            np.testing.assert_array_equal(field[:, :, 3], arrays["source_depth"] > 0)

    def test_wrong_light_cannot_be_called_a_native_capture_positive_control(self):
        manifest = copy.deepcopy(self.source[0])
        manifest["query"]["sun_elevation_deg"] = 46
        with (
            patch(
                "compare_building_sample_pbr.point_responses",
                side_effect=AssertionError("shaded"),
            ),
            self.assertRaisesRegex(ValueError, "Same native"),
        ):
            analyse((manifest, *self.source[1:]), self.high, self.tables)

    def test_all_actual_source_points_are_kept_and_native_arrays_are_unmodified(self):
        original = {k: v.copy() for k, v in self.source[1].items()}
        fields, work = point_responses(self.source[1], *self.source[2:], self.tables)
        self.assertEqual(work["source_points"], 43049)
        self.assertEqual(work["ggx_evaluations"], 86098)
        mask = self.source[1]["source_depth"] > 0
        for field in fields.values():
            np.testing.assert_array_equal(field[:, :, 3], mask)
            np.testing.assert_array_equal(field[~mask], 0)
        for key, value in original.items():
            np.testing.assert_array_equal(value, self.source[1][key])

    def test_actual_capture_matches_and_view_effect_is_small_only_in_this_model(self):
        report, _images, _fields = analyse(self.source, self.high, self.tables)
        self.assertLess(
            report["source_native_vs_capture_model"]["rgb_relative_l1"], 0.0005
        )
        self.assertGreater(
            report["source_capture_vs_perspective_model"]["rgb_relative_l1"], 0.0005
        )
        self.assertEqual(report["direct_source_block_axis"], 16)
        self.assertLess(report["direct_block_maximum_rgba_error"], 1e-10)
        self.assertLessEqual(
            report["model_view_response_original_relative_l1_actual"],
            report["model_view_response_original_relative_l1_bound"] + 1e-10,
        )
        self.assertLess(report["model_view_response_original_relative_l1_bound"], 0.002)
        self.assertEqual(report["model_view_response_alpha_maximum_delta"], 0)
        self.assertGreater(
            report["original_vs_model_perspective"]["rgb_relative_l1"], 0.05
        )
        self.assertGreater(
            report["original_vs_model_perspective"]["alpha_relative_l1"], 0.03
        )


if __name__ == "__main__":
    unittest.main()
