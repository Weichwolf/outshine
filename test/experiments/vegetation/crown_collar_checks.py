"""Analytic chord bounds, complete demand preflight and identical field samplers."""

import unittest
from itertools import pairwise
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from collar_union_product import add_collars
from crown_collar_accuracy import first_exits
from crown_collars import chord_profile_bound, demanded_collars, interval_branch
from crown_union_checks import cube
from outshine_ref.vegetation import CubicSpine
from scipy.interpolate import PchipInterpolator
from spatial_crown_replay import check_mesh


def fixture():
    return SimpleNamespace(
        positions=np.array(
            (
                (0, 0, 0),
                (0, 0, 1),
                (0, 0, 2),
                (1, 0, 2),
                (0, 0, 3),
                (2, 0, 3),
                (1, 0, 3.3),
                (0, 0, 4),
            ),
            float,
        ),
        parents=np.array((-1, 0, 1, 1, 2, 3, 2, 4), np.int32),
    )


class CrownCollarChecks(unittest.TestCase):
    def test_independent_first_exit_matches_analytic_sphere(self):
        sphere = SimpleNamespace(
            origin=np.zeros(3),
            ray_extent=0.5,
            evaluate=lambda p: 0.3**2 - np.sum(p**2, axis=1),
        )
        directions = np.eye(3)
        for steps in (256, 512):
            np.testing.assert_allclose(
                first_exits(sphere, directions, steps), 0.3, atol=1e-14
            )

    def test_empty_union_and_rejected_normals_precede_kernel_work(self):
        mesh = cube((0, 0, 0), 1)
        mesh = {key: mesh[key] for key in ("vertices", "faces", "normals")}
        invalid = mesh | {"normals": -mesh["normals"]}
        with patch(
            "collar_union_product.manifold.Mesh64",
            side_effect=AssertionError("kernel on absent or invalid demand"),
        ):
            result, work = add_collars(mesh, (), ())
            self.assertIs(result, mesh)
            self.assertEqual(work["union_s"], 0)
            with self.assertRaises(ValueError):
                add_collars(mesh, (invalid,), (1,))

    def test_chord_bound_matches_exact_parabola_and_linear_radius(self):
        spine = CubicSpine.from_hermite(
            np.array(((0, 0, 0), (0.1, 1, 0))), np.array(((0, 1, 0), (0.2, 1, 0)))
        )
        profile = PchipInterpolator((0, 1), (0.04, 0.02))
        t = np.linspace(0, 1, 9)
        expected = 0.2 / (8 * 8**2)
        self.assertAlmostEqual(
            chord_profile_bound(spine, profile, t, 1), expected, places=15
        )
        middle = (t[1:] + t[:-1]) / 2
        delta = np.linalg.norm(
            spine.evaluate(middle)
            - (spine.evaluate(t[1:]) + spine.evaluate(t[:-1])) / 2,
            axis=1,
        )
        np.testing.assert_allclose(delta, expected, atol=1e-15)

    def test_piecewise_curve_radius_bound_and_adaptive_refusal(self):
        crown = fixture()
        radii = np.linspace(0.08, 0.02, len(crown.parents))
        branch, plan = interval_branch(
            crown, radii, (0, 1, 3, 5), 0.3, 2.4, samples=3, curve_tolerance_m=0.001
        )
        self.assertLessEqual(plan["centre_plus_radius_chord_bound_m"], 0.001)
        self.assertLessEqual(len(branch.points), 66)
        from limb_surface import path_recipe

        spine, profile = path_recipe(crown.positions, radii, (0, 1, 3, 5))
        t = np.asarray(plan["parameter"])
        dense = np.unique(
            np.concatenate([np.linspace(a, b, 41) for a, b in pairwise(t)])
        )
        linear = np.column_stack(
            [np.interp(dense, t, np.asarray(branch.points)[:, j]) for j in range(3)]
        )
        radius = np.interp(dense, t, branch.radii)
        error = np.linalg.norm(spine.evaluate(dense) - linear, axis=1)
        error += abs(profile(dense) * plan["shrink"] - radius)
        self.assertLessEqual(
            error.max(), plan["centre_plus_radius_chord_bound_m"] + 1e-14
        )
        with self.assertRaises(ValueError):
            interval_branch(
                crown,
                radii,
                (0, 1, 3, 5),
                0.3,
                2.4,
                samples=65,
                curve_tolerance_m=1e-10,
            )

    def test_zero_and_invalid_demand_before_plan_io_or_surface_work(self):
        with patch(
            "crown_collars.restore", side_effect=AssertionError("plan on zero demand")
        ):
            self.assertEqual(demanded_collars(Path("absent"), ()), ([], []))
            for nodes in ((1, 1), tuple(range(1, 10)), (0,), (True,)):
                with self.assertRaises(ValueError):
                    demanded_collars(Path("absent"), nodes)
            with self.assertRaises(ValueError):
                demanded_collars(Path("absent"), (1,), cells=15)
            with self.assertRaises(ValueError):
                demanded_collars(Path("absent"), (1,), cells={})
        with (
            patch("crown_collars.restore", return_value=fixture()),
            patch(
                "crown_collars.blocked_voxel_mesh",
                side_effect=AssertionError("partial field"),
            ),
            self.assertRaises(ValueError),
        ):
            demanded_collars(Path("absent"), (1, 5))

    def test_dense_blocked_reverse_and_subset_are_exact_same_local_products(self):
        with patch("crown_collars.restore", return_value=fixture()):
            dense, dp = demanded_collars(
                Path("fixture"), (1, 2), cells=24, method="dense"
            )
            blocked, bp = demanded_collars(
                Path("fixture"), (2, 1), cells={"1": 24, "2": 24}
            )
            subset, _ = demanded_collars(Path("fixture"), (2,), cells=24)
        self.assertEqual([p["fork_node"] for p in bp], [1, 2])
        for index, (left, right) in enumerate(zip(dense, blocked)):
            for key in left:
                np.testing.assert_array_equal(left[key], right[key])
            check_mesh(right)
            self.assertLessEqual(
                bp[index]["field_point_evaluations"],
                dp[index]["field_point_evaluations"],
            )
        for key in subset[0]:
            np.testing.assert_array_equal(subset[0][key], blocked[1][key])


if __name__ == "__main__":
    unittest.main()
