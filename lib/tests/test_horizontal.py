"""Independent ODE controls, complete corner bounds and original-axis ownership."""

import copy
import unittest

import numpy as np
import shapely
from outshine_ref.transport.clothoid_turns import (
    fit_symmetric_turns,
    sample_symmetric_turns,
)
from outshine_ref.transport.horizontal import plan_horizontal_axis
from scipy.integrate import solve_ivp

RECIPE = {
    "model": "clothoid",
    "radius_m": 30.0,
    "error_m": 0.025,
    "end_guard_m": 8.0,
    "maximum_shift_m": 30.0,
}


class HorizontalCurves(unittest.TestCase):
    def test_clothoid_poses_match_independent_piecewise_curvature_ode(self):
        for angle in (0.02, -0.4, np.pi / 2, -1.62, 2.5):
            fit = fit_symmetric_turns(
                [0, 0],
                np.array([50.0]),
                np.array([0.0]),
                np.array([angle]),
                transition_fraction=0.25,
            )
            length = float(fit["length_m"][0])
            maximum = angle / (0.75 * length)
            stations = np.linspace(0, 1, 129)

            def derivative(s, state, maximum=maximum, length=length):
                kappa = maximum * min(
                    s / (0.25 * length), (length - s) / (0.25 * length), 1
                )
                return [np.cos(state[2]), np.sin(state[2]), kappa]

            state = [*fit["start"][0], 0.0]
            result = solve_ivp(
                derivative,
                [0, length],
                state,
                t_eval=stations * length,
                rtol=1e-11,
                atol=1e-12,
                max_step=length / 256,
            )
            self.assertTrue(result.success)
            xy, heading, curvature = sample_symmetric_turns(fit, stations)
            np.testing.assert_allclose(xy[0], result.y[:2].T, rtol=0, atol=2e-8)
            np.testing.assert_allclose(heading[0], result.y[2], rtol=0, atol=1e-9)
            np.testing.assert_allclose(xy[0, -1], fit["end"][0], rtol=0, atol=1e-8)
            self.assertEqual(curvature[0, 0], 0)
            self.assertEqual(curvature[0, -1], 0)
            self.assertLessEqual(abs(curvature).max(), abs(maximum) + 1e-12)

    def test_known_circle_has_exact_radius_but_nonzero_endpoint_curvature(self):
        original = np.array([[-100, 0], [0, 0], [0, 100]], float)
        recipe = dict(RECIPE, model="circle")
        points, note = plan_horizontal_axis(original, recipe)
        arc = points[1:-1]
        np.testing.assert_allclose(
            np.linalg.norm(arc - [-30, 30], axis=1), 30, rtol=0, atol=1e-12
        )
        self.assertAlmostEqual(note["corners"][0]["length_m"], 15 * np.pi)
        self.assertAlmostEqual(note["corners"][0]["endpoint_curvature_per_m"], 1 / 30)

    def test_model_chord_bound_covers_all_dense_curve_intervals(self):
        original = np.array([[-100, 0], [0, 0], [0, 100]], float)
        points, note = plan_horizontal_axis(original, RECIPE)
        corner = note["corners"][0]
        fit = fit_symmetric_turns(
            [0, 0],
            np.array([corner["trim_m"]]),
            np.array([0.0]),
            np.array([np.pi / 2]),
            transition_fraction=0.25,
        )
        count = corner["segments"]
        dense = sample_symmetric_turns(fit, np.linspace(0, 1, count * 64 + 1))[0][0]
        maximum = 0
        for i in range(count):
            chord = shapely.LineString(points[1 + i : 3 + i])
            error = shapely.distance(
                shapely.points(dense[i * 64 : (i + 1) * 64 + 1]), chord
            )
            maximum = max(maximum, float(error.max()))
        self.assertLessEqual(maximum, corner["chord_bound_m"] + 1e-10)
        self.assertLessEqual(corner["chord_bound_m"], RECIPE["error_m"])

    def test_endpoints_and_original_approach_directions_survive(self):
        original = np.array([[-100, 0], [0, 0], [0, 100]], float)
        points, _note = plan_horizontal_axis(original, RECIPE)
        np.testing.assert_array_equal(points[[0, -1]], original[[0, -1]])
        self.assertAlmostEqual(points[1, 1], 0)
        self.assertAlmostEqual(points[-2, 0], 0)
        self.assertGreater(np.linalg.norm(points[1] - points[0]), RECIPE["end_guard_m"])

    def test_plan_owns_read_only_axis_and_independent_recipe(self):
        original = np.array([[-100, 0], [0, 0], [0, 100]], float)
        recipe = copy.deepcopy(RECIPE)
        result, note = plan_horizontal_axis(original, recipe)
        expected = result.copy()
        original[:] = 42
        recipe["radius_m"] = 900
        np.testing.assert_array_equal(result, expected)
        self.assertEqual(note["recipe"]["radius_m"], 30)
        with self.assertRaises(ValueError):
            result[0] = 0

    def test_fit_owns_caller_heading_and_transition_arrays(self):
        heading, transition = np.array([0.0]), np.array([0.25])
        fit = fit_symmetric_turns(
            [0, 0],
            np.array([50.0]),
            heading,
            np.array([1.0]),
            transition_fraction=transition,
        )
        before = sample_symmetric_turns(fit, [0, 0.5, 1])[0]
        heading[:] = 1
        transition[:] = 0.5
        np.testing.assert_array_equal(
            sample_symmetric_turns(fit, [0, 0.5, 1])[0], before
        )

    def test_straight_redundant_knots_are_geometrically_identical(self):
        original = np.array([[0, 0], [30, 0], [60, 0]], float)
        points, note = plan_horizontal_axis(original, RECIPE)
        self.assertTrue(shapely.LineString(points).equals(shapely.LineString(original)))
        self.assertFalse(note["corners"])

    def test_short_neighbor_segment_is_rejected_without_radius_shrink(self):
        for original in (
            [[0, 0], [12, 0], [12, 100]],
            [[-100, 0], [0, 0], [0, 50], [100, 50]],
        ):
            with self.assertRaisesRegex(ValueError, "insufficient original straight"):
                plan_horizontal_axis(original, RECIPE)

    def test_displacement_and_return_turns_need_separate_recipes(self):
        with self.assertRaisesRegex(ValueError, "allowed original corridor"):
            plan_horizontal_axis(
                [[-100, 0], [0, 0], [0, 100]], dict(RECIPE, maximum_shift_m=0.1)
            )
        with self.assertRaisesRegex(ValueError, "returning road corner"):
            plan_horizontal_axis([[0, 0], [100, 0], [1, 1]], RECIPE)

    def test_invalid_or_excessive_source_and_sampler_work_is_rejected(self):
        for original in (
            [[0, 0], [0, 0]],
            [[0, 0], [np.nan, 1]],
            np.zeros((129, 2)),
            [[1e7, 0], [1e7 + 10, 0]],
            [[0, 0], [100, 0], [100, 100], [0, 0]],
        ):
            with self.assertRaises(ValueError):
                plan_horizontal_axis(original, RECIPE)
        fit = fit_symmetric_turns(
            [0, 0], np.array([50.0]), np.array([0.0]), np.array([1.0])
        )
        for stations in ([-0.1, 1], [0, np.nan], np.zeros(65537)):
            with self.assertRaises(ValueError):
                sample_symmetric_turns(fit, stations)
        with self.assertRaises(ValueError):
            fit_symmetric_turns([0, 0], np.ones(4097), np.zeros(4097), np.ones(4097))


if __name__ == "__main__":
    unittest.main()
