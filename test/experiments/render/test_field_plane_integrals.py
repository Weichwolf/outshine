"""Independent colour masses, opacity semantics and clipping capacities."""

import unittest
from unittest.mock import patch

import numpy as np
from field_plane_integrals import cell_integrals, mask_integrals
from field_view_integrals import summed_field
from field_view_plane import FieldPlane
from field_view_sampling import FieldGrid, ImageView, reconstruct


class PlaneAreaTests(unittest.TestCase):
    def setUp(self):
        self.grid = FieldGrid((0, 0, 0), (0, 0, 1), 1)
        self.view = ImageView((0, 0, 10), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 20, 8, 8)
        self.plane = FieldPlane()

    def test_single_hdr_translucent_cell_preserves_independent_projected_mass(self):
        field = np.array([[[4, 2, 1, 0.5]]], float)
        result, work = cell_integrals(field, self.grid, self.view, self.plane)
        np.testing.assert_allclose(result.sum((0, 1)), [64, 32, 16, 8], atol=1e-11)
        self.assertEqual(work["source_cells"], 1)
        self.assertGreater(work["clipped_cell_pixel_pairs"], 1)
        with self.assertRaisesRegex(ValueError, "Binary point"):
            mask_integrals(field, self.grid, self.view, self.plane)

    def test_registered_colour_cells_match_existing_prefix_on_parallel_plane(self):
        field = np.zeros((4, 4, 4))
        field[::2, ::2] = [5, 2, 1, 1]
        field[1::2, 1::2] = [0.1, 0.2, 0.3, 0.5]
        result, _ = cell_integrals(field, self.grid, self.view, self.plane)
        old = reconstruct(summed_field(field, dtype=np.float64), self.grid, self.view)
        expected = np.zeros_like(result)
        x, y = old["origin"]
        h, w = old["rgba"].shape[:2]
        expected[y : y + h, x : x + w] = old["rgba"]
        np.testing.assert_allclose(result, expected, rtol=0, atol=1e-11)

    def test_tilted_cell_alpha_matches_independent_run_union_with_holes(self):
        field = np.zeros((8, 8, 4))
        field[1:7, 1:7] = [2, 3, 4, 1]
        field[3:5, 3:5] = 0
        plane = FieldPlane((0.1, -0.2, 0.05))
        image, _ = cell_integrals(field, self.grid, self.view, plane)
        mask, _ = mask_integrals(field, self.grid, self.view, plane)
        np.testing.assert_allclose(image[:, :, 3], mask, rtol=0, atol=1e-11)
        np.testing.assert_allclose(
            image[:, :, :3], mask[:, :, None] * [2, 3, 4], rtol=0, atol=1e-11
        )

    def test_pair_capacity_precedes_geos_boundary_expansion(self):
        with (
            patch(
                "field_plane_integrals.shapely.polygons",
                side_effect=AssertionError("expanded"),
            ),
            self.assertRaisesRegex(ValueError, "before expansion"),
        ):
            cell_integrals(
                np.ones((1, 1, 4)), self.grid, self.view, self.plane, pairs_most=3
            )

    def test_empty_and_outside_fields_return_no_coverage_without_dropping_inputs(self):
        for grid, field in (
            (self.grid, np.zeros((3, 3, 4))),
            (FieldGrid((100, 0, 0), (0, 0, 1), 1), np.ones((3, 3, 4))),
        ):
            image, _ = cell_integrals(field, grid, self.view, self.plane)
            mask, _ = mask_integrals(field, grid, self.view, self.plane)
            self.assertEqual(image.sum(), 0)
            self.assertEqual(mask.sum(), 0)

    def test_invalid_premultiplication_and_output_capacity_are_declined_whole(self):
        field = np.zeros((3, 3, 4))
        field[0, 0, 0] = 1
        for call in (cell_integrals, mask_integrals):
            with self.assertRaisesRegex(ValueError, "premultiplied"):
                call(field, self.grid, self.view, self.plane)
        big = ImageView(self.view.eye, self.view.basis, 20, 256, 256)
        with self.assertRaisesRegex(ValueError, "Bounded"):
            cell_integrals(np.ones((3, 3, 4)), self.grid, big, self.plane)


if __name__ == "__main__":
    unittest.main()
