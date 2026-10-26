"""Independent projections, cavity integrals, rays and bounded partial replay."""

import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.buildings.facades import FacadeRepeat
from outshine_ref.buildings.recessed_facades import RecessPlan, recess_surface


def ray(mesh, origin, direction):
    triangles = mesh["vertices"][mesh["faces"]]
    first, second = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    matrix = np.stack((first, second, np.broadcast_to(-direction, first.shape)), axis=2)
    valid = abs(np.linalg.det(matrix)) > 1e-12
    indices = np.flatnonzero(valid)
    values = np.linalg.solve(matrix[valid], (origin - triangles[valid, 0])[..., None])[
        ..., 0
    ]
    u, v, t = values.T
    keep = (u >= -1e-10) & (v >= -1e-10) & (u + v <= 1 + 1e-10) & (t >= 0)
    chosen = np.flatnonzero(keep)[np.argmin(t[keep])]
    return origin + direction * t[chosen], int(mesh["roles"][indices[chosen]])


class RecessedFacades(unittest.TestCase):
    def setUp(self):
        self.plan = RecessPlan(FacadeRepeat(12.0, 9.0, 4, 3, 0.48, 0.53))

    def test_projected_material_areas_cavity_volume_and_normals(self):
        plan = self.plan
        mesh = recess_surface(plan)
        triangles = mesh["vertices"][mesh["faces"]]
        cross = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        projected = cross[:, 2] / 2
        self.assertTrue(np.all(projected >= 0))
        np.testing.assert_allclose(
            mesh["normals"][mesh["faces"]],
            np.repeat(
                (cross / np.linalg.norm(cross, axis=1)[:, None])[:, None, :], 3, axis=1
            ),
            atol=1e-12,
        )
        width, height = np.array(plan.repeat.cell_size_m) * [0.48, 0.53]
        outer, inner = width * height, (width - 0.12) * (height - 0.12)
        areas = [projected[mesh["roles"] == role].sum() for role in (0, 1, 2)]
        np.testing.assert_allclose(
            areas, [108 - 12 * outer, 12 * inner, 12 * (outer - inner)], atol=1e-12
        )
        volume = -(projected * triangles[:, :, 2].mean(axis=1)).sum()
        self.assertAlmostEqual(volume, 12 * (outer * 0.04 + inner * 0.14), places=12)

    def test_real_ray_depths_and_oblique_inner_reveal(self):
        mesh = recess_surface(self.plan, (0, 0, 1, 1))
        for xy, depth, role in (
            ((0.1, 0.1), 0, 0),
            ((1.5, 1.5), 0.18, 1),
            ((0.81, 1.5), 0.04, 2),
        ):
            point, actual = ray(mesh, np.array([*xy, 1.0]), np.array([0.0, 0.0, -1.0]))
            self.assertAlmostEqual(point[2], -depth, places=12)
            self.assertEqual(actual, role)
        inner_right = (1.5 + 1.44 / 2) - 0.06
        direction = np.array([0.8, 0.0, -0.6])
        front = np.array([inner_right - 0.12, 1.5, 0.0])
        point, role = ray(mesh, front - direction, direction)
        self.assertAlmostEqual(point[2], -0.09, places=12)
        self.assertEqual(role, 2)

    def test_batch_matches_scalar_and_partial_global_cells_replay_exactly(self):
        full = recess_surface(self.plan)
        scalar = recess_surface(self.plan, method="scalar")
        for key in full:
            np.testing.assert_array_equal(full[key], scalar[key])
        part = recess_surface(self.plan, (2, 1, 4, 3))
        selected = (full["cells"][:, 0] >= 2) & (full["cells"][:, 1] >= 1)
        for key in ("vertices", "uv", "normals"):
            np.testing.assert_array_equal(
                full[key][full["faces"][selected]], part[key][part["faces"]]
            )
        np.testing.assert_array_equal(full["cells"][selected], part["cells"])

    def test_capacity_rejected_before_template_or_full_grid_allocation(self):
        with patch(
            "outshine_ref.buildings.recessed_facades.cell_template",
            side_effect=AssertionError("expanded"),
        ):
            with self.assertRaisesRegex(ValueError, "capacity"):
                recess_surface(self.plan, max_cells=11)
            huge = RecessPlan(
                FacadeRepeat(3.0 * (2**32 - 1), 3.0, 2**32 - 1, 1, 0.48, 0.53)
            )
            with self.assertRaisesRegex(ValueError, "capacity"):
                recess_surface(huge, max_cells=2**32)
        empty = recess_surface(huge, (2**32 - 1, 0, 2**32 - 1, 1), max_cells=0)
        self.assertEqual(empty["faces"].shape, (0, 3))
        self.assertEqual(empty["vertices"].shape, (0, 3))

    def test_frame_and_cell_bounds_are_explicit_validated_inputs(self):
        for changes in (
            {"depth_m": 0.02},
            {"frame_m": (3.0, 3.0)},
            {"frame_m": [0.06, 0.06]},
            {"depth_m": float("nan")},
            {"front_step_m": True},
        ):
            with self.assertRaises(ValueError):
                RecessPlan(self.plan.repeat, **changes)
        for bounds in ((-1, 0, 1, 1), (0, 0, 5, 1), (0, 2, 1, 1), (0, 0, 1.0, 1)):
            with self.assertRaises(ValueError):
                recess_surface(self.plan, bounds)


if __name__ == "__main__":
    unittest.main()
