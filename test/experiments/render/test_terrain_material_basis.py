"""Material-block phase, metric gradient, residency and admission contracts."""

import unittest
from unittest.mock import Mock

import numpy as np
from terrain_material_basis import bake, sample


def affine(at, dx, dy):
    gradient = np.tile([0.125, -0.25], (len(at), 1))
    return np.column_stack(
        (
            0.2 + 0.001 * at[:, 0],
            0.3 + 0.002 * at[:, 1],
            np.full(len(at), 0.4),
            0.6 - 0.001 * at[:, 1],
            np.sum(gradient * dx, axis=-1),
            np.sum(gradient * dy, axis=-1),
        )
    )


class MaterialBasisTests(unittest.TestCase):
    def test_phase_gradients_and_reuse_without_recipe_calls(self):
        evaluator = Mock(side_effect=affine)
        grid = bake(evaluator, [-3, 7], 4, 0.125, 9)
        points = np.array([[-3, 7], [-2.73, 8.11], [1, 11]])
        for dx, dy in (
            ([0.125, 0], [0, 0.125]),
            ([0, 0.125], [-0.125, 0]),
            ([-0.1, 0.075], [0.01, 0]),
        ):
            dx, dy = np.tile(dx, (3, 1)), np.tile(dy, (3, 1))
            np.testing.assert_allclose(
                sample(grid, points, dx, dy), affine(points, dx, dy), atol=3e-8, rtol=0
            )
        evaluator.assert_called_once()
        self.assertEqual(grid.retained_bytes, 9 * 9 * 6 * 4)
        with self.assertRaises(ValueError):
            grid.values.flags.writeable = True

    def test_half_storage_is_explicit_and_bounded(self):
        grid = bake(affine, [0, 0], 1, 0.1, 17, dtype="float16")
        self.assertEqual(grid.retained_bytes, 17 * 17 * 6 * 2)
        at, dx, dy = np.array([[0.2, 0.8]]), [[0.1, 0]], [[0, 0.1]]
        np.testing.assert_allclose(
            sample(grid, at, dx, dy), affine(at, dx, dy), atol=5e-4
        )

    def test_adjacent_blocks_share_phase_and_nonlinear_refinement_converges(self):
        left = bake(affine, [-1, 0], 1, 0.1, 9)
        right = bake(affine, [0, 0], 1, 0.1, 9)
        at = np.array([[0, 0], [0, 0.13], [0, 1]])
        dx, dy = np.tile([0.1, 0], (3, 1)), np.tile([0, 0.1], (3, 1))
        np.testing.assert_array_equal(
            sample(left, at, dx, dy), sample(right, at, dx, dy)
        )

        def curved(at, dx, dy):
            result = affine(at, dx, dy)
            result[:, 0] = np.sin(at[:, 0] * 2) * np.cos(at[:, 1] * 3)
            return result

        at = np.random.default_rng(1934).uniform(0, 1, (100, 2))
        dx, dy = np.tile([0.1, 0], (100, 1)), np.tile([0, 0.1], (100, 1))
        error = [
            np.max(
                np.abs(
                    sample(bake(curved, [0, 0], 1, 0.1, side), at, dx, dy)
                    - curved(at, dx, dy)
                )
            )
            for side in (9, 33)
        ]
        self.assertLess(error[1], error[0] / 8)

    def test_refuses_missing_domain_and_wrong_footprint_without_fallback(self):
        grid = bake(affine, [0, 0], 1, 0.1, 9)
        for at, dx, dy in (
            ([[-0.001, 0]], [[0.1, 0]], [[0, 0.1]]),
            ([[0, 1.001]], [[0.1, 0]], [[0, 0.1]]),
            ([[0, 0]], [[0.2, 0]], [[0, 0.1]]),
            ([[0, 0]], [[np.nan, 0]], [[0, 0.1]]),
        ):
            with self.subTest(at=at, dx=dx), self.assertRaises(ValueError):
                sample(grid, at, dx, dy)

    def test_bad_admission_never_evaluates_recipe(self):
        evaluator = Mock()
        for options in (
            {"side": 258},
            {"side": True},
            {"extent_m": 0},
            {"footprint_m": np.inf},
            {"origin_m": [np.nan, 0]},
            {"dtype": "int16"},
        ):
            args = {"origin_m": [0, 0], "extent_m": 1, "footprint_m": 0.1, "side": 9}
            args.update(options)
            with self.subTest(options=options), self.assertRaises(ValueError):
                bake(evaluator, **args)
        evaluator.assert_not_called()
        for value in (np.nan, 1e10):
            with self.subTest(value=value), self.assertRaises(ValueError):
                bake(
                    lambda at, *_, value=value: np.full((len(at), 6), value),
                    [0, 0],
                    1,
                    0.1,
                    9,
                    dtype="float16",
                )


if __name__ == "__main__":
    unittest.main()
