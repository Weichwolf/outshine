"""Stored scalar fields retain phase, coverage and original composition semantics."""

import unittest
from unittest.mock import Mock

import numpy as np
from terrain_litter_field import bake, sample


class LitterFieldTests(unittest.TestCase):
    def test_affine_field_survives_mips_rotation_and_anisotropy_without_generation(
        self,
    ):
        evaluate = Mock(side_effect=lambda at: (0.5 + at[:, 0] * 0.001, None))
        field = bake([-16, -16], 1, side=64, dtype="float32", evaluate=evaluate)
        at = np.array([[0.3, 1.7], [-2.3, -0.2]])
        for angle, ratio, width in ((0, 1, 0.1), (0.7, 16, 1), (0.3, 4, 1.5)):
            dx = np.tile([np.cos(angle), np.sin(angle)], (2, 1)) * width
            dy = np.tile([-np.sin(angle), np.cos(angle)], (2, 1)) * width / ratio
            t = max(width - 1, 0)
            expected = at[:, 0] * 0.001 * (1 - t * t * (3 - 2 * t))
            np.testing.assert_allclose(sample(field, at, dx, dy), expected, atol=2e-9)
        evaluate.assert_called_once()
        for level in field.levels:
            with self.assertRaises(ValueError):
                level.flags.writeable = True
        self.assertEqual(field.retained_bytes, sum(4**i for i in range(7)) * 4)

    def test_far_footprint_is_mean_without_reading_outside_resident_tile(self):
        field = bake([-16, -16], 1, side=32)
        result = sample(field, [[1e6, 1e6]], [[2, 0]], [[0, 1]])
        np.testing.assert_array_equal(result, [0])

    def test_point_and_line_footprints_remain_supported(self):
        evaluate = lambda at: (np.full(len(at), 0.7), None)
        field = bake([-16, -16], 1, side=32, dtype="float32", evaluate=evaluate)
        np.testing.assert_allclose(
            sample(field, [[0, 0], [0, 0]], [[0, 0], [0.3, 0]], [[0, 0], [0, 0]]),
            [0.2, 0.2],
            atol=5e-9,
        )

    def test_rejects_missing_coverage_invalid_storage_and_nonfinite_source(self):
        field = bake([-16, -16], 1, side=32)
        with self.assertRaises(ValueError):
            sample(field, [[16, 0]], [[0.1, 0]], [[0, 0.1]])
        for size in (0, 17, 1024):
            with self.assertRaises(ValueError):
                bake([0, 0], 1, side=size)
        with self.assertRaises(ValueError):
            bake([0, 0], 1, evaluate=lambda at: (np.full(len(at), np.nan), None))


if __name__ == "__main__":
    unittest.main()
