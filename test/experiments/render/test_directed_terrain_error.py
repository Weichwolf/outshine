"""Projective decomposition controls and a scene-depth counterexample to point-only admission."""

import unittest

import numpy as np
from directed_terrain_error import assess, control, split_projection
from terrain_projection_bound import spectral_bound


class DirectedTerrainError(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report, cls.images, cls.depths, cls.masks = assess()

    def test_reference_keeps_the_actual_resting_object_that_candidate_erases(self):
        report = self.report
        self.assertEqual(report["old_selected_stride"], 1)
        self.assertEqual(report["directed_selected_stride"], 8)
        self.assertGreater(report["reference_object_pixels"], 0)
        self.assertEqual(report["candidate_object_pixels"], 0)
        self.assertEqual(
            report["lost_object_pixels"], report["reference_object_pixels"]
        )
        self.assertEqual(
            report["changed_material_pixels"], report["lost_object_pixels"]
        )
        lost = self.masks["reference"] & ~self.masks["candidate"]
        self.assertTrue(
            np.all(self.depths["candidate"][lost] > self.depths["reference"][lost])
        )
        self.assertEqual(report["unchanged_outer_triangles"], 64)
        self.assertEqual(report["unchanged_object_triangles"], 12)

    def test_all_common_refinement_point_errors_obey_the_directed_bound_and_half_pixel(
        self,
    ):
        for level in self.report["levels"]:
            self.assertLessEqual(
                level["measured_common_refinement_point_error_px"],
                level["directed_pixel_bound"] + 1e-12,
            )
        coarsest = self.report["levels"][-1]
        self.assertLess(coarsest["directed_pixel_bound"], 0.5)
        self.assertGreater(coarsest["isotropic_pixel_bound"], 0.5)
        self.assertLess(coarsest["xy_residual_m"], 2e-15)
        self.assertGreater(coarsest["isotropic_rounding_m"], 0)

    def test_object_footprint_and_bottom_are_inside_the_actual_flat_source_valley(self):
        vertices, top, _, object_faces, _ = control()
        obj = vertices[np.unique(object_faces)]
        self.assertTrue(np.all((obj[:, 0] > 2) & (obj[:, 0] < 3)))
        self.assertTrue(np.all((obj[:, 1] > -3) & (obj[:, 1] < -2)))
        valley = top.reshape(9, 9, 3)[2:4, 2:4, 2]
        np.testing.assert_array_equal(valley, -1 / 32)
        self.assertEqual(obj[:, 2].min(), -1 / 32)
        self.assertEqual(obj[:, 2].max(), -1 / 64)

    def test_nonunit_axis_and_isotropic_rest_enclose_independent_perspective_samples(
        self,
    ):
        rng = np.random.default_rng(1243)
        rows = np.array([[1.2, 0.15, 0], [-0.2, 1.7, 0.1], [0.05, 0.03, 1.0]])
        axis = np.array([0.1, 0.2, 2.0])
        points = rng.uniform([-2, -2, 10], [2, 2, 20], size=(2048, 3))
        original = points @ rows.T
        ndc = original[:, :2] / original[:, 2, None]
        low, high = ndc.min(axis=0)[None], ndc.max(axis=0)[None]
        viewport = np.array([1280.0, 720.0])
        norm = rng.normal(size=points.shape)
        norm /= np.linalg.norm(norm, axis=1)[:, None]
        delta = rng.uniform(-0.25, 0.25, size=(len(points), 1)) * axis + 0.01 * norm
        moved = (points + delta) @ rows.T
        difference = ((moved[:, :2] / moved[:, 2, None]) - ndc) * (0.5 * viewport)
        bound = split_projection(
            np.array([[0.25]]),
            np.array([[0.01]]),
            np.array([original[:, 2].min()]),
            rows,
            low,
            high,
            viewport,
            axis,
        ).item()
        self.assertLessEqual(np.linalg.norm(difference, axis=1).max(), bound)
        euclidean = 0.25 * np.linalg.norm(axis) + 0.01
        isotropic = (
            euclidean
            * spectral_bound(rows, low, high, viewport)[0]
            / (original[:, 2].min() - euclidean * np.linalg.norm(rows[2]))
        )
        self.assertLessEqual(bound, isotropic)

    def test_nonpositive_denominator_and_unknown_or_negative_residual_stay_unqualified(
        self,
    ):
        rows = np.eye(3)
        low, high, viewport, axis = (
            np.array([[-1.0, -1.0]]),
            np.array([[1.0, 1.0]]),
            np.array([1280.0, 720.0]),
            np.array([0.0, 0.0, 1.0]),
        )
        for height, rest, w in (
            (1.0, 0.0, 1.0),
            (np.inf, 0.0, 2.0),
            (-1.0, 0.0, 2.0),
            (0.0, -0.1, 2.0),
            (0.0, 0.0, -1.0),
            (0.0, 0.0, np.inf),
            (0.0, 0.0, np.nan),
        ):
            bound = split_projection(
                np.array([[height]]),
                np.array([[rest]]),
                np.array([w]),
                rows,
                low,
                high,
                viewport,
                axis,
            )
            self.assertTrue(np.isinf(bound).all())


if __name__ == "__main__":
    unittest.main()
