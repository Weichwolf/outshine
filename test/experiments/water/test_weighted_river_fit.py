"""Same directed feasible set, different explicit objective and separate hard pins."""

import unittest
from pathlib import Path
from unittest.mock import patch

import directional_river_fit as reference
import numpy as np
from outshine_ref.contracts import Mesh
from weighted_river_fit import constraints, directions, fit_weighted_surface


def rectangle(height):
    return Mesh(
        np.array(
            [
                [0, 0, height[0]],
                [1, 0, height[1]],
                [0, 1, height[2]],
                [1, 1, height[3]],
            ],
            float,
        ),
        np.array([[0, 1, 2], [1, 3, 2]], np.uint32),
    )


class WeightedRiverFit(unittest.TestCase):
    def test_weighted_analytic_solution_differs_from_minimax(self):
        mesh = Mesh(
            np.array([[0, 0, 0], [1, 0, 1], [0, 1, 0]], float),
            np.array([[0, 1, 2]], np.uint32),
        )
        fit = fit_weighted_surface(mesh, [[1, 0]], weights=[1, 9, 1], cross_grade=2)
        self.assertIsNotNone(fit.mesh, dict(fit.report))
        np.testing.assert_allclose(fit.mesh.vertices[:, 2], [0.9, 0.9, 0], atol=1e-6)
        self.assertGreater(fit.report["maximum_prior_error_m"], 0.5)
        self.assertTrue(fit.report["solver_optimal"])

    def test_separate_compatible_pins_preserve_a_downhill_surface(self):
        mesh = rectangle([1, 0.99, 1, 0.99])
        fit = fit_weighted_surface(mesh, [[1, 0], [1, 0]], fixed={0: 1, 3: 0.99})
        self.assertIsNotNone(fit.mesh, dict(fit.report))
        np.testing.assert_allclose(fit.mesh.vertices, mesh.vertices, atol=1e-6, rtol=0)
        self.assertLess(fit.report["fixed_height_error_m"], 1e-8)

    def test_incompatible_pins_do_not_get_a_relaxed_result(self):
        fit = fit_weighted_surface(
            rectangle([0, 1, 0, 1]), [[1, 0], [1, 0]], fixed={0: 0, 1: 1}
        )
        self.assertIsNone(fit.mesh)
        self.assertFalse(fit.report["valid"])

    def test_multiple_directions_on_one_face_are_all_retained(self):
        mesh = rectangle([0, 0, 1, 1])
        rows = [[0, 1, 0, -0.1, 0], [0, 0, 1, -0.1, 0], [1, 0, 1, -0.1, 0]]
        fit = fit_weighted_surface(
            mesh, [[1, 0], [1, 0]], directed_rows=rows, cross_grade=2
        )
        self.assertIsNotNone(fit.mesh, dict(fit.report))
        gradients = reference.face_gradients(mesh, fit.mesh.vertices[:, 2])
        self.assertLess(gradients.max(), 1e-7)

    def test_constraint_rows_are_exactly_the_original_lp_rows(self):
        mesh = rectangle([1, 0.99, 1, 0.99])
        flow = [[1, 0], [1, 0]]
        rows = [[0, 1, 1, -0.1, 0], [0, 0, 1, -0.2, 0.03]]
        captured = {}
        old = reference.linprog

        def observed(*args, **kwargs):
            captured.update(kwargs)
            return old(*args, **kwargs)

        with patch.object(reference, "linprog", side_effect=observed):
            fit = reference.fit_surface(mesh, flow, directed_rows=rows)
        self.assertIsNotNone(fit.mesh)
        normalized, directions_rows = directions(mesh, flow, rows)
        matrix, bounds = constraints(mesh, normalized, directions_rows, 0.1, 0.005)
        difference = matrix - captured["A_ub"][2 * len(mesh.vertices) :, :-1]
        difference.eliminate_zeros()
        self.assertEqual(difference.nnz, 0)
        np.testing.assert_array_equal(
            bounds, captured["b_ub"][2 * len(mesh.vertices) :]
        )

    def test_zero_work_budgets_do_not_invoke_solver(self):
        with patch("weighted_river_fit.clarabel.DefaultSolver") as solver:
            for kwargs in ({"seconds": 0}, {"iterations": 0}):
                fit = fit_weighted_surface(
                    rectangle([0, 1, 0, 1]), [[1, 0], [1, 0]], **kwargs
                )
                self.assertIsNone(fit.mesh)
            solver.assert_not_called()

    def test_real_ten_metre_surface_meets_the_unchanged_dual_certificate(self):
        from river_profiles import RiverProfile
        from river_surface import source_body, surface_mesh

        with np.load(
            Path(__file__).parent / "fixtures/feldkirch-ill-profile.npz"
        ) as data:
            flat, _topology = surface_mesh(source_body(data), 10.0)
            target, _references = RiverProfile(
                data["segment_nodes"], data["segment_levels"]
            ).sample(flat.vertices[:, :2], "blend-affine")
            mesh = Mesh(np.c_[flat.vertices[:, :2], target], flat.triangles)
            flow, _source, _distances = reference.infer_flow(
                mesh, data["segment_nodes"], data["segment_levels"]
            )
            rows, _sources, _coverage = reference.source_face_rows(
                mesh, data["segment_nodes"], data["segment_levels"]
            )
        xy = mesh.vertices[mesh.triangles, :2]
        u, v = xy[:, 1] - xy[:, 0], xy[:, 2] - xy[:, 0]
        area = np.abs(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]) / 2
        weights = np.bincount(mesh.triangles.ravel(), weights=np.repeat(area / 3, 3))
        fit = fit_weighted_surface(mesh, flow, weights=weights, directed_rows=rows)
        self.assertIsNotNone(fit.mesh, dict(fit.report))
        self.assertLessEqual(fit.report["dual_gap"], 1e-7)
        self.assertLessEqual(fit.report["source_face_violation"], 1e-7)

    def test_bad_weight_and_direction_inputs_are_rejected(self):
        for kwargs in (
            {"weights": [1, 0, 1, 1]},
            {"weights": [1, np.inf, 1, 1]},
            {"directed_rows": [[2, 1, 0, -0.1, 0]]},
            {"directed_rows": [[0, 0, 0, -0.1, 0]]},
        ):
            with self.assertRaises(ValueError):
                fit_weighted_surface(
                    rectangle([0, 1, 0, 1]), [[1, 0], [1, 0]], **kwargs
                )


if __name__ == "__main__":
    unittest.main()
