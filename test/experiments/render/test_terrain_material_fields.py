"""Controlled current-rock response and bounded raw-field interpolation checks."""

import unittest
from functools import partial

import numpy as np
from probe_terrain_material_fields import appearance
from rock_direction import value
from terrain_material_grid import bake, sample
from terrain_material_lattice import analytic, evaluate, prepare
from terrain_material_noise import MATRICES, raw_noise, rock_fields, rock_response


def shader_noise(at):
    cell = np.floor(at).astype(np.int64)
    f = at - cell
    slope = 6 * f * (1 - f)
    f = f * f * (3 - 2 * f)
    taps = [
        value((cell + [x, y, z])[None])[0]
        for z in range(2)
        for y in range(2)
        for x in range(2)
    ]
    a0, a1, b0, b1, c0, c1, d0, d1 = taps
    mix = lambda a, b, t: a * (1 - t) + b * t
    a, b, c, d = (mix(taps[i], taps[i + 1], f[0]) for i in (0, 2, 4, 6))
    low, high = mix(a, b, f[1]), mix(c, d, f[1])
    return np.array(
        [
            mix(mix(a1 - a0, b1 - b0, f[1]), mix(c1 - c0, d1 - d0, f[1]), f[2])
            * slope[0],
            mix(b - a, d - c, f[2]) * slope[1],
            (high - low) * slope[2],
            mix(low, high, f[2]) - 0.5,
        ]
    )


def shader_rock(point, dx, dy):
    result = []
    for index, (matrix, wavelength) in enumerate(
        zip(MATRICES, (84.0, 7.0, 1.045), strict=True)
    ):
        tx, ty = matrix @ dx / wavelength, matrix @ dy / wavelength
        square_size = max(tx @ tx, ty @ ty)
        if square_size >= 0.64:
            result.append(np.zeros(3))
            continue
        t = np.clip((np.sqrt(square_size) - 0.25) / 0.55, 0, 1)
        weight = 1 - t * t * (3 - 2 * t)
        field = shader_noise(matrix @ point / wavelength)
        phase = np.array([field[:3] @ tx, field[:3] @ ty])
        if index < 2:
            square = 0.07**2 + phase @ phase / 12
            pulse = 0.07 / np.sqrt(square) * np.exp(-(field[3] ** 2) / (2 * square))
            phase *= pulse * field[3] / square
        result.append(np.r_[phase, field[3]] * weight)
    macro, coarse, fine = result
    return np.r_[
        np.clip(
            1 + 0.6 * (0.4 * macro[2] + 0.5 * coarse[2] + 0.1 * fine[2]), 0.72, 1.28
        ),
        0.14 * macro[2] + 0.06 * coarse[2],
        2.1 * macro[:2] + 0.28 * coarse[:2] + 0.04 * fine[:2],
    ]


class TerrainMaterialFields(unittest.TestCase):
    def test_translation_and_light_changes_reuse_the_same_original_fields(self):
        origin = np.array([1000.13, 1770.31, -200.27])
        basis = np.array([[1, 0], [0.7, -0.3], [0, 1]])
        uv = np.array([[0.5, 0.5], [0.8, 1.2], [2.2, 1.5], [3.5, 3.1]])
        corners = origin + np.array([[0, 0], [4, 0], [0, 4], [4, 4]]) @ basis.T
        lattices = []
        for matrix, wavelength in zip(MATRICES, (84, 7, 1.045), strict=True):
            at = corners @ (matrix / wavelength).T
            lattices.append(prepare(np.stack((at.min(axis=0), at.max(axis=0)))))
        original_words = [g.words_zyx.tobytes() for g in lattices]
        callbacks = tuple(partial(evaluate, g) for g in lattices)
        steps = np.array([0.01, 0.015])
        dx, dy = (
            np.tile(basis[:, 0] * steps[0], (4, 1)),
            np.tile(basis[:, 1] * steps[1], (4, 1)),
        )
        material_responses = []
        for shift in ([0, 0], [0.1, 0.2]):
            world = origin + (uv + shift) @ basis.T
            direct = self.response(
                rock_fields(world, 7, 1.045, noise_queries=(analytic,) * 3), dx, dy
            ).reshape(2, 2, 4)
            lookup = self.response(
                rock_fields(world, 7, 1.045, noise_queries=callbacks), dx, dy
            ).reshape(2, 2, 4)
            self.assertEqual(direct.tobytes(), lookup.tobytes())
            material_responses.append(direct.tobytes())
            lights = []
            for light, view in (
                ([0.5, 0.8, -0.2], [0, 1, 0.6]),
                ([-0.4, 0.8, 0.2], [0.1, 0.9, -0.2]),
            ):
                recipe = {"albedo": [0.3, 0.3, 0.3], "roughness": 0.72}
                expected = appearance(
                    direct, basis, steps, recipe, light=light, view=view
                )
                actual = appearance(
                    lookup, basis, steps, recipe, light=light, view=view
                )
                self.assertEqual(expected[0].tobytes(), actual[0].tobytes())
                self.assertEqual(expected[1].tobytes(), actual[1].tobytes())
                lights.append(actual[1].tobytes())
            self.assertNotEqual(*lights)
        self.assertNotEqual(*material_responses)
        self.assertEqual(original_words, [g.words_zyx.tobytes() for g in lattices])

    def response(self, fields, dx, dy):
        return rock_response(
            fields, dx, dy, coarse_m=7.0, fine_m=1.045, height_m=0.04, contrast=0.6
        )

    def test_raw_reconstruction_matches_independent_current_glsl_algebra(self):
        rng = np.random.default_rng(1684)
        points = rng.uniform(-500, 500, (32, 3))
        fields = rock_fields(points, 7.0, 1.045)
        for sign in (-1, 1):
            for footprint in (0.001, 0.3, 4.0, 1000.0):
                dx = np.tile(np.array([1, sign * 0.7, 0]) * footprint, (32, 1))
                dy = np.tile(np.array([0.2, -sign * 0.3, 1]) * footprint, (32, 1))
                expected = np.array(
                    [
                        shader_rock(p, x, y)
                        for p, x, y in zip(points, dx, dy, strict=True)
                    ]
                )
                np.testing.assert_allclose(
                    self.response(fields, dx, dy), expected, atol=2e-13, rtol=2e-13
                )

    def test_raw_world_gradient_has_the_correct_chain_rule(self):
        points = np.random.default_rng(99).uniform(-10, 10, (64, 3))
        for matrix in MATRICES:
            raw = raw_noise(points, matrix, 1.045)
            for axis in range(3):
                delta = np.eye(3)[axis] * 1e-5
                difference = (
                    raw_noise(points + delta, matrix, 1.045)[:, 0]
                    - raw_noise(points - delta, matrix, 1.045)[:, 0]
                ) / 2e-5
                np.testing.assert_allclose(
                    raw[:, axis + 1], difference, atol=1e-8, rtol=0
                )

    def test_footprint_stays_at_query_time_and_far_response_is_neutral(self):
        points = np.array([[10.1, 1770.3, -22.4]])
        fields = rock_fields(points, 7.0, 1.045)
        copies = [f.tobytes() for f in fields]
        small = self.response(
            fields, np.array([[0.01, 0, 0]]), np.array([[0, 0, 0.01]])
        )
        far = self.response(fields, np.array([[1e5, 0, 0]]), np.array([[0, 0, 1e5]]))
        np.testing.assert_array_equal(far, [[1, 0, 0, 0]])
        self.assertFalse(np.array_equal(small, far))
        self.assertEqual(copies, [f.tobytes() for f in fields])

    def test_raw_mean_cannot_commute_with_crevice_response(self):
        fields = [
            np.array([[0, 0.1, 0, 0], [0.14, 0.1, 0, 0]], dtype=float),
            np.zeros((2, 4)),
            np.zeros((2, 4)),
        ]
        dx, dy = np.tile([0.1, 0, 0], (2, 1)), np.tile([0, 0, 0.1], (2, 1))
        mean_response = self.response(fields, dx, dy).mean(axis=0)
        raw_mean = self.response(
            [f.mean(axis=0, keepdims=True) for f in fields], dx[:1], dy[:1]
        )[0]
        self.assertGreater(abs(raw_mean[2] - mean_response[2]), 0.05)

    def test_grid_preserves_nodes_and_returns_owned_immutable_float32(self):
        grid = bake(
            [3, 4, 5], [1, 0.5, 0], [0, -0.2, 1], (2, 3), (7, 9), MATRICES[1], 7
        )
        u, v = np.meshgrid(np.linspace(0, 2, 9), np.linspace(0, 3, 7))
        uv = np.stack((u, v), axis=-1)
        np.testing.assert_allclose(
            sample(grid, uv), grid.values.astype(float), atol=2e-17, rtol=0
        )
        self.assertEqual(grid.retained_bytes, 7 * 9 * 4 * 4)
        self.assertEqual(grid.values.dtype, np.float32)
        with self.assertRaises(ValueError):
            grid.values.setflags(write=True)
        for invalid in ([[-0.01, 0]], [[2.01, 0]], [[0, np.nan]]):
            with self.assertRaises(ValueError):
                sample(grid, invalid)

    def test_original_lattice_words_preserve_full_response_exactly(self):
        points = np.random.default_rng(1685).uniform(-20, 20, (128, 3))
        queries = []
        for matrix, wavelength in zip(MATRICES, (84, 7, 1.045), strict=True):
            at = points @ (matrix / wavelength).T
            lattice = prepare(np.stack((at.min(axis=0), at.max(axis=0))))
            fraction, taps = lattice.corners(at)
            cell = np.floor(at).astype(np.int64)
            np.testing.assert_array_equal(fraction, at - cell)
            for tap, shift in zip(
                taps,
                ([x, y, z] for z in (0, 1) for y in (0, 1) for x in (0, 1)),
                strict=True,
            ):
                np.testing.assert_array_equal(tap, value(cell + shift))
            queries.append(partial(evaluate, lattice))
        reference = rock_fields(points, 7, 1.045, noise_queries=(analytic,) * 3)
        lookup = rock_fields(points, 7, 1.045, noise_queries=queries)
        for a, b in zip(reference, lookup, strict=True):
            np.testing.assert_array_equal(a, b)
        dx, dy = (
            np.tile([0.01, 0.007, 0], (128, 1)),
            np.tile([0, -0.003, 0.01], (128, 1)),
        )
        np.testing.assert_array_equal(
            self.response(reference, dx, dy), self.response(lookup, dx, dy)
        )

    def test_exact_lattice_bounds_budget_and_immutability(self):
        grid = prepare([[-1, -1, -1], [1, 1, 1]])
        self.assertEqual(grid.retained_bytes, 4**3 * 4)
        for points in ([[1, 1, 1]], [[-1, -1, -1]]):
            expected = analytic(np.asarray(points))
            actual = evaluate(grid, points)
            for a, b in zip(actual, expected, strict=True):
                np.testing.assert_array_equal(a, b)
        with self.assertRaises(ValueError):
            grid.words_zyx.setflags(write=True)
        with self.assertRaisesRegex(ValueError, "word budget"):
            prepare([[-1, -1, -1], [1, 1, 1]], word_limit=63)
        for bounds in (
            [[0, 0, 0], [1e6, 1e6, 1e6]],
            [[1, 0, 0], [0, 0, 0]],
            [[0, 0, 0], [2**31, 0, 0]],
        ):
            with self.assertRaises(ValueError):
                prepare(bounds)
        with self.assertRaises(ValueError):
            evaluate(grid, [[2, 1, 1]])


if __name__ == "__main__":
    unittest.main()
