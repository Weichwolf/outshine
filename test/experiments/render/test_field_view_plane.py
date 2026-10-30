"""Independent plane coordinates, explicit approximation and real-source fit."""

import unittest
from unittest.mock import patch

import numpy as np
from building_sample_inputs import readbacks
from building_sample_source import read_source
from compare_building_sample_reuse import DATA, errors
from field_view_integrals import summed_field
from field_view_plane import FieldPlane, fit_plane
from field_view_sampling import FieldGrid, ImageView, reconstruct, source_corners


class PlaneTests(unittest.TestCase):
    def setUp(self):
        self.grid = FieldGrid((0, 0, 0), (0, 0, 1), 1)
        self.view = ImageView((0, 0, 10), ((1, 0, 0), (0, 1, 0), (0, 0, -1)), 20, 8, 8)
        self.field = summed_field(np.ones((1, 1, 4)), dtype=np.float64)

    def test_fit_recovers_known_affine_depth_and_keeps_all_points(self):
        y, x = np.mgrid[:5, :5]
        height = 0.2 * ((x + 0.5) / 5 * 2 - 1) - 0.3 * (1 - (y + 0.5) / 5 * 2) + 0.1
        depth = (height + 2) / 4
        plane, work = fit_plane(depth, np.ones((5, 5), bool))
        np.testing.assert_allclose(
            plane.coefficients, [0.2, -0.3, 0.1], rtol=0, atol=1e-14
        )
        self.assertEqual(work["source_points"], 25)
        self.assertEqual(work["rank"], 3)
        self.assertLess(work["normalized_depth_rms"], 1e-14)

    def test_valid_zero_depth_is_retained_when_occupied_mask_explicitly_says_so(self):
        plane, work = fit_plane(np.zeros((3, 3)), np.ones((3, 3), bool))
        self.assertEqual(work["source_points"], 9)
        np.testing.assert_allclose(plane.coefficients, [0, 0, -2], atol=1e-14)

    def test_curved_source_reports_large_residual_instead_of_certifying_a_plane(self):
        y, x = np.mgrid[:16, :16]
        u, v = (x + 0.5) / 16 * 2 - 1, 1 - (y + 0.5) / 16 * 2
        height = 0.5 * (u * u + v * v)
        _, work = fit_plane((height + 2) / 4, np.ones((16, 16), bool))
        self.assertGreater(work["normalized_depth_rms"], 0.1)
        self.assertGreater(work["normalized_depth_maximum_residual"], 0.3)

    def test_capture_frame_translation_rotation_and_scale_preserve_plane_coordinates(
        self,
    ):
        grid = FieldGrid((10000, 20000, 30000), (0.5, 2**-0.5, 0.5), 100)
        plane = FieldPlane((0.2, -0.3, 0.1))
        uv = np.array([[-0.7, 0.4], [0.2, -0.3]])
        points = plane.points(grid, uv)
        right, up = grid.axes()
        np.testing.assert_allclose(
            (points - grid.centre) @ right / grid.half_extent_m, uv[:, 0], atol=1e-13
        )
        np.testing.assert_allclose(
            (points - grid.centre) @ up / grid.half_extent_m, uv[:, 1], atol=1e-13
        )
        np.testing.assert_allclose(
            (points - plane.centre(grid)) @ plane.normal(grid), 0, atol=1e-11
        )

    def test_sparse_noncollinear_points_and_rank_deficiency_are_distinct(self):
        depth = np.full((5, 5), 0.6)
        mask = np.zeros((5, 5), bool)
        mask[0, 0] = mask[2, 2] = mask[4, 4] = True
        with self.assertRaisesRegex(ValueError, "identify"):
            fit_plane(depth, mask)
        mask[4, 4] = False
        mask[0, 4] = True
        plane, _ = fit_plane(depth, mask)
        np.testing.assert_allclose(plane.coefficients, [0, 0, 0.4], atol=1e-14)

    def test_input_capacity_precedes_fit_and_coordinate_expansion(self):
        with (
            patch(
                "field_view_plane.np.nonzero", side_effect=AssertionError("expanded")
            ),
            self.assertRaisesRegex(ValueError, "Bounded"),
        ):
            fit_plane(np.zeros((513, 513)), np.ones((513, 513), bool))
        for depth in (
            np.full((3, 3), np.nan),
            np.full((3, 3), 1.1),
            np.full((3, 4), 0.5),
        ):
            with self.assertRaisesRegex(ValueError, "Bounded"):
                fit_plane(depth, np.ones(depth.shape, bool))
        with self.assertRaisesRegex(ValueError, "three occupied"):
            fit_plane(np.full((3, 3), 0.5), np.zeros((3, 3), bool))

    def test_world_plane_does_not_change_capture_coordinates(self):
        plane = FieldPlane((0.2, -0.3, 0.1))
        coords = np.array([[-1, 1], [1, -1], [0.3, -0.4]])
        points = plane.points(self.grid, coords)
        np.testing.assert_allclose(points[:, :2], coords, atol=1e-14)
        np.testing.assert_allclose(points[:, 2], coords @ [0.2, -0.3] + 0.1, atol=1e-14)
        np.testing.assert_allclose(
            (points - plane.centre(self.grid)) @ plane.normal(self.grid), 0, atol=1e-14
        )

    def test_inverse_ray_plane_coordinates_match_independent_forward_projection(self):
        plane = FieldPlane((0.2, -0.3, 0.1))
        coords = np.array([[0.3, -0.4]])
        point = plane.points(self.grid, coords)[0]
        pixel = np.array([4, 4]) + [
            20 * point[0] / (10 - point[2]),
            -20 * point[1] / (10 - point[2]),
        ]
        uv = source_corners(
            self.grid, self.view, self.grid.axes(), pixel[None], (64, 64), plane=plane
        )
        np.testing.assert_allclose(
            uv[0, 0], [(0.3 + 1) * 32, (1 + 0.4) * 32], rtol=0, atol=1e-12
        )

    def test_zero_plane_preserves_original_arrays_and_work(self):
        original = reconstruct(self.field, self.grid, self.view)
        explicit = reconstruct(self.field, self.grid, self.view, plane=FieldPlane())
        self.assertEqual(original.keys(), explicit.keys())
        for name in original:
            if name == "rgba":
                np.testing.assert_array_equal(original[name], explicit[name])
            else:
                self.assertEqual(original[name], explicit[name])

    def test_offset_parallel_plane_has_the_independent_projected_square_area(self):
        result = reconstruct(
            self.field, self.grid, self.view, plane=FieldPlane((0, 0, 0.2))
        )
        self.assertTrue(result["parallel"])
        self.assertAlmostEqual(
            result["rgba"][:, :, 3].sum(), (40 / 9.8) ** 2, places=10
        )

    def test_tilt_requires_explicit_approximation_and_reports_it(self):
        plane = FieldPlane((0.1, -0.2, 0))
        with self.assertRaisesRegex(ValueError, "explicit"):
            reconstruct(self.field, self.grid, self.view, plane=plane)
        result = reconstruct(
            self.field, self.grid, self.view, plane=plane, approximate=True
        )
        self.assertFalse(result["parallel"])
        self.assertGreater(result["maximum_rectangle_excess_ratio"], 0)

    def test_plane_crossing_eye_and_nonfinite_parameters_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "eye plane"):
            reconstruct(
                self.field,
                self.grid,
                self.view,
                plane=FieldPlane((20, 0, 0)),
                approximate=True,
            )
        with self.assertRaises(ValueError):
            FieldPlane((np.nan, 0, 0))

    def test_real_source_fit_improves_the_same_finite_original_without_refitting_it(
        self,
    ):
        _, arrays, grid, view = read_source(DATA / "source-512", DATA / "high-sampling")
        before = arrays["source_depth"].copy()
        plane, work = fit_plane(arrays["source_depth"], arrays["source_surface"] > 0)
        self.assertEqual(work["source_points"], 43049)
        self.assertLess(work["normalized_depth_rms"] * grid.half_extent_m, 6)
        table = summed_field(arrays["source_lit_rgba"], dtype=np.float64)
        results = []
        for support in (None, plane):
            sampled = reconstruct(table, grid, view, plane=support, approximate=True)
            image = np.zeros((view.height, view.width, 4))
            x, y = sampled["origin"]
            h, w = sampled["rgba"].shape[:2]
            image[y : y + h, x : x + w] = sampled["rgba"]
            results.append(image)
        original = readbacks(DATA / "high-sampling")[1]["case0_original_ss4096"]
        old, new = [errors(original, value) for value in results]
        self.assertLess(new["rgb_relative_l1"], old["rgb_relative_l1"])
        self.assertLess(new["alpha_relative_l1"], old["alpha_relative_l1"])
        self.assertLess(new["rgb_relative_l1"], 0.045)
        self.assertLess(new["alpha_relative_l1"], 0.02)
        np.testing.assert_array_equal(before, arrays["source_depth"])


if __name__ == "__main__":
    unittest.main()
