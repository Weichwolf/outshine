"""Whole-box bounds against the existing independent rational projection oracle."""

import unittest
from fractions import Fraction as Q
from itertools import product

import numpy as np
from capture_box_envelope import box_envelope, conditionally_inside
from capture_box_reference import corner_envelope, submitted_inputs, typed_projection
from vertex_projection.verify_envelope import (
    Arithmetic,
    expanded,
    family_limits,
    inside,
    precomposed,
    rational_bound,
)


def case_at(model, view, shift, point, clip):
    return {
        "model": model.ravel(order="F").astype(float).tolist(),
        "view": view.ravel(order="F").astype(float).tolist(),
        "shift": [*map(float, shift), 0.0],
        "point": [*map(float, point), 1.0],
        "clip": clip,
    }


class BoxEnvelopeTests(unittest.TestCase):
    def test_submitted_a18_carriers_and_complete_stream_are_enclosed(self):
        manifest, arrays, operands = submitted_inputs()
        positions = arrays["vertex_positions_f32"]
        self.assertEqual(len(positions), 94999)
        low, high = (
            positions.min(0, keepdims=True).astype(float),
            positions.max(0, keepdims=True).astype(float),
        )
        result = box_envelope(low, high, *operands, positions_normal_or_zero=True)
        for flat in (False, True):
            projected = typed_projection(positions, *operands, flat=flat)
            self.assertTrue((projected >= result["lower"]).all())
            self.assertTrue((projected <= result["upper"]).all())
        self.assertTrue(conditionally_inside(result)[0])
        self.assertFalse(result["producer_expression_family_qualified"])
        self.assertTrue(manifest["checks"]["colour_equals_frozen_original_capture"])

    def test_corner_opponent_keeps_position_error_correlation(self):
        model = view = np.eye(4, dtype=np.float32)
        low, high = np.array([[-100.0, -0.5, 0.25]]), np.array([[1.0, 0.5, 0.75]])
        args = (low, high, model, view, np.zeros(3, np.float32))
        whole = box_envelope(*args, positions_normal_or_zero=True)
        corners = corner_envelope(*args, positions_normal_or_zero=True)
        self.assertGreater(whole["upper"][0, 0] - corners["upper"][0, 0], 0.0002)
        for point in product(*zip(low[0], high[0], strict=True)):
            for result in (whole, corners):
                clip = list(zip(result["lower"][0], result["upper"][0], strict=True))
                rational_bound(
                    case_at(model, view, np.zeros(3, np.float32), point, clip)
                )

    def test_exact_point_bound_and_evaluations_fit_entire_box(self):
        rng = np.random.default_rng(1724)
        model = rng.uniform(-2, 2, (4, 4)).astype(np.float32)
        view = rng.uniform(-2, 2, (4, 4)).astype(np.float32)
        shift = np.array([32000.1, -45000.2, -32000.3], np.float32)
        low, high = np.array([[-20.0, -4, 10]]), np.array([[31.0, 12, 80]])
        bounds = box_envelope(
            low, high, model, view, shift, positions_normal_or_zero=True
        )
        clip = list(zip(bounds["lower"][0], bounds["upper"][0], strict=True))
        points = [
            *product(*zip(low[0], high[0], strict=True)),
            *rng.uniform(low[0], high[0], (5, 3)).astype(np.float32),
        ]
        for point in points:
            case = case_at(model, view, shift, point, clip)
            rational_bound(case)
            for mode in ("nearest", "zero", "down", "up"):
                for flush in (False, True):
                    arithmetic = Arithmetic(mode, flush)
                    self.assertTrue(inside(clip, expanded(case, arithmetic, 1724)))
                    self.assertTrue(inside(clip, precomposed(case, arithmetic)))

    def test_inverse_transforms_do_not_expand_the_ideal_box_twice(self):
        model = np.array(
            [[1, 1, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]], np.float32
        )
        view = np.array(
            [[1, -1, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]], np.float32
        )
        low, high = np.array([[-1.0, -1, 0.25]]), np.array([[1.0, 1, 0.75]])
        result = box_envelope(
            low,
            high,
            model,
            view,
            np.zeros(3, np.float32),
            positions_normal_or_zero=True,
        )
        self.assertLess(result["ideal_upper"][0, 0], 1.00000000001)
        self.assertGreater(result["ideal_lower"][0, 0], -1.00000000001)
        self.assertGreater(result["error_upper"][0, 0], 0)

    def test_identity_inside_is_conditional_and_uncertain_w_stays_out(self):
        model = view = np.eye(4, dtype=np.float32)
        low, high = [
            a.astype(np.float32).astype(float)
            for a in (np.array([[-0.5, -0.5, 0.2]]), np.array([[0.5, 0.5, 0.8]]))
        ]
        result = box_envelope(
            low,
            high,
            model,
            view,
            np.zeros(3, np.float32),
            positions_normal_or_zero=True,
        )
        self.assertTrue(conditionally_inside(result)[0])
        self.assertFalse(result["producer_expression_family_qualified"])
        self.assertFalse(result["raster_qualified"])
        view = view.copy()
        view[3] = [0, 0, 1, 0]
        low[0, 2], high[0, 2] = -1, 1
        result = box_envelope(
            low,
            high,
            model,
            view,
            np.zeros(3, np.float32),
            positions_normal_or_zero=True,
        )
        self.assertFalse(conditionally_inside(result)[0])

    def test_input_carrier_and_family_domain_are_not_inferred(self):
        a, b = np.array([[0.0, 0, 0.25]]), np.array([[1.0, 1, 0.75]])
        model = view = np.eye(4, dtype=np.float32)
        shift = np.zeros(3, np.float32)
        for m, v, s, flag in (
            (model.astype(float), view, shift, True),
            (model, view, shift, False),
            (model, view, np.array([1e-40, 0, 0], np.float32), True),
        ):
            with self.assertRaises(ValueError):
                box_envelope(a, b, m, v, s, positions_normal_or_zero=flag)
        with self.assertRaises(ValueError):
            box_envelope(
                a + 1e-10, b, model, view, shift, positions_normal_or_zero=True
            )
        large = model.copy()
        large[0, 0] = 1e20
        with self.assertRaisesRegex(ValueError, "overflow"):
            box_envelope(a, b, large, view, shift, positions_normal_or_zero=True)

    def test_existing_family_limits_and_intermediate_underflow(self):
        gamma, growth, noise = family_limits(Q(1))
        self.assertEqual(gamma, Q(21, 2**23 - 21))
        self.assertEqual(growth, Q(2**23, 2**23 - 59))
        self.assertEqual(noise, 238**3 * Q(1, 2**126) * growth)
        model = np.eye(4, dtype=np.float32)
        view = model.copy()
        model[0, 0] = np.float32(2**-100)
        view[0, 0] = np.float32(2**-100)
        point = np.array([[1.0, 0, 0.5]])
        result = box_envelope(
            point,
            point,
            model,
            view,
            np.zeros(3, np.float32),
            positions_normal_or_zero=True,
        )
        clip = list(zip(result["lower"][0], result["upper"][0], strict=True))
        case = case_at(model, view, np.zeros(3, np.float32), point[0], clip)
        rational_bound(case)
        self.assertTrue(inside(clip, expanded(case, Arithmetic("nearest", True), 990)))


if __name__ == "__main__":
    unittest.main()
