"""Physical path geometry and independently evaluated fixed-screen error changes."""

import unittest

import numpy as np
from building_motion_reference import (
    motion_bounds,
    residual_change,
    residuals,
    trajectory,
)
from current_visibility import project


class BuildingMotionReference(unittest.TestCase):
    def test_physical_velocity_midpoint_and_fixed_basis(self):
        basis = np.array([[0, 1, 0], [0, 0, 1], [-1, 0, 0.0]])
        camera = (np.array([50, 60, 70.0]), basis, 900, 1280, 720)
        times, cameras = trajectory(camera, [300, 100, 200], 1, 9)
        np.testing.assert_array_equal(cameras[4][0], camera[0])
        velocity = (
            np.diff(np.array([c[0] for c in cameras]), axis=0) / np.diff(times)[:, None]
        )
        np.testing.assert_array_equal(velocity, np.tile([-200, 300, 100], (8, 1)))
        for at in cameras:
            self.assertIs(at[1], basis)
        for args in (
            ([0, float("nan"), 0], 1, 9),
            ([0, 0, 0], 0, 9),
            ([0, 0, 0], 1, 1),
        ):
            with self.assertRaises(ValueError):
                trajectory(camera, *args)
        with self.assertRaises(ValueError):
            trajectory((camera[0], basis * 2, *camera[2:]), [0, 0, 0], 1, 9)

    def test_one_roi_contains_independently_projected_motion_endpoints(self):
        camera = (np.zeros(3), np.diag([-1, 1, 1]), 100, 128, 72)
        _, cameras = trajectory(camera, [3, 1, 2], 1, 9)
        points = np.array([[-1, -1, 10], [1, 1, 12.0]])
        box = motion_bounds(points, cameras)
        for at in cameras:
            xy, _ = project(points, at)
            self.assertTrue(np.all(xy[:, :2] >= box[0]))
            self.assertTrue(np.all(xy[:, :2] <= box[1]))

    def test_reference_motion_cancels_but_changing_residual_survives(self):
        reference0 = {"C": np.zeros((1, 1, 3)), "T": np.ones((1, 1, 1))}
        reference1 = {"C": np.ones((1, 1, 3)) * 4, "T": np.zeros((1, 1, 1))}
        r0 = residuals(
            {
                "original": reference0,
                "candidate": {"C": reference0["C"] + 0.5, "T": reference0["T"]},
            }
        )
        r1 = residuals(
            {
                "original": reference1,
                "candidate": {"C": reference1["C"] + 0.5, "T": reference1["T"]},
            }
        )
        np.testing.assert_allclose(r0["candidate"]["RGB"], 0.5, atol=1e-15)
        self.assertLess(
            residual_change(r0, r1, 0.125)["candidate"]["RGB_residual_delta_l1"], 1e-14
        )
        r1["candidate"]["RGB"] += 0.25
        r1["candidate"]["T"] += 0.5
        result = residual_change(r0, r1, 0.125)["candidate"]
        self.assertAlmostEqual(result["RGB_residual_delta_l1"], 0.75)
        self.assertAlmostEqual(result["T_residual_delta_l1"], 0.5)
        with self.assertRaises(ValueError):
            residual_change(r0, r1, 0)
        r1["candidate"]["T"] = np.zeros((2, 2, 1))
        with self.assertRaises(ValueError):
            residual_change(r0, r1, 0.125)


if __name__ == "__main__":
    unittest.main()
