"""Independent closed boundary, analytic volume, actual through rays and capacity."""

import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.buildings.opening_panels import OpeningPanel, panel_surface


def hits(mesh, point):
    triangles = mesh["vertices"][mesh["faces"]]
    a, b = (
        triangles[:, 1, :2] - triangles[:, 0, :2],
        triangles[:, 2, :2] - triangles[:, 0, :2],
    )
    p = np.asarray(point) - triangles[:, 0, :2]
    determinant = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    active = abs(determinant) > 1e-12
    u = np.divide(
        p[:, 0] * b[:, 1] - p[:, 1] * b[:, 0],
        determinant,
        out=np.zeros(len(a)),
        where=active,
    )
    v = np.divide(
        a[:, 0] * p[:, 1] - a[:, 1] * p[:, 0],
        determinant,
        out=np.zeros(len(a)),
        where=active,
    )
    active &= (u >= -1e-10) & (v >= -1e-10) & (u + v <= 1 + 1e-10)
    depth = (
        triangles[:, 0, 2]
        + u * (triangles[:, 1, 2] - triangles[:, 0, 2])
        + v * (triangles[:, 2, 2] - triangles[:, 0, 2])
    )
    return np.unique(np.round(depth[active], 12))


class OpeningPanels(unittest.TestCase):
    def test_closed_oriented_surface_and_analytic_volume(self):
        plan = OpeningPanel(3.2, 3.0, (1.05, 0.16, 2.15, 2.31))
        mesh = panel_surface(plan)
        triangles = mesh["vertices"][mesh["faces"]]
        directed = {}
        for t in triangles:
            for a, b in zip(t, np.roll(t, -1, axis=0), strict=True):
                a, b = tuple(a), tuple(b)
                directed[a, b] = directed.get((a, b), 0) + 1
        for (a, b), count in directed.items():
            self.assertEqual(count, 1)
            self.assertEqual(directed.get((b, a)), 1)
        n = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        np.testing.assert_allclose(
            n / np.linalg.norm(n, axis=1, keepdims=True),
            mesh["normals"][mesh["faces"][:, 0]],
        )
        volume = (
            np.einsum(
                "ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])
            ).sum()
            / 6
        )
        self.assertAlmostEqual(volume, (3.2 * 3.0 - 1.1 * 2.15) * 0.24, places=12)
        self.assertEqual(len(mesh["faces"]), 32)
        uv = mesh["uv"][mesh["faces"]]
        np.testing.assert_allclose(
            np.linalg.norm(triangles[:, 1:] - triangles[:, :1], axis=2),
            np.linalg.norm(uv[:, 1:] - uv[:, :1], axis=2),
        )

    def test_real_aperture_rays_and_closed_counterpart(self):
        mesh = panel_surface(OpeningPanel(3.2, 3.0, (1.05, 0.16, 2.15, 2.31)))
        self.assertEqual(len(hits(mesh, (1.6, 1.2))), 0)
        for point in ((0.2, 1.1), (2.8, 1.1), (1.6, 0.08), (1.6, 2.7)):
            np.testing.assert_allclose(hits(mesh, point), [-0.24, 0.0])
        bad = {
            "vertices": np.r_[
                mesh["vertices"],
                [[1.05, 0.16, 0.0], [2.15, 0.16, 0.0], [2.15, 2.31, 0.0]],
            ],
            "faces": np.r_[mesh["faces"], [[64, 65, 66]]],
        }
        self.assertTrue(len(hits(bad, (1.7, 1.0))))

    def test_invalid_capacity_and_owned_arrays(self):
        for opening in (
            (0.0, 0.16, 2.0, 2.3),
            (1.0, 0.0, 2.0, 2.3),
            (1.0, 0.16, 4.0, 2.3),
            (2.0, 0.16, 1.0, 2.3),
            [1.0, 0.16, 2.0, 2.3],
        ):
            with self.assertRaises(ValueError):
                OpeningPanel(3.2, 3.0, opening)
        plan = OpeningPanel(3.2, 3.0, (1.05, 0.16, 2.15, 2.31))
        with patch(
            "outshine_ref.buildings.opening_panels.np.array",
            side_effect=AssertionError("expanded"),
        ), self.assertRaisesRegex(ValueError, "capacity"):
            panel_surface(plan, max_triangles=31)
        a, b = panel_surface(plan), panel_surface(plan)
        for key in a:
            np.testing.assert_array_equal(a[key], b[key])
            self.assertFalse(np.shares_memory(a[key], b[key]))


if __name__ == "__main__":
    unittest.main()
