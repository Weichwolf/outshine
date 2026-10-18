from pathlib import Path
import unittest

import numpy as np

from outshine_ref.contracts import Mesh
from directional_river_fit import face_gradients, fit_surface, infer_flow, source_face_rows
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

    def test_every_crossing_course_keeps_its_own_direction(self):
        mesh = Mesh(np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=float),
                    np.array([[0, 1, 2]], dtype=np.uint32))
        rows, source, _ = source_face_rows(mesh, [[[0, .2], [.8, .2]],
                                                 [[.2, 0], [.2, .8]]], [[1, 0], [1, 0]])
        self.assertEqual(len(rows), 2)
        self.assertEqual(set(source), {0, 1})
        np.testing.assert_array_equal(rows[:, 0], 0)
        self.assertEqual({tuple(row) for row in rows[:, 1:3]}, {(1, 0), (0, 1)})

    def test_real_surface_follows_the_chosen_direction_on_every_face(self):
        with np.load(Path(__file__).parent / "fixtures/feldkirch-ill-profile.npz") as data:
            flat, _ = surface_mesh(source_body(data))
            height, _ = RiverProfile(data["segment_nodes"], data["segment_levels"]).sample(
                flat.vertices[:, :2], "blend-affine")
            mesh = Mesh(np.c_[flat.vertices[:, :2], height], flat.triangles)
            flow, _, _ = infer_flow(mesh, data["segment_nodes"], data["segment_levels"])
            rows, _, coverage = source_face_rows(mesh, data["segment_nodes"], data["segment_levels"])
        baseline = fit_surface(mesh, flow)
        gradients = face_gradients(mesh, baseline.mesh.vertices[:, 2])
        baseline_uphill = (gradients[rows[:, 0].astype(int)] * rows[:, 1:3]).sum(axis=1)
        self.assertGreater(baseline_uphill.max(), 1e-4)
        fit = fit_surface(mesh, flow, directed_rows=rows)
        self.assertIsNotNone(fit.mesh)
        self.assertLessEqual(fit.report["maximum_uphill_grade"], 1e-7)
        self.assertLessEqual(fit.report["maximum_downhill_grade"], .1 + 1e-7)
        self.assertLessEqual(fit.report["maximum_cross_grade"], .005 + 1e-7)
        self.assertLessEqual(fit.report["directed_row_violation_m_per_m"], 1e-7)
        self.assertGreater(coverage["outside_domain_length_m"], 0)
        self.assertLess(fit.report["dual_gap_m"], 1e-7)


if __name__ == "__main__":
    unittest.main()
