"""Independent cone interpolation, positive compression and coupled shadow controls."""

import unittest
from unittest.mock import patch

import numpy as np
from joint_light_measure import JointMeasure
from joint_basis_reduction import fit_report
from joint_basis_study import view_light_knots
from needle_area_checks import sheet
from needle_area_light import integrate
from positive_light_basis import angular_weights, positive_columns
from scatter_scene import ScatterScene


class PositiveBasisTests(unittest.TestCase):
    def test_octahedral_cone_coefficients_preserve_linear_light_and_knots(self):
        knots = np.r_[np.eye(3), -np.eye(3)]
        queries = np.array([[1.0, 2, 3], [-2.0, 1, -3]])
        queries /= np.linalg.norm(queries, axis=1)[:, None]
        weights = angular_weights(knots, queries)
        self.assertTrue((weights >= 0).all())
        np.testing.assert_allclose(knots.T @ weights, queries.T, atol=1e-14, rtol=0)
        np.testing.assert_allclose(weights[:3, 0], queries[0], atol=1e-14)
        self.assertGreater(weights[:, 0].sum(), 1)
        np.testing.assert_allclose(angular_weights(knots, knots), np.eye(6), atol=1e-14)
        self.assertAlmostEqual(
            float(np.maximum(knots[:, 2], 0) @ weights[:, 0]), queries[0, 2]
        )

    def test_rigid_rotation_preserves_coefficients_and_invalid_cones_refuse(self):
        knots = np.r_[np.eye(3), -np.eye(3)]
        q = np.array([[1.0, 1, 1]]) / np.sqrt(3)
        rotation = np.array([[0.0, 0, 1], [0, 1, 0], [-1, 0, 0]])
        np.testing.assert_allclose(
            angular_weights(knots @ rotation.T, q @ rotation.T),
            angular_weights(knots, q),
            atol=1e-14,
        )
        with self.assertRaises(ValueError):
            angular_weights(np.tile([0.0, 0, 1], (6, 1)), q)
        hemisphere = knots.copy()
        hemisphere[:, 2] += 2
        hemisphere /= np.linalg.norm(hemisphere, axis=1)[:, None]
        with self.assertRaises(ValueError):
            angular_weights(hemisphere, q)
        with self.assertRaises(ValueError):
            angular_weights(knots, [[2, 0, 0]])

    def test_actual_positive_columns_reconstruct_rank_two_with_gaps_and_owned_outputs(
        self,
    ):
        basis = np.array([[1.0, 0], [0, 1], [0.3, 0.6], [0, 0]])
        coefficients = np.array([[1.0, 0, 0.3, 0.9], [0, 1, 0.8, 0.2]])
        matrix = basis @ coefficients
        before = matrix.copy()
        reduced = positive_columns(matrix, 2)
        np.testing.assert_allclose(
            reduced["basis"] @ reduced["weights"], matrix, atol=1e-14, rtol=0
        )
        np.testing.assert_array_equal(matrix, before)
        np.testing.assert_array_equal(reduced["basis"], matrix[:, reduced["indices"]])
        self.assertTrue((reduced["weights"] >= 0).all())
        self.assertFalse(np.shares_memory(reduced["basis"], matrix))
        np.testing.assert_array_equal(reduced["basis"][3], 0)

    def test_zero_and_invalid_requests_do_not_start_solves(self):
        with patch("positive_light_basis.nnls", side_effect=AssertionError("solve")):
            result = positive_columns(np.zeros((4, 6)), 4)
            self.assertEqual(result["basis"].shape, (4, 0))
            self.assertEqual(result["weights"].shape, (0, 6))
            for matrix, rank in [
                (np.ones((4, 6)), 0),
                (-np.ones((4, 6)), 1),
                (np.ones((4, 6)), 7),
            ]:
                with self.assertRaises(ValueError):
                    positive_columns(matrix, rank)

    def test_both_aligned_grids_enclose_origin_and_reconstruct_independent_queries(
        self,
    ):
        for count in (32, 64):
            knots = view_light_knots(count)
            self.assertEqual(knots.shape, (count, 3))
            queries = np.array([[1.0, 2, -3], [-2, 3, 1]]) / np.sqrt(14)
            weights = angular_weights(knots, queries)
            np.testing.assert_allclose(knots.T @ weights, queries.T, atol=1e-14)

    def test_final_interval_projection_is_nonexpansive_before_and_after_filtering(self):
        mass = np.array([0.0, 0.2, 0.7, 1.0])[:, None]
        truth = mass * np.array([[0.0, 0.6, 1.0]])
        raw = truth + np.array(
            [[-0.4, 0.3, 0.5], [-0.2, 0.4, 0.7], [0.1, -0.6, 0.7], [-0.3, 0.5, -0.9]]
        )
        for kernel in (np.eye(4), np.array([[0.1, 0.2, 0.3, 0.4]])):
            reference, upper, prediction = kernel @ truth, kernel @ mass, kernel @ raw
            clipped = np.clip(prediction, 0, upper)
            self.assertTrue(
                (abs(clipped - reference) <= abs(prediction - reference) + 1e-15).all()
            )
        bounded, report = fit_report(raw, np.eye(3), truth, truth, [np.eye(3)], mass)
        np.testing.assert_array_equal(bounded, np.clip(raw, 0, mass))
        self.assertGreater(report["heldout_raw"]["negative_values"], 0)
        self.assertEqual(report["heldout"]["negative_values"], 0)


class JointFieldTests(unittest.TestCase):
    def test_integrator_hook_matches_existing_fields_without_changing_outputs(self):
        source = sheet()
        view = np.array([0.0, 0, 1])
        lights = np.array([[0.0, 0, 1], [1, 0, 1]])
        lights /= np.linalg.norm(lights, axis=1)[:, None]
        base, _ = integrate(
            source,
            ScatterScene(source["vertices"], source["faces"]),
            view,
            lights,
            size=4,
            order=2,
        )
        scene = ScatterScene(source["vertices"], source["faces"])
        measure = JointMeasure(source, scene, view, lights, size=4)
        actual, _ = integrate(
            source, scene, view, lights, size=4, order=2, contact_measure=measure
        )
        np.testing.assert_array_equal(actual, base)
        np.testing.assert_array_equal(measure.coverage, actual[0].ravel())
        for index in range(2):
            np.testing.assert_array_equal(
                measure.unshadowed[index], actual[1 + 2 * index].ravel()
            )
            np.testing.assert_array_equal(
                measure.joint[index], actual[2 + 2 * index].ravel()
            )

    def test_real_blocker_proves_visibility_and_cosine_marginals_are_not_joint(self):
        source = sheet()
        source["vertices"] = np.r_[
            source["vertices"],
            [[0.5, -0.5, -1], [0.5, 0, -1], [0.5, 0, 1], [0.5, -0.5, 1]],
        ]
        source["faces"] = np.r_[source["faces"], [[4, 5, 6], [4, 6, 7]]]
        scene = ScatterScene(source["vertices"], source["faces"])
        measure = JointMeasure(
            source, scene, np.array([0.0, 0, 1]), [[1.0, 0, 0]], size=1
        )
        measure.add(
            np.array([[0, -0.25, 0], [0, 0.25, 0]]),
            np.array([0, 0]),
            np.array([0.5, 0.5]),
            np.array([[1.0, 0, 0], [0.2, 0, np.sqrt(0.96)]]),
        )
        self.assertAlmostEqual(measure.unshadowed[0, 0], 0.6)
        self.assertAlmostEqual(measure.joint[0, 0], 0.1)
        self.assertAlmostEqual(measure.independent()[0, 0], 0.3)
        self.assertEqual(scene.work["rays"], 2)

    def test_wrong_owner_or_work_budget_refuses_before_rays_and_zero_mass_skips(self):
        source = sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        measure = JointMeasure(
            source, scene, np.array([0.0, 0, 1]), np.eye(3)[:2], size=1, max_work=2
        )
        with self.assertRaises(ValueError):
            measure.add(
                np.zeros((2, 3)),
                np.zeros(2, int),
                np.ones(2) / 2,
                np.tile([0.0, 0, 1], (2, 1)),
            )
        self.assertEqual(scene.work["rays"], 0)
        measure.add(
            np.zeros((2, 3)),
            np.zeros(2, int),
            np.zeros(2),
            np.tile([0.0, 0, 1], (2, 1)),
        )
        self.assertEqual(scene.work["rays"], 0)
        self.assertEqual(measure.points, 0)
        with self.assertRaises(ValueError):
            integrate(
                source,
                scene,
                np.array([0.0, 1, 0]),
                [[0, 0, 1]],
                size=1,
                contact_measure=measure,
            )
        self.assertEqual(scene.work["rays"], 0)


if __name__ == "__main__":
    unittest.main()
