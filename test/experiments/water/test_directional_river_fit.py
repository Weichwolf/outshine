from pathlib import Path
import unittest

import numpy as np

from outshine_ref.contracts import Mesh
from directional_river_fit import fit_surface, infer_flow
from river_profiles import RiverProfile
from river_surface import source_body, surface_mesh


class DirectionalRiverFit(unittest.TestCase):
    @staticmethod
    def rectangle(height):
        return Mesh(np.array([[0, 0, height[0]], [1, 0, height[1]],
                              [0, 1, height[2]], [1, 1, height[3]]], dtype=float),
                    np.array([[0, 1, 2], [1, 3, 2]], dtype=np.uint32))

    def test_uniform_downhill_surface_and_fixed_contacts_stay_unchanged(self):
        mesh = self.rectangle([1, .99, 1, .99])
        fit = fit_surface(mesh, [[1, 0], [1, 0]], fixed={0: 1, 3: .99})
        self.assertIsNotNone(fit.mesh)
        np.testing.assert_allclose(fit.mesh.vertices, mesh.vertices, rtol=0, atol=1e-10)
        self.assertTrue(fit.report["solver_optimal"])
        self.assertLess(fit.report["dual_gap_m"], 1e-10)

    def test_uphill_prior_is_corrected_with_the_analytic_minimax_error(self):
        fit = fit_surface(self.rectangle([0, 1, 0, 1]), [[1, 0], [1, 0]])
        self.assertIsNotNone(fit.mesh)
        self.assertAlmostEqual(fit.report["objective_error_m"], .5)
        self.assertLessEqual(fit.report["maximum_uphill_grade"], 1e-10)
        self.assertLess(fit.report["dual_gap_m"], 1e-10)

    def test_incompatible_hard_contacts_are_never_relaxed(self):
        fit = fit_surface(self.rectangle([0, 1, 0, 1]), [[1, 0], [1, 0]], fixed={0: 0, 1: 1})
        self.assertIsNone(fit.mesh)
        self.assertFalse(fit.report["valid"])

    def test_zero_work_budget_returns_an_explicit_absence(self):
        fit = fit_surface(self.rectangle([1, 0, 1, 0]), [[1, 0], [1, 0]], seconds=0)
        self.assertIsNone(fit.mesh)
        self.assertEqual(fit.report["reason"], "solver_budget_exhausted")

    def test_real_surface_follows_the_chosen_direction_on_every_face(self):
        with np.load(Path(__file__).parent / "fixtures/feldkirch-ill-profile.npz") as data:
            flat, _ = surface_mesh(source_body(data))
            height, _ = RiverProfile(data["segment_nodes"], data["segment_levels"]).sample(
                flat.vertices[:, :2], "blend-affine")
            mesh = Mesh(np.c_[flat.vertices[:, :2], height], flat.triangles)
            flow, _, _ = infer_flow(mesh, data["segment_nodes"], data["segment_levels"])
        fit = fit_surface(mesh, flow)
        self.assertIsNotNone(fit.mesh)
        self.assertLessEqual(fit.report["maximum_uphill_grade"], 1e-7)
        self.assertLessEqual(fit.report["maximum_downhill_grade"], .1 + 1e-7)
        self.assertLessEqual(fit.report["maximum_cross_grade"], .005 + 1e-7)
        self.assertLess(fit.report["dual_gap_m"], 1e-7)


if __name__ == "__main__":
    unittest.main()
