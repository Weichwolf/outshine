"""Light support geometry, finite measure and unchanged current BRDF guard controls."""

import unittest

import numpy as np
from relief_finite import kernel_quadrature
from relief_finite_kernel import finite_response, footprint, kernel_moments, transform
from relief_lit_kernel import lit_polygon, response
from relief_shading import cases
from relief_shading_reference import direction


class LitKernel(unittest.TestCase):
    def setUp(self):
        self.square = np.array([[-1.0, -1], [1, -1], [1, 1], [-1, 1]])

    def test_exact_half_plane_and_complement_preserve_measure(self):
        light = np.array([1.0, 0, 0.2])
        light /= np.linalg.norm(light)
        lit, state = lit_polygon(self.square, light)
        self.assertEqual(state, "partial")
        self.assertAlmostEqual(lit[:, 0].max(), 0.2, places=14)
        unlit, _ = lit_polygon(self.square, -light)
        transformed = transform(0.5, light[None], np.array([-light[0], 0, light[2]]))
        mass, centre = kernel_moments(self.square, transformed)
        a, ca = kernel_moments(lit, transformed)
        b, cb = kernel_moments(unlit, transformed)
        np.testing.assert_allclose(a + b, mass, atol=1e-12, rtol=0)
        np.testing.assert_allclose(
            a[:, None] * ca + b[:, None] * cb,
            mass[:, None] * centre,
            atol=1e-12,
            rtol=0,
        )
        expected = np.array([[-1.0, -1], [0.2, -1], [0.2, 1], [-1, 1]])
        half = transformed[-1]
        low = kernel_quadrature(expected, 0.5, half, 128)
        high = kernel_quadrature(expected, 0.5, half, 256)
        np.testing.assert_allclose(low, high, atol=2e-11, rtol=0)
        np.testing.assert_allclose(a, high[:, 0], atol=2e-11, rtol=0)
        np.testing.assert_allclose(a[:, None] * ca, high[:, 1:], atol=2e-11, rtol=0)

    def test_full_and_empty_support_have_no_new_partial_geometry(self):
        kept, state = lit_polygon(self.square, np.array([0.0, 0, 1]))
        self.assertIs(kept, self.square)
        self.assertEqual(state, "full")
        empty, state = lit_polygon(self.square, np.array([0.0, 0, -1]))
        self.assertEqual((empty.shape, state), ((0, 2), "empty"))
        model = (np.zeros((1, 2)), np.zeros(1), [(self.square + [3, 0], 0.25)], {})
        value, work = response(
            model, 0.5, np.array([[1.0, 0, 0]]), np.array([0.0, 0, 1])
        )
        np.testing.assert_array_equal(value, [0])
        self.assertEqual(work.get("residual_brdf_evaluations", 0), 0)

    def test_all_lit_support_matches_original_estimator(self):
        polygon = self.square * 0.1
        model = (np.array([[0.0, 0]]), np.array([0.5]), [(polygon, 12.5)], {})
        lights = np.array([direction(x) for x in (-30, 0, 30)])
        view = direction(50)
        actual, work = response(model, 0.5, lights, view)
        np.testing.assert_allclose(
            actual, finite_response(model, 0.5, lights, view), atol=1e-12, rtol=1e-11
        )
        self.assertEqual(work.get("partial", 0), 0)

    def test_tangent_axis_exchange_preserves_response(self):
        polygon = np.array([[-0.7, -0.4], [0.8, -0.4], [0.8, 0.9], [-0.7, 0.9]])
        model = (np.zeros((1, 2)), np.zeros(1), [(polygon, 1.0)], {})
        lights = np.array([direction(a, 35) for a in (-75, 15, 70)])
        view = direction(50)
        value, _ = response(model, 0.9, lights, view)
        swapped = (model[0], model[1], [(polygon[::-1, ::-1], 1.0)], {})
        other, _ = response(swapped, 0.9, lights[:, [1, 0, 2]], view[[1, 0, 2]])
        np.testing.assert_allclose(value, other, atol=1e-12, rtol=1e-10)

    def test_negative_view_dot_is_not_silently_culled(self):
        model = (np.array([[2.0, 0]]), np.ones(1), [], {})
        lights, view = np.array([direction(-45)]), direction(45)
        value, _ = response(model, 0.5, lights, view)
        self.assertGreater(value[0], 0)
        np.testing.assert_array_equal(value, finite_response(model, 0.5, lights, view))

    def test_worst_fully_lit_corner_retains_mixed_view_guard(self):
        case = next(c for c in cases() if c["name"] == "sheared-corner-0.003")
        model = footprint(case, 1)
        light, view = direction(-35, 90), direction(50)
        vertices = model[2][0][0]
        _, state = lit_polygon(vertices, light)
        self.assertEqual(state, "full")
        raw_view = view[2] - vertices @ view[:2]
        self.assertLess(raw_view.min(), 0)
        self.assertGreater(raw_view.max(), 0)
        value, work = response(model, 0.2, light[None], view)
        np.testing.assert_array_equal(
            value, finite_response(model, 0.2, light[None], view)
        )
        self.assertEqual(work.get("partial", 0), 0)


if __name__ == "__main__":
    unittest.main()
