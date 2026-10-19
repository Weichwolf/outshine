"""Installed physical water API: ownership, frame, optional imports and bounded absence."""

import json
import subprocess
import sys
import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface
from outshine_ref.water.frame import model_height_targets
from outshine_ref.water.geodetic import query_facet


class WaterProfileLibraryTests(unittest.TestCase):
    def inputs(self):
        mesh = Mesh(
            np.array([[-100, -100, 500], [100, -100, 500], [0, 100, 500.0]]),
            np.array([[0, 1, 2]]),
        )
        return mesh, np.array([[0, 1, 0, 0, -0.1, 0], [0, 0, 1, 0, -0.005, 0.005]])

    def test_water_import_does_not_load_optional_solver_or_geodesy(self):
        script = "import sys,json,outshine_ref; before=set(sys.modules); import outshine_ref.water; print(json.dumps(sorted(set(sys.modules)-before)))"
        modules = json.loads(
            subprocess.check_output([sys.executable, "-c", script], text=True)
        )
        self.assertFalse(
            any(m.startswith(("clarabel", "scipy", "pyproj", "osqp")) for m in modules)
        )

    def test_public_linear_fit_preserves_owned_xyz_faces_and_explicit_contact(self):
        mesh, rows = self.inputs()
        original = mesh.vertices.copy()
        fit = fit_physical_surface(
            mesh,
            [10, 47],
            rows,
            up_bounds_m=[400, 600],
            primary_rows=np.array([[0, 1]]),
            fixed={0: 501.0},
        )
        self.assertIsNotNone(fit.mesh)
        self.assertTrue(fit.report["physical_whole_face_admitted"])
        self.assertTrue(fit.report["finite_qp_optimal"])
        self.assertAlmostEqual(fit.mesh.vertices[0, 2], 501, delta=1e-9)
        np.testing.assert_array_equal(mesh.vertices, original)
        np.testing.assert_array_equal(fit.mesh.vertices[:, :2], original[:, :2])
        np.testing.assert_array_equal(fit.mesh.triangles, mesh.triangles)
        self.assertFalse(fit.mesh.vertices.flags.writeable)
        self.assertFalse(fit.mesh.triangles.flags.writeable)
        self.assertFalse(np.shares_memory(mesh.vertices, fit.mesh.vertices))

    def test_zero_budget_does_not_construct_a_solver(self):
        mesh, rows = self.inputs()
        with patch(
            "outshine_ref.water._program.clarabel.DefaultSolver",
            side_effect=AssertionError,
        ):
            fit = fit_physical_surface(
                mesh, [10, 47], rows, up_bounds_m=[400, 600], seconds=0
            )
        self.assertIsNone(fit.mesh)
        self.assertEqual(fit.report["reason"], "solver_budget_exhausted")

    def test_zero_model_height_uses_signed_up_and_preserves_physical_contacts(self):
        xy = np.array([[-1000, -1000], [1000, -1000], [0, 1000.0]])
        height = model_height_targets([10, 47], xy, np.zeros(3))
        self.assertTrue((height < 0).all())
        mesh = Mesh(np.c_[xy, height], np.array([[0, 1, 2]]))
        rows = np.array([[0, 1, 0, 0, -0.005, 0.005], [0, 0, 1, 0, -0.005, 0.005]])
        fit = fit_physical_surface(
            mesh,
            [10, 47],
            rows,
            up_bounds_m=[-1, 1],
            fixed={0: float(height[0])},
            contact_points=mesh.vertices.mean(0)[None],
            primary_rows=np.array([[0, 1]]),
        )
        self.assertIsNotNone(fit.mesh, fit.report)
        self.assertTrue(fit.report["physical_whole_face_admitted"])
        self.assertLessEqual(fit.report["fixed_point_up_error_m"], 1e-9)
        self.assertAlmostEqual(fit.mesh.vertices[0, 2], height[0], delta=1e-9)
        np.testing.assert_array_equal(fit.mesh.vertices[:, :2], xy)
        np.testing.assert_array_equal(fit.mesh.triangles, mesh.triangles)

    def test_foreign_shape_and_bad_geographic_frame_rejected(self):
        mesh, rows = self.inputs()
        with self.assertRaises(TypeError):
            fit_physical_surface(object(), [10, 47], rows, up_bounds_m=[400, 600])
        for anchor in ([10, 91], [181, 47], [np.nan, 47]):
            with self.assertRaises(ValueError):
                fit_physical_surface(mesh, anchor, rows, up_bounds_m=[400, 600])

    def test_shared_geodetic_query_uses_the_returned_physical_triangles(self):
        mesh, rows = self.inputs()
        fit = fit_physical_surface(
            mesh, [0, 0], rows, up_bounds_m=[400, 600], primary_rows=np.array([[0, 1]])
        )
        self.assertIsNotNone(fit.mesh)
        query = query_facet([0, 0], fit.mesh.vertices, [[0, 0]], metric_height_m=500)
        self.assertTrue(query["inside_numeric"][0])
        self.assertLessEqual(query["grade_m_per_m"][0, 0], 0)


if __name__ == "__main__":
    unittest.main()
