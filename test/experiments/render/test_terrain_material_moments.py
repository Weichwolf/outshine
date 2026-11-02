"""Sector identity, additive filtering and independent joint lighting checks."""

import unittest
from unittest.mock import Mock

import numpy as np
from relief_shading_reference import direction, normals, roughened, specular
from terrain_material_filter import response as reference
from terrain_material_moments import accumulate, response, sample
from terrain_material_moments import pyramid as moment_pyramid
from terrain_material_pyramid import bake


class MaterialMomentTests(unittest.TestCase):
    def test_constant_material_keeps_brdf_and_colour(self):
        raw = np.tile([0.3, 0.4, 0.1, 0.2, 0.7, -0.4], (8, 1))
        weights = np.full(8, 1 / 8)
        lights, view = np.array([direction(0), direction(80, 170)]), direction(50)
        np.testing.assert_allclose(
            response(accumulate(raw, weights), lights, view),
            reference(raw, weights, lights, view),
            rtol=1e-12,
            atol=1e-15,
        )

    def test_filtering_is_linear_without_lobe_reassignment(self):
        raw = np.array([[1, 0, 0, 0.1, 1, 2], [0, 0, 1, 0.9, -2, -1]], dtype=float)
        a, b = [accumulate(row[None], np.ones(1)) for row in raw]
        np.testing.assert_allclose(
            0.3 * a + 0.7 * b, accumulate(raw, np.array([0.3, 0.7])), atol=1e-16
        )
        self.assertEqual(np.flatnonzero(a[:, 0]).tolist(), [3])
        self.assertEqual(np.flatnonzero(b[:, 0]).tolist(), [0])

    def test_colour_normal_moment_preserves_correlated_diffuse(self):
        raw = np.array([[1, 0, 0, 0.5, 0.1, 0.2], [0, 0, 1, 0.5, 1, 0.3]], dtype=float)
        weights, light, view = np.array([0.5, 0.5]), direction(0), direction(0)
        moments = accumulate(raw, weights)
        mean = weights @ normals(raw[:, 4:])
        n = mean / np.linalg.norm(mean)
        roughness = roughened(0.5, np.linalg.norm(mean))
        spec = specular(n[None], roughness, light, view)[0]
        expected = 0.96 * weights @ (raw[:, :3] * normals(raw[:, 4:])[:, 2:]) / np.pi
        np.testing.assert_allclose(
            response(moments, light[None], view)[0] - spec, expected, atol=1e-15
        )
        self.assertGreater(expected[0], expected[2])

    def test_mixed_horizon_is_not_claimed_exact(self):
        raw = np.array([[1, 0, 0, 1, 0.1, 0.1], [1, 0, 0, 1, 4, 0.1]], dtype=float)
        weights, light, view = np.array([0.5, 0.5]), direction(80), direction(0)
        actual = response(accumulate(raw, weights), light[None], view)
        expected = reference(raw, weights, light[None], view)
        self.assertGreater(float(np.max(np.abs(actual - expected))), 0.005)

    def test_rejects_missing_coverage_nonfinite_and_negative_mass(self):
        raw = np.array([[0.2, 0.3, 0.4, 0.5, 0, 0]])
        for weights in ([0.5], [-1], [np.nan], []):
            with self.assertRaises(ValueError):
                accumulate(raw, weights)
        for moment in (np.zeros((4, 14)), np.full((4, 14), np.nan)):
            with self.assertRaises(ValueError):
                response(moment, np.array([direction(0)]), direction(0))

    def test_static_mips_preserve_mass_and_filter_without_generation(self):
        evaluate = Mock(
            side_effect=lambda at, dx, dy: np.column_stack(
                (
                    at[:, 0] < 0.5,
                    np.zeros(len(at)),
                    at[:, 0] >= 0.5,
                    np.full(len(at), 0.6),
                    dx[:, 0] * np.where(at[:, 0] < 0.5, 0.2, -0.4),
                    dy[:, 0] * np.where(at[:, 0] < 0.5, 0.2, -0.4),
                )
            )
        )
        source = bake(evaluate, [0, 0], 1, 8)
        levels = moment_pyramid(source)
        expected = accumulate(
            source.levels[0].fields[:, :, 0].reshape(-1, 6), np.full(64, 1 / 64)
        )
        np.testing.assert_allclose(levels[-1][0, 0], expected, atol=3e-8)
        for level in levels:
            np.testing.assert_allclose(level[..., 0].sum(axis=-1), 1, atol=1e-7)
            with self.assertRaises(ValueError):
                level.flags.writeable = True
        query = np.array([[0.51, 0.49, 0.2, 0, 0, 0.1, 0, 0]])
        for lod in (0, 0.4, 1, 1.99, 2, 3):
            query[:, 6] = lod
            result = sample(levels, query)[0]
            self.assertAlmostEqual(float(result[:, 0].sum()), 1)
        query[:, 6] = 1.37
        actual = sample(levels, query)
        query[:, 6] = 1
        a = sample(levels, query)
        query[:, 6] = 2
        b = sample(levels, query)
        np.testing.assert_allclose(actual, 0.63 * a + 0.37 * b, atol=1e-15)
        evaluate.assert_called_once()

    def test_filter_rejects_unknown_footprints_and_missing_stencils(self):
        levels = (np.zeros((8, 8, 4, 14)),)
        for query in (
            [0.5, 0.5, 0.2, 0, 0, 0.1, 1, 0],
            [0.5, 0.5, 0.2, 0, 0.4, 0, 0, 0],
            [0, 0.5, 0.2, 0, 0, 0.1, 0, 0],
            [0.01, 0.5, 0.001, 0, 0, 0.001, 0, 0],
            [np.nan, 0.5, 0.2, 0, 0, 0.1, 0, 0],
        ):
            with self.assertRaises(ValueError):
                sample(levels, [query])


if __name__ == "__main__":
    unittest.main()
