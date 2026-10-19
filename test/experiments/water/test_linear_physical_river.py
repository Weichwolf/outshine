"""Derived normal bounds, equivalent substitution and physical QP consumers (#760)."""

import unittest
from pathlib import Path

import numpy as np
from compare_physical_river import inputs
from outshine_ref.contracts import Mesh
from physical_river_fit import fit_physical_surface
from physical_river_frame import normal_caps, normal_norm_bounds, tangent_directions
from physical_river_program import condition_matrix


class LinearPhysicalRiverTests(unittest.TestCase):
    def small(self):
        mesh = Mesh(
            np.array([[-100, -100, 500], [100, -100, 500], [0, 100, 500.0]]),
            np.array([[0, 1, 2]]),
        )
        rows = np.array([[0, 1, 0, 0, -0.1, 0], [0, 0, 1, 0, -0.005, 0.005]])
        return mesh, rows, np.array([[0, 1]])

    def test_nonorthogonal_interval_norm_bound_contains_independent_plane_normals(self):
        mesh, _rows, primary = self.small()
        random = np.random.default_rng(843)
        for anchor in ([0, 0], [10, 47], [179.99, 75], [18, -35]):
            caps = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, [400, 600])
            up = caps["up"][0]
            east = tangent_directions(up[None], np.array([[1, 0, 0]]))[0]
            north = np.cross(up, east)
            tangent = np.array([east, 0.6 * east + 0.8 * north])
            interval = np.array([[-0.1, 0.02], [-0.03, 0.05]])
            bound = normal_norm_bounds(
                caps, np.array([0, 0]), tangent, interval, primary
            )[0]
            values = np.r_[
                random.uniform(interval[:, 0], interval[:, 1], (1000, 2)),
                [[a, b] for a in interval[0] for b in interval[1]],
            ]
            coordinates = np.linalg.solve(
                tangent @ np.array([east, north]).T, values.T
            ).T
            normals = up - coordinates[:, :1] * east - coordinates[:, 1:] * north
            normals /= normals[:, 2, None]
            self.assertLessEqual(np.linalg.norm(normals, axis=1).max(), bound)

    def test_parallel_wrong_face_or_fractional_primary_rows_are_rejected(self):
        mesh, rows, primary = self.small()
        caps = normal_caps([10, 47], mesh.vertices[:, :2], mesh.triangles, [400, 600])
        for selected, faces in (
            (primary.astype(float), np.array([0, 0])),
            (primary, np.array([0, 1])),
            (np.array([[0, 2]]), np.array([0, 0])),
        ):
            with self.assertRaises(ValueError):
                normal_norm_bounds(caps, faces, rows[:, 1:4], rows[:, 4:], selected)
        parallel = tangent_directions(
            np.repeat(caps["up"], 2, axis=0), np.array([[1, 0, 0], [1, 0, 0]])
        )
        with self.assertRaises(ValueError):
            normal_norm_bounds(caps, np.array([0, 0]), parallel, rows[:, 4:], primary)

    def test_linear_rows_equal_constant_norm_substitution_in_original_socp(self):
        mesh, rows, primary = self.small()
        caps = normal_caps([10, 47], mesh.vertices[:, :2], mesh.triangles, [400, 600])
        faces = rows[:, 0].astype(int)
        tangent = tangent_directions(caps["up"][faces], rows[:, 1:4])
        bound = normal_norm_bounds(caps, faces, tangent, rows[:, 4:], primary)
        soc_a, soc_b, _cone, _rhs = condition_matrix(
            mesh, caps, faces, tangent, rows[:, 4], rows[:, 5], 500
        )
        qp_a, qp_b, cone, _rhs = condition_matrix(
            mesh, caps, faces, tangent, rows[:, 4], rows[:, 5], 500, bound
        )
        self.assertEqual(cone.shape, (0, 3))
        for x in np.random.default_rng(760).uniform(-10, 10, (20, 3)):
            np.testing.assert_allclose(
                soc_a @ np.r_[x, bound] - soc_b, qp_a @ x - qp_b, atol=2e-14, rtol=0
            )

    def test_qp_contacts_remain_hard_and_contradictions_return_no_field(self):
        mesh, rows, primary = self.small()
        for solver in ("clarabel", "osqp"):
            fit = fit_physical_surface(
                mesh,
                [10, 47],
                rows,
                up_bounds_m=[400, 600],
                primary_rows=primary,
                solver=solver,
                iterations=50000,
                fixed={0: 501.0},
            )
            self.assertIsNotNone(fit.mesh, fit.report)
            self.assertAlmostEqual(fit.mesh.vertices[0, 2], 501, delta=1e-9)
            invalid = fit_physical_surface(
                mesh,
                [10, 47],
                rows,
                up_bounds_m=[400, 600],
                primary_rows=primary,
                solver=solver,
                iterations=50000,
                fixed={0: 450.0, 1: 550.0},
            )
            self.assertIsNone(invalid.mesh)

    def test_original_ill_same_quality_sparse_qp_and_unchanged_source_geometry(self):
        fixture = (
            Path(__file__).with_name("fixtures") / "feldkirch-ill-native-inputs.json"
        )
        mesh, anchor, rows, box, arrays, _report = inputs(fixture)
        count = len(mesh.triangles)
        primary = np.c_[np.arange(count), np.arange(count) + count]
        reference = fit_physical_surface(mesh, anchor, rows, up_bounds_m=box)
        self.assertIsNotNone(reference.mesh)
        for solver in ("clarabel", "osqp"):
            fit = fit_physical_surface(
                mesh,
                anchor,
                rows,
                up_bounds_m=box,
                primary_rows=primary,
                solver=solver,
                iterations=50000,
                seconds=10,
            )
            self.assertIsNotNone(fit.mesh, fit.report)
            self.assertTrue(fit.report["finite_qp_optimal"])
            self.assertLessEqual(fit.report["whole_face_envelope_violation"], 1e-9)
            self.assertLess(fit.report["derived_normal_bound_excess"], 0)
            self.assertEqual(fit.report["unknowns"], len(mesh.vertices))
            self.assertEqual(fit.report["normal_auxiliaries"], 0)
            self.assertLess(
                fit.report["matrix_bytes"], reference.report["matrix_bytes"]
            )
            self.assertLess(
                abs(
                    fit.report["integrated_enu_up_rms_m"]
                    - reference.report["integrated_enu_up_rms_m"]
                ),
                1e-6,
            )
            np.testing.assert_array_equal(fit.mesh.triangles, mesh.triangles)
            np.testing.assert_array_equal(
                fit.mesh.vertices[:, :2], mesh.vertices[:, :2]
            )
        self.assertEqual(len(np.unique(arrays["source_members"])), 13)

    def test_exhausted_osqp_iteration_budget_returns_no_unvalidated_geometry(self):
        mesh, rows, primary = self.small()
        fit = fit_physical_surface(
            mesh,
            [10, 47],
            rows,
            up_bounds_m=[400, 600],
            primary_rows=primary,
            solver="osqp",
            iterations=1,
        )
        self.assertIsNone(fit.mesh)
        self.assertFalse(fit.report["valid"])


if __name__ == "__main__":
    unittest.main()
