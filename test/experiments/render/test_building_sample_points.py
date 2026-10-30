"""Geometry-only correspondence, explicit uncertainty and native finite responses."""

import copy
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from building_sample_inputs import capture_fields, readbacks
from building_sample_points import (
    analyse_points,
    capture_channels,
    match_cells,
    summarize,
)
from field_view_sampling import FieldGrid, ImageView

DATA = (
    Path(__file__).resolve().parents[2] / "outshine/data/render/building-depth-samples"
)


def flat():
    grid = FieldGrid((0, 0, 0), (0, 0, 1), 2)
    view = ImageView((0, 0, 10), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 10, 4, 4)
    mask = np.ones((4, 4), bool)
    source_depth = np.full((4, 4), 0.5, np.float32)
    source_normal = np.zeros((4, 4, 3), np.float32)
    source_normal[:, :, 2] = 1
    depth = np.full((4, 4), float(np.float32(0.1)) / 10, np.float32)
    normal = np.ones((4, 4, 4), np.float32)
    normal[:, :, :3] = source_normal
    return grid, view, mask, source_depth, source_normal, depth, normal


class PointCorrespondence(unittest.TestCase):
    def test_same_plane_centres_recover_original_cells(self):
        result = match_cells(*flat())
        y, x = np.indices((4, 4))
        np.testing.assert_array_equal(result["pixels_xy"], np.c_[x.ravel(), y.ravel()])
        np.testing.assert_array_equal(result["source_xy"], result["pixels_xy"])
        self.assertTrue(result["unique_geometry"].all())
        self.assertTrue(result["interior_texture"].all())
        self.assertLess(float(result["world_height_residual_m"].max()), 1e-5)

    def test_shared_cell_edges_remain_ambiguous_without_rgb_tie_breaking(self):
        args = list(flat())
        old = args[1]
        args[1] = ImageView((0.5, 0, 10), old.basis, old.focal_px, 4, 4)
        result = match_cells(*args)
        self.assertGreater(int((result["candidate_counts"] > 1).sum()), 0)
        self.assertFalse(result["interior_texture"].any())

    def test_inconsistent_depth_never_invents_a_matched_source(self):
        args = list(flat())
        args[3][1, 1] = 0.6
        result = match_cells(*args)
        index = np.flatnonzero((result["pixels_xy"] == [1, 1]).all(1))[0]
        self.assertEqual(result["candidate_counts"][index], 0)
        self.assertFalse(result["unique_geometry"][index])

    def test_changed_shading_normal_is_not_a_same_normal_pbr_probe(self):
        args = list(flat())
        args[4][1, 1] = [1, 0, 0]
        result = match_cells(*args)
        index = np.flatnonzero((result["pixels_xy"] == [1, 1]).all(1))[0]
        self.assertEqual(result["candidate_counts"][index], 0)

    def test_capacity_precedes_ray_and_neighbour_arrays(self):
        with (
            patch(
                "building_sample_points.np.column_stack",
                side_effect=AssertionError("allocation"),
            ),
            self.assertRaisesRegex(ValueError, "Bounded"),
        ):
            match_cells(*flat(), points_most=15)

    def test_nonfinite_or_undeclared_roundoff_controls_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "engineering-roundoff"):
            match_cells(*flat(), operations=17)
        args = list(flat())
        args[-1][0, 0, 0] = np.nan
        with self.assertRaisesRegex(ValueError, "normal"):
            match_cells(*args)

    def test_black_capture_keeps_the_cohort_and_reports_absolute_response(self):
        mapping = match_cells(*flat())
        source = np.zeros((4, 4, 4), np.float32)
        source[:, :, 3] = 1
        query = np.ones_like(source)
        result = summarize(source, query, mapping)["interior_same_cell_response"]
        self.assertEqual(result["points"], 16)
        self.assertIsNone(result["rgb_relative_l1"])
        self.assertEqual(result["rgb_absolute_l1"], 48)


class NativePointResponses(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.primary = readbacks(DATA)
        cls.canonical = readbacks(DATA / "same-rays")
        cls.grid, cls.fields = capture_fields(*cls.canonical)

    def test_capture_depth_mask_disagreement_is_not_free_or_valid(self):
        mask = self.fields["samples"][:, :, 3] > 0
        mask = mask.copy()
        mask[mask] = False
        with self.assertRaisesRegex(ValueError, "point mask"):
            capture_channels(*self.canonical, len(mask), mask)

    def test_actual_finite_native_cohorts_keep_every_ambiguous_point_explicit(self):
        rows, _maps = analyse_points(
            self.primary, self.canonical, self.grid, self.fields["samples"]
        )
        self.assertEqual(len(rows), 4)
        for row in rows:
            self.assertEqual(
                row["unique_geometry_matches"]
                + row["ambiguous_geometry_matches"]
                + row["unmatched_geometry"]
                + row["unresolved_numeric_precision"],
                row["native_base_sample_hits"],
            )
            self.assertGreater(row["quarter_cell_interior_points"], 0)
            self.assertLess(
                row["quarter_cell_interior_points"], row["native_base_sample_hits"]
            )
            for probe in row["roundoff_sensitivity"]:
                self.assertEqual(probe["changed_source_cells_on_common_matches"], 0)
        smallest = next(row for row in rows if row["query"]["name"] == "px32-yaw0")
        self.assertEqual(smallest["native_base_sample_hits"], 167)
        self.assertEqual(smallest["quarter_cell_interior_points"], 2)
        self.assertGreater(smallest["ambiguous_geometry_matches"], 0)

    def test_rgb_changes_cannot_select_an_easier_correspondence_cohort(self):
        rows, before = analyse_points(
            self.primary, self.canonical, self.grid, self.fields["samples"]
        )
        changed = copy.copy(self.primary[1])
        name = "case0_samples_linear"
        changed[name] = changed[name].copy()
        changed[name][:, :, :3] *= 3
        updated, after = analyse_points(
            (self.primary[0], changed),
            self.canonical,
            self.grid,
            self.fields["samples"],
        )
        self.assertEqual(set(before), set(after))
        for key in before:
            np.testing.assert_array_equal(before[key], after[key])
        self.assertNotEqual(
            rows[0]["interior_same_cell_response"],
            updated[0]["interior_same_cell_response"],
        )


if __name__ == "__main__":
    unittest.main()
