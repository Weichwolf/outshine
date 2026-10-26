"""Public weighted-fit ownership, analytic constraints and bounded failure cases."""

import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightBudget, HeightFit
from outshine_ref.transport.weighted_heights import fit_weighted_heights


class WeightedHeights(unittest.TestCase):
    def setUp(self):
        self.surface = Mesh([[0, 0, 0], [20, 0, 0.8], [0, 20, 0.8]], [[0, 1, 2]])
        self.frames = np.array([np.eye(2) / 0.025])

    def check(self, mesh, frames=None):
        p = mesh.vertices[mesh.triangles]
        n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        g = -n[:, :2] / n[:, 2, None]
        normalized = np.einsum(
            "fij,fj->fi", self.frames if frames is None else frames, g
        )
        self.assertLessEqual(float(np.linalg.norm(normalized, axis=1).max()), 1 + 1e-7)
        np.testing.assert_array_equal(mesh.triangles, self.surface.triangles)

    def fit(self, **kwargs):
        return fit_weighted_heights(
            self.surface, self.frames, reference_datum_m=0.0, **kwargs
        )

    def test_analytic_triangle_and_existing_owned_result_contract(self):
        result = self.fit()
        self.assertIsInstance(result, HeightFit)
        self.assertTrue(result.report["valid"])
        delta = (0.8 - 0.5 / np.sqrt(2)) / 3
        np.testing.assert_allclose(
            result.mesh.vertices[:, 2], [2 * delta, 0.8 - delta, 0.8 - delta], atol=1e-5
        )
        self.check(result.mesh)
        np.testing.assert_array_equal(
            result.mesh.vertices[:, :2], self.surface.vertices[:, :2]
        )
        np.testing.assert_array_equal(self.surface.vertices[:, 2], [0, 0.8, 0.8])
        with self.assertRaises(ValueError):
            result.mesh.vertices[0, 2] = 10
        with self.assertRaises(TypeError):
            result.report["valid"] = False

    def test_weights_control_prior_cost_without_mutating_inputs(self):
        weights = np.array([100.0, 1.0, 1.0])
        frames = self.frames.copy()
        result = fit_weighted_heights(
            self.surface, frames, reference_datum_m=0.0, weights=weights
        )
        self.check(result.mesh)
        self.assertLess(result.mesh.vertices[0, 2], 0.01)
        np.testing.assert_array_equal(weights, [100, 1, 1])
        np.testing.assert_array_equal(frames, self.frames)

    def test_feasible_affine_prior_returns_same_owned_mesh_without_solver(self):
        surface = Mesh([[0, 0, 5], [20, 0, 5.1], [0, 20, 5.2]], [[0, 1, 2]])
        with patch(
            "outshine_ref.transport.weighted_heights._fit.solve",
            side_effect=AssertionError,
        ):
            result = fit_weighted_heights(
                surface, self.frames, reference_datum_m=5.0, fixed_vertices=[0]
            )
        self.assertIs(result.mesh, surface)
        self.assertEqual(result.report["solver_status"], "prior_feasible")
        self.assertEqual(result.report["iterations"], 0)

    def test_contacts_bind_reference_datum_instead_of_prior(self):
        result = self.fit(fixed_vertices=[0, 1, 2, 2])
        np.testing.assert_array_equal(result.mesh.vertices[:, 2], [0, 0, 0])
        self.check(result.mesh)

    def test_invalid_solver_output_never_becomes_a_surface(self):
        for output in (None, np.array([np.nan, 1, 2]), np.array([1.0, 2.0])):
            with patch(
                "outshine_ref.transport.weighted_heights._fit.solve",
                return_value=(output, {"solver_status": "Failed"}),
            ):
                result = self.fit()
            self.assertIsNone(result.mesh)
            self.assertFalse(result.report["valid"])
            self.assertTrue(result.report["reason"])

    def test_feasible_but_worse_than_flat_reference_is_absent(self):
        with patch(
            "outshine_ref.transport.weighted_heights._fit.solve",
            return_value=(np.full(3, 10.0), {"solver_status": "MaxTime"}),
        ):
            result = self.fit()
        self.assertIsNone(result.mesh)
        self.assertEqual(result.report["reason"], "objective_worse_than_flat_reference")
        self.assertGreaterEqual(result.report["total_ms"], 0)

    def test_unrepresentable_cost_and_nonfinite_candidate_gradient_are_rejected(self):
        with self.assertRaises(ValueError):
            self.fit(weights=np.full(3, 1e308))
        with patch(
            "outshine_ref.transport.weighted_heights._fit.solve",
            return_value=(np.array([0.0, 1e308, -1e308]), {"solver_status": "Failed"}),
        ):
            result = self.fit()
        self.assertIsNone(result.mesh)
        self.assertEqual(result.report["reason"], "nonfinite_solver_gradient")

    def test_overslope_and_wrong_contact_candidate_is_contracted_not_exempted(self):
        with patch(
            "outshine_ref.transport.weighted_heights._fit.solve",
            return_value=(np.array([1.0, 9.0, 9.0]), {"solver_status": "MaxTime"}),
        ):
            result = self.fit(fixed_vertices=[0])
        self.assertTrue(result.report["valid"])
        self.assertLess(
            result.report["conservative_candidate"]["feasible_flat_contraction"], 1
        )
        self.assertEqual(result.mesh.vertices[0, 2], 0)
        self.check(result.mesh)

    def test_rotation_translation_anisotropy_and_reversed_faces(self):
        rotation = np.array([[0.0, -1.0], [1.0, 0.0]])
        vertices = self.surface.vertices.copy()
        vertices[:, :2] = vertices[:, :2] @ rotation.T + [1e6, -7e5]
        vertices[:, 2] += 163
        surface = Mesh(vertices, self.surface.triangles[:, ::-1])
        frames = np.array([[[1 / 0.08, 0], [0, 1 / 0.025]]]) @ rotation.T
        result = fit_weighted_heights(
            surface, frames, reference_datum_m=163.0, fixed_vertices=[0]
        )
        p = result.mesh.vertices[result.mesh.triangles]
        n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        g = -n[:, :2] / n[:, 2, None]
        self.assertLessEqual(np.linalg.norm(frames[0] @ g[0]), 1 + 1e-7)
        self.assertEqual(result.mesh.vertices[0, 2], 163)
        np.testing.assert_array_equal(
            result.mesh.vertices[:, :2], surface.vertices[:, :2]
        )
        np.testing.assert_array_equal(result.mesh.triangles, surface.triangles)

    def test_input_and_zero_time_budgets_skip_solver(self):
        for budget, reason in (
            (HeightBudget(max_vertices=2), "input_budget_exhausted"),
            (HeightBudget(max_faces=1, time_limit_s=0), "time_budget_exhausted"),
        ):
            with patch(
                "outshine_ref.transport.weighted_heights._fit.solve",
                side_effect=AssertionError,
            ):
                result = self.fit(budget=budget)
            self.assertIsNone(result.mesh)
            self.assertEqual(result.report["reason"], reason)

    def test_malformed_frames_weights_contacts_and_reference_rejected(self):
        for weights in ([1, 2], [1, 0, 1], [1, -1, 1], [1, np.nan, 1], [np.inf, 1, 1]):
            with self.assertRaises(ValueError):
                self.fit(weights=weights)
        for fixed in ([3], [-1], [0.5], [True], [[0]]):
            with self.assertRaises(ValueError):
                self.fit(fixed_vertices=fixed)
        for datum in (np.nan, np.inf, [0], 1j, True):
            with self.assertRaises(ValueError):
                fit_weighted_heights(self.surface, self.frames, reference_datum_m=datum)
        for frames in (np.zeros((1, 2, 2)), np.zeros((2, 2, 2)), self.frames * np.nan):
            with self.assertRaises(ValueError):
                fit_weighted_heights(self.surface, frames, reference_datum_m=0)


if __name__ == "__main__":
    unittest.main()
