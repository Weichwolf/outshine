"""Independent spatial integration and joint material/filter counterexamples."""

import unittest
from functools import partial
from unittest.mock import Mock

import numpy as np
from probe_terrain_material_substrate import inputs, optical_row
from relief_shading_reference import direction, specular
from terrain_material_filter import (
    distribution,
    joint_models,
    moment_responses,
    response,
)
from terrain_material_litter import compose


class MaterialFilterTests(unittest.TestCase):
    def test_polynomial_quadrature_on_rotated_anisotropic_pixel(self):
        centre, dx, dy = (
            np.array([3.1, -4.2]),
            np.array([0.8, 0.6]),
            np.array([-0.12, 0.16]),
        )

        def polynomial(at, horizontal, vertical):
            return np.column_stack(
                (
                    at[:, 0] ** 2,
                    at[:, 1] ** 2,
                    at[:, 0] * at[:, 1],
                    np.full(len(at), 0.7),
                    horizontal[:, 0],
                    vertical[:, 0],
                )
            )

        raw, weights = distribution(polynomial, centre, dx, dy, 3)
        second = np.outer(centre, centre) + (np.outer(dx, dx) + np.outer(dy, dy)) / 12
        np.testing.assert_allclose(
            weights @ raw[:, :3], [second[0, 0], second[1, 1], second[0, 1]], atol=1e-14
        )
        np.testing.assert_allclose(raw[:, 4:], np.tile([1, 0], (len(raw), 1)))
        self.assertAlmostEqual(float(weights.sum()), 1)

    def test_constant_material_keeps_all_light_responses(self):
        raw = np.tile([0.3, 0.2, 0.1, 0.7, 0.1, -0.2], (9, 1))
        weights = np.ones(9) / 9
        lights, view = np.array([direction(0), direction(70, 40)]), direction(50)
        expected = response(raw, weights, lights, view)
        for name, result in moment_responses(raw, weights, lights, view).items():
            np.testing.assert_allclose(
                result, expected, rtol=0, atol=2e-16, err_msg=name
            )

    def test_joint_materials_preserve_color_normal_correlation(self):
        raw = np.array([[1, 0, 0, 0.5, 1, 0], [0, 0, 1, 0.5, -1, 0]], dtype=float)
        weights, lights, view = (
            np.array([0.5, 0.5]),
            np.array([direction(60)]),
            direction(0),
        )
        result = response(raw, weights, lights, view)
        self.assertGreater(result[0, 2], result[0, 0])
        mean = moment_responses(raw, weights, lights, view)["mean_gradient"]
        self.assertAlmostEqual(float(mean[0, 0]), float(mean[0, 2]))
        self.assertGreater(float(np.max(np.abs(result - mean))), 0.05)

    def test_single_normal_matches_independent_brdf_arithmetic(self):
        normal, light, view = direction(25, 30), direction(60, -10), direction(70, 10)
        raw = np.array([[0.3, 0.4, 0.5, 0.6, *(-normal[:2] / normal[2])]])
        half = (light + view) / np.linalg.norm(light + view)
        fresnel = 0.04 + 0.96 * (1 - half @ view) ** 5
        expected = (1 - fresnel) * raw[0, :3] * max(
            normal @ light, 0
        ) / np.pi + specular(normal[None], 0.6, light, view)[0]
        np.testing.assert_allclose(
            response(raw, np.ones(1), light[None], view)[0], expected, atol=1e-16
        )

    def test_joint_partition_reuses_correlated_materials_across_views(self):
        raw = np.repeat(
            np.array([[1, 0, 0, 0.4, 1, 0], [0, 0, 1, 0.8, -1, 0]], dtype=float),
            4,
            axis=0,
        )
        weights = np.repeat([0.25, 0.75], 4) / 4
        models = joint_models(raw, weights)
        original = {
            key: (fields.tobytes(), areas.tobytes())
            for key, (fields, areas) in models.items()
        }
        lights = np.array([direction(0), direction(60), direction(80, 90)])
        for name, (fields, areas) in models.items():
            self.assertLessEqual(len(fields), int(name.split("_")[1]))
            self.assertAlmostEqual(float(areas.sum()), 1)
            self.assertTrue(np.all(areas > 0))
            for view in (direction(0), direction(70)):
                if not name.startswith("joint_1_"):
                    np.testing.assert_allclose(
                        response(fields, areas, lights, view),
                        response(raw, weights, lights, view),
                        atol=2e-15,
                    )
        self.assertEqual(
            original,
            {
                key: (fields.tobytes(), areas.tobytes())
                for key, (fields, areas) in models.items()
            },
        )

    def test_uniform_joint_material_stays_one_representative(self):
        raw = np.tile([0.3, 0.2, 0.1, 0.7, 0.1, -0.2], (9, 1))
        for fields, areas in joint_models(raw, np.ones(9) / 9).values():
            self.assertEqual(len(fields), 1)
            np.testing.assert_allclose(fields[0], raw[0], atol=1e-15)
            np.testing.assert_allclose(areas, [1], atol=1e-15)

    def test_joint_statistics_use_shaded_ranges_before_averaging(self):
        raw = np.array([[-1, 2, 0.3, -1, 0, 0], [-2, 3, 0.3, -2, 0, 0]], dtype=float)
        for fields, areas in joint_models(raw, np.ones(2) / 2).values():
            np.testing.assert_allclose(fields, [[0, 1, 0.3, 0.04, 0, 0]], atol=1e-15)
            np.testing.assert_allclose(areas, [1], atol=1e-15)

    def test_point_material_has_no_derivative_step_filtering(self):
        palette, layout = inputs()
        for row in (0, 7, 12):
            evaluate = partial(compose, **optical_row(palette, layout, row))
            first, _ = distribution(
                evaluate,
                [1000.137, -200.271],
                [0.01, 0.003],
                [-0.001, 0.007],
                4,
                derivative_step_m=1e-7,
            )
            second, _ = distribution(
                evaluate,
                [1000.137, -200.271],
                [0.01, 0.003],
                [-0.001, 0.007],
                4,
                derivative_step_m=0.5e-7,
            )
            np.testing.assert_array_equal(first, second)

    def test_bad_admission_does_not_evaluate(self):
        evaluate = Mock()
        for order, dx, dy in (
            (1, [1, 0], [0, 1]),
            (257, [1, 0], [0, 1]),
            (4, [1, 0], [2, 0]),
            (4, [np.nan, 0], [0, 1]),
        ):
            with self.subTest(order=order, dx=dx), self.assertRaises(ValueError):
                distribution(evaluate, [0, 0], dx, dy, order)
        evaluate.assert_not_called()


if __name__ == "__main__":
    unittest.main()
