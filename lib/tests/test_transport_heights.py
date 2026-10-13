"""Whole-face height bounds, fixed contacts and bounded failure behaviour."""

import unittest

import numpy as np

from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget, fit_surface_heights


class TransportHeights(unittest.TestCase):
    def setUp(self):
        self.vertices = np.array([[0., 0., 0.], [1, 0, .08], [0, 1, .08]])
        self.faces = np.array([[0, 1, 2]])
        self.frames = np.array([np.eye(2) / .08])

    def check(self, mesh, frames):
        p = mesh.vertices[mesh.triangles]
        normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        gradient = -normal[:, :2] / normal[:, 2:3]
        self.assertLessEqual(np.linalg.norm(np.einsum("fki,fi->fk", frames, gradient), axis=1).max(), 1.0000001)

    def test_analytic_triangle_shared_buffers_and_immutable_result(self):
        source = Mesh(self.vertices, self.faces)
        result = fit_surface_heights(source, self.frames)
        self.assertIsNotNone(result.mesh)
        self.check(result.mesh, self.frames)
        self.assertAlmostEqual(result.report["maximum_height_change_m"], (.08 - .08 / np.sqrt(2)) * .5, places=7)
        np.testing.assert_array_equal(source.vertices, self.vertices)
        np.testing.assert_array_equal(result.mesh.vertices[:, :2], self.vertices[:, :2])
        np.testing.assert_array_equal(result.mesh.triangles, source.triangles)
        with self.assertRaises(TypeError):
            result.report["valid"] = False
        with self.assertRaises(ValueError):
            result.mesh.vertices[0] = 0

    def test_prior_feasible_does_no_solver_work(self):
        source = Mesh(self.vertices * [1, 1, .1], self.faces)
        result = fit_surface_heights(source, self.frames)
        self.assertIs(result.mesh, source)
        self.assertEqual(result.report["solver_status"], "prior_feasible")
        self.assertEqual(result.report["iterations"], 0)

    def test_anisotropy_and_rotation_translation_preserve_the_bound(self):
        frame = np.array([[[1 / .08, 0.], [0., 1 / .025]]])
        for rotation in (np.eye(2), np.array([[0., -1], [1, 0]])):
            vertices = self.vertices.copy()
            vertices[:, :2] = vertices[:, :2] @ rotation.T + [1e6, -7e5]
            vertices[:, 2] += 163.
            frames = frame @ rotation.T
            source = Mesh(vertices, self.faces)
            result = fit_surface_heights(source, frames)
            self.assertIsNotNone(result.mesh)
            self.check(result.mesh, frames)
            np.testing.assert_array_equal(result.mesh.vertices[:, :2], source.vertices[:, :2])

    def test_fixed_and_infeasible_contacts(self):
        source = Mesh(self.vertices, self.faces)
        feasible = fit_surface_heights(source, self.frames, fixed={0: 0.})
        self.assertAlmostEqual(feasible.mesh.vertices[0, 2], 0., places=7)
        failed = fit_surface_heights(source, self.frames, fixed={0: 0., 1: 2.})
        self.assertIsNone(failed.mesh)
        self.assertFalse(failed.report["valid"])
        self.assertEqual(failed.report["solver_status"], "PrimalInfeasible")

    def test_budgets_produce_explained_absence(self):
        source = Mesh(self.vertices, self.faces)
        for budget, reason in ((HeightBudget(max_vertices=2), "input_budget_exhausted"),
                               (HeightBudget(time_limit_s=0), "time_budget_exhausted")):
            result = fit_surface_heights(source, self.frames, budget=budget)
            self.assertIsNone(result.mesh)
            self.assertFalse(result.report["valid"])
            self.assertEqual(result.report["reason"], reason)
        expired = fit_surface_heights(source, self.frames, budget=HeightBudget(time_limit_s=1e-12))
        if expired.mesh is not None:
            self.check(expired.mesh, self.frames)
            self.assertTrue(expired.report["valid"])

    def test_bad_frames_contacts_and_projected_geometry_are_rejected(self):
        source = Mesh(self.vertices, self.faces)
        for frames in (np.zeros((1, 2, 2)), np.ones((2, 2, 2)), self.frames * float("nan")):
            with self.subTest(frames=frames.tolist()), self.assertRaises(ValueError):
                fit_surface_heights(source, frames)
        for fixed in ({3: 0.}, {0: float("inf")}, {-1: 0.}):
            with self.assertRaises(ValueError):
                fit_surface_heights(source, self.frames, fixed=fixed)
        vertices = self.vertices.copy()
        vertices[:, 1] = 0
        with self.assertRaises(ValueError):
            fit_surface_heights(Mesh(vertices, self.faces), self.frames)
        for budget in (dict(time_limit_s=-1), dict(max_iterations=0), dict(max_faces=2.5)):
            with self.assertRaises(ValueError):
                HeightBudget(**budget)


if __name__ == "__main__":
    unittest.main()
