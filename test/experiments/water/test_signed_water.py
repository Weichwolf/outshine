"""Signed prism bounds checked against an independent nearest-ellipsoid solve."""

import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.places.camera import axes
from outshine_ref.water import fit_physical_surface, frame
from outshine_ref.water.geodetic import WGS84_A, WGS84_E2
from signed_water_reference import nearest_normals, prism_points

XY = np.array([[-1000, -1000], [1000, -1000], [0, 1000.0]])
FACES = np.array([[0, 1, 2]])
ROWS = np.array([[0, 1, 0, 0, -0.005, 0.005], [0, 0, 1, 0, -0.005, 0.005]])


class SignedWater(unittest.TestCase):
    def test_signed_support_and_whole_prism_normals_against_nearest_points(self):
        for anchor in ([0, 0], [10, 47], [179.99, 75], [18, -35]):
            for box in ([0, 1], [-1, 1], [-600, -400], [-50001, -49999]):
                with self.subTest(anchor=anchor, box=box):
                    cap = frame.normal_caps(anchor, XY, FACES, box)
                    points = prism_points(XY, box)
                    normals, heights = nearest_normals(anchor, points)
                    self.assertGreaterEqual(
                        float(np.min(heights - points[:, 2])), -64 * np.spacing(WGS84_A)
                    )
                    excess = (
                        np.linalg.norm(normals - cap["up"][0], axis=1) - cap["delta"][0]
                    )
                    self.assertLessEqual(float(excess.max()), 0)

    def test_inaccurate_inverse_centre_requires_roundtrip_inflation(self):
        anchor = [10, 47]
        xy = XY * 1e-9
        bounds = [-500.000001, -499.999999]
        inverse = frame.inverse_frame

        def perturbed(anchor, points):
            geographic, height, _up = inverse(anchor, points)
            geographic[:, 1] += 1e-5
            up = (
                np.array([axes(y, x)[2] for x, y in geographic])
                @ axes(anchor[1], anchor[0]).T
            )
            return geographic, height, up

        with patch.object(frame, "inverse_frame", perturbed):
            cap = frame.normal_caps(anchor, xy, FACES, bounds)
        points = prism_points(xy, bounds)
        normals, _height = nearest_normals(anchor, points)
        variation = np.linalg.norm(normals - cap["up"][0], axis=1).max()
        naive = cap["radius_m"][0] / (WGS84_A * (1 - WGS84_E2) + bounds[0]) + 1e-12
        self.assertGreater(variation, naive)
        self.assertLessEqual(variation, cap["delta"][0])

    def test_zero_and_below_datum_fit_keep_hard_contacts_and_all_intervals(self):
        bary = prism_points(XY, [0, 0])[:, :2]
        for anchor in ([10, 47], [35.5, 31.5], [179.99, 75], [18, -35]):
            for model_height in (0.0, -430.0):
                target = frame.model_height_targets(
                    anchor, XY, np.full(3, model_height)
                )
                mesh = Mesh(np.c_[XY, target], FACES)
                box = [target.min() - 1, target.max() + 1]
                centre_contact = mesh.vertices.mean(0)[None]
                for primary in (None, np.array([[0, 1]])):
                    result = fit_physical_surface(
                        mesh,
                        anchor,
                        ROWS,
                        up_bounds_m=box,
                        fixed={0: float(target[0])},
                        contact_points=centre_contact,
                        primary_rows=primary,
                    )
                    self.assertIsNotNone(result.mesh, result.report)
                    self.assertLessEqual(result.report["fixed_point_up_error_m"], 1e-9)
                    self.assertAlmostEqual(
                        result.mesh.vertices[0, 2], target[0], delta=1e-9
                    )
                    a, b, c = result.mesh.vertices
                    normal = np.cross(b - a, c - a)
                    weights = np.linalg.solve(
                        np.stack([b[:2] - a[:2], c[:2] - a[:2]], axis=1),
                        (bary - a[:2]).T,
                    ).T
                    points = np.c_[
                        bary, a[2] + weights @ np.array([b[2] - a[2], c[2] - a[2]])
                    ]
                    ups, _heights = nearest_normals(anchor, points)
                    caps = frame.normal_caps(anchor, XY, FACES, box)
                    directions = frame.tangent_directions(
                        np.repeat(caps["up"], 2, axis=0), ROWS[:, 1:4]
                    )
                    for row, direction in zip(ROWS, directions, strict=True):
                        tangents = direction - (ups @ direction)[:, None] * ups
                        tangents /= np.linalg.norm(tangents, axis=1)[:, None]
                        grades = -(tangents @ normal) / (ups @ normal)
                        self.assertGreaterEqual(grades.min(), row[4] - 1e-9)
                        self.assertLessEqual(grades.max(), row[5] + 1e-9)
                    np.testing.assert_array_equal(result.mesh.vertices[:, :2], XY)
                    np.testing.assert_array_equal(result.mesh.triangles, FACES)

    def test_invalid_normal_domain_and_contradictory_signed_pins_stay_rejected(self):
        radius = WGS84_A * (1 - WGS84_E2)
        for bounds in ([-radius, 1], [-radius - 1, 1], [1, 1], [1, 0], [np.nan, 1]):
            with self.assertRaises(ValueError):
                frame.normal_caps([10, 47], XY, FACES, bounds)
        mesh = Mesh(np.c_[XY, np.zeros(3)], FACES)
        result = fit_physical_surface(
            mesh, [10, 47], ROWS, up_bounds_m=[-1, 1], fixed={0: 10.0}
        )
        self.assertIsNone(result.mesh)


if __name__ == "__main__":
    unittest.main()
