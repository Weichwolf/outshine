"""Complete fixed-orientation framing checked against independent linear programming."""

import unittest

import numpy as np
from scipy.optimize import linprog
from showcase_tiles import camera, frame_proof


def source_bounds():
    return np.asarray(
        [
            (x, y, z)
            for bounds in (
                (-530, -180, -260, 330, 0, 12),
                (70, -620, 95, -590, 0, 168),
                (420, 40, 660, 100, 0, 5),
            )
            for x in (bounds[0], bounds[2])
            for y in (bounds[1], bounds[3])
            for z in (bounds[4], bounds[5])
        ],
        dtype=float,
    )


def minimum_depth(points, view):
    origin = points.min(axis=0) / 2 + points.max(axis=0) / 2
    q = (points - origin) @ view.basis.T
    x, y = np.array([view.width * 0.46, view.height * 0.43]) / view.focal_px
    coefficients = np.tile(
        [[-x, -1.0, 0.0], [-x, 1.0, 0.0], [-y, 0.0, -1.0], [-y, 0.0, 1.0]], (len(q), 1)
    )
    rhs = np.c_[
        x * q[:, 2] - q[:, 0],
        x * q[:, 2] + q[:, 0],
        y * q[:, 2] - q[:, 1],
        y * q[:, 2] + q[:, 1],
    ].ravel()
    coefficients = np.r_[coefficients, [[-1.0, 0.0, 0.0]]]
    rhs = np.r_[rhs, q[:, 2].min() - 0.1]
    result = linprog(
        [1.0, 0.0, 0.0],
        A_ub=coefficients,
        b_ub=rhs,
        bounds=[(0.1, None), (None, None), (None, None)],
    )
    if not result.success:
        raise AssertionError("Independent framing LP failed: " + result.message)
    return result.fun


class CompleteTileCamera(unittest.TestCase):
    def test_asymmetric_sources_fit_and_match_the_fixed_projection_lower_bound(self):
        points = source_bounds()
        view = camera(points)
        direction = np.array([1.0, -1.25, 0.95])
        direction /= np.linalg.norm(direction)
        np.testing.assert_allclose(-view.basis[2], direction, atol=1e-13, rtol=0)
        required = minimum_depth(points, view)
        self.assertAlmostEqual(
            np.linalg.norm(view.eye - view.target) - 10.0, required, places=9
        )
        proof = frame_proof({"vertices": points}, view)
        self.assertTrue(proof["all_vertices_inside"])
        self.assertEqual((view.width, view.height, view.fov_deg), (3840, 2160, 48.0))

    def test_translation_preserves_the_complete_framing_and_metric_depth(self):
        points = source_bounds()
        a = camera(points)
        offset = np.array([1e6, -2e6, 300.0])
        b = camera(points + offset)
        np.testing.assert_allclose(b.eye - offset, a.eye, atol=1e-7, rtol=0)
        np.testing.assert_allclose(b.target - offset, a.target, atol=1e-7, rtol=0)
        self.assertTrue(
            frame_proof({"vertices": points + offset}, b)["all_vertices_inside"]
        )

    def test_planar_linear_and_point_inputs_keep_positive_depth(self):
        for points in (
            np.array([[2.0, 3.0, 4.0]]),
            np.array([[0.0, 0.0, 0.0], [100.0, 0.0, 0.0]]),
            np.array([[0.0, 0.0, 0.0], [100.0, 0.0, 0.0], [0.0, 50.0, 0.0]]),
        ):
            view = camera(points)
            self.assertGreaterEqual(
                np.linalg.norm(view.eye - view.target), 10.1 - 1e-12
            )
            self.assertTrue(
                frame_proof({"vertices": points}, view)["all_vertices_inside"]
            )

    def test_missing_dimensions_empty_and_nonfinite_bounds_are_rejected(self):
        for points in (
            [],
            [[0.0, 1.0]],
            [[0.0, float("nan"), 0.0]],
            [[0.0, 0.0, float("inf")]],
            [0.0, 1.0, 2.0],
        ):
            with self.assertRaises(ValueError):
                camera(points)


if __name__ == "__main__":
    unittest.main()
