"""Physical curvature, independent derivatives, original Ill, pins and bounded failure."""

import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from compare_physical_river import audit, inputs
from outshine_ref.contracts import Mesh
from outshine_ref.places.camera import axes
from outshine_ref.water import fit_physical_surface
from outshine_ref.water._fit import grade_envelopes
from outshine_ref.water.frame import (
    inverse_frame,
    model_height_targets,
    normal_caps,
    tangent_directions,
)
from outshine_ref.water.geodetic import WGS84_A, curvature_radii, query_facet


def triangle():
    return Mesh(
        np.array([[-100, -100, 500], [100, -100, 500], [0, 100, 500.0]]),
        np.array([[0, 1, 2]]),
    )


class PhysicalRiverTests(unittest.TestCase):
    def test_height_targets_are_geodetic_not_enu_up(self):
        xy = np.array([[-2000, 1000], [0, 0], [1000, 1000]])
        for anchor in ([0, 0], [10, 47], [179.99, 75], [18, -35]):
            target = np.array([450.0, 500, 470])
            up = model_height_targets(anchor, xy, target)
            _, height, _ = inverse_frame(anchor, np.c_[xy, up])
            np.testing.assert_allclose(
                height, target, rtol=0, atol=64 * np.spacing(WGS84_A)
            )
            self.assertGreater(abs(up[0] - target[0]), 0.3)

    def test_normal_cap_contains_whole_prism_counterexamples(self):
        mesh = triangle()
        random = np.random.default_rng(760)
        bary = random.dirichlet(np.ones(3), 1000)
        for anchor in ([0, 0], [10, 47], [179.99, 75], [18, -35]):
            cap = normal_caps(anchor, mesh.vertices[:, :2], mesh.triangles, [400, 600])
            points = np.c_[bary @ mesh.vertices[:, :2], random.uniform(400, 600, 1000)]
            corners = np.array(
                [[x, y, z] for x, y in mesh.vertices[:, :2] for z in (400, 600)]
            )
            _, _, up = inverse_frame(anchor, np.r_[points, corners])
            self.assertTrue(
                (np.linalg.norm(up - cap["up"][0], axis=1) <= cap["delta"][0]).all()
            )
        for bounds in ([-1, 600], [0, 600], [600, 400]):
            with self.assertRaises(ValueError):
                normal_caps([0, 0], mesh.vertices[:, :2], mesh.triangles, bounds)

    def test_physical_grade_matches_independent_height_differences(self):
        mesh = triangle()
        vertices = mesh.vertices.copy()
        vertices[:, 2] += np.array([1, -1, 0.2])
        normal = np.cross(vertices[1] - vertices[0], vertices[2] - vertices[0])
        for anchor in ([0, 0], [10, 47], [179.99, 75], [18, -35]):
            geographic, height, up = inverse_frame(anchor, vertices.mean(0)[None])
            cap = normal_caps(anchor, vertices[:, :2], mesh.triangles, [400, 600])
            tangent = tangent_directions(cap["up"], np.array([[1, 0.3, 0]]))[0]
            projected = tangent - up[0] * (up[0] @ tangent)
            projected /= np.linalg.norm(projected)
            expected = -(normal @ projected) / (normal @ up[0])
            frame = axes(anchor[1], anchor[0])
            lon, lat = geographic[0]
            east, north = axes(lat, lon)[:2] @ frame.T @ projected
            prime, meridian = curvature_radii(lat)
            delta = np.rad2deg(
                [
                    east / ((prime + height[0]) * np.cos(np.deg2rad(lat))),
                    north / (meridian + height[0]),
                ]
            )
            actual = query_facet(
                anchor,
                vertices,
                [geographic[0] - delta, geographic[0] + delta],
                metric_height_m=float(height[0]),
            )
            derivative = np.diff(actual["plane_height_m"])[0] / 2
            self.assertAlmostEqual(derivative, expected, delta=5e-9)
            envelope = grade_envelopes(normal[None], cap, np.array([0]), tangent[None])[
                0
            ]
            self.assertLessEqual(envelope[0], expected)
            self.assertGreaterEqual(envelope[1], expected)

    def test_centre_only_accepts_an_uphill_facet_whole_prism_repairs_it(self):
        mesh = triangle()
        rows = np.array([[0, 1, 0, 0, -0.1, 0]])
        control = fit_physical_surface(
            mesh, [0, 0], rows, up_bounds_m=[400, 600], normal_model="centre"
        )
        physical = fit_physical_surface(mesh, [0, 0], rows, up_bounds_m=[400, 600])
        self.assertIsNotNone(control.mesh)
        self.assertIsNotNone(physical.mesh)
        value = query_facet(
            [0, 0], control.mesh.vertices, [[0.0002, 0]], metric_height_m=500
        )
        self.assertGreater(value["grade_m_per_m"][0, 0], 1e-6)
        self.assertLessEqual(physical.report["whole_face_envelope_violation"], 1e-9)
        self.assertTrue(physical.report["finite_qp_optimal"])

    def test_contacts_are_hard_and_incompatible_pins_do_not_get_relaxed(self):
        mesh = triangle()
        rows = np.array([[0, 1, 0, 0, -0.1, 0]])
        valid = fit_physical_surface(
            mesh, [10, 47], rows, up_bounds_m=[400, 600], fixed={0: 501.0}
        )
        self.assertIsNotNone(valid.mesh)
        self.assertAlmostEqual(valid.mesh.vertices[0, 2], 501, delta=1e-9)
        invalid = fit_physical_surface(
            mesh, [10, 47], rows, up_bounds_m=[400, 600], fixed={0: 450.0, 1: 550.0}
        )
        self.assertIsNone(invalid.mesh)
        self.assertFalse(invalid.report["valid"])

    def test_zero_budget_never_calls_solver_and_bad_direction_is_rejected(self):
        mesh = triangle()
        rows = np.array([[0, 1, 0, 0, -0.1, 0]])
        with patch(
            "outshine_ref.water._program.clarabel.DefaultSolver",
            side_effect=AssertionError,
        ):
            result = fit_physical_surface(
                mesh, [10, 47], rows, up_bounds_m=[400, 600], seconds=0
            )
        self.assertEqual(result.report["reason"], "solver_budget_exhausted")
        rows[0, 1] = 0
        with self.assertRaises(ValueError):
            fit_physical_surface(mesh, [10, 47], rows, up_bounds_m=[400, 600])

    def test_original_ill_retains_sources_island_and_physical_constraints(self):
        fixture = (
            Path(__file__).with_name("fixtures") / "feldkirch-ill-native-inputs.json"
        )
        mesh, anchor, rows, box, arrays, report = inputs(fixture)
        self.assertEqual(report["topology"]["holes"], 1)
        self.assertEqual(len(np.unique(arrays["source_members"])), 13)
        self.assertEqual(len(arrays["course_original_address"]), 121)
        fit = fit_physical_surface(mesh, anchor, rows, up_bounds_m=box)
        self.assertIsNotNone(fit.mesh)
        self.assertTrue(fit.report["finite_qp_optimal"])
        np.testing.assert_array_equal(fit.mesh.triangles, mesh.triangles)
        np.testing.assert_array_equal(fit.mesh.vertices[:, :2], mesh.vertices[:, :2])
        sampled, _ = audit(fit.mesh, anchor, rows, box, arrays["row_tangent"])
        self.assertEqual(sampled["sampled_physical_grade_violation"], 0)
        self.assertLess(sampled["sampled_cap_excess"], 0)


if __name__ == "__main__":
    unittest.main()
