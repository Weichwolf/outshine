"""Original-word substrate checks against the existing independent reference."""

import json
import unittest
from pathlib import Path
from unittest.mock import Mock

import numpy as np
from ground_gradients import noise_gradient
from ground_litter_reference import detail as reference
from rock_direction import value
from terrain_material_plane import hash_words, noise, prepare
from terrain_material_substrate import detail

ROOT = Path(__file__).resolve().parents[3]


class SubstrateFields(unittest.TestCase):
    def test_words_equal_original_3d_hash_at_z_zero(self):
        cells = np.random.default_rng(1684).integers(-(2**30), 2**30, (256, 2))
        expected = value(np.column_stack((cells, np.zeros(256, dtype=np.int64))))
        np.testing.assert_array_equal(
            hash_words(cells).astype(float) / 16777215, expected
        )

    def test_noise_value_and_gradient_match_existing_reference(self):
        points = np.random.default_rng(4).uniform(-8, 8, (128, 2))
        field = prepare([[-8, -8], [8, 8]])
        actual, gradient = noise(points)
        old, old_gradient = noise_gradient(np.column_stack((points, np.zeros(128))))
        np.testing.assert_allclose(actual, old, rtol=0, atol=1e-15)
        np.testing.assert_allclose(gradient, old_gradient[:, :2], rtol=0, atol=2e-15)
        lookup = noise(points, words=field.get)
        for a, b in zip((actual, gradient), lookup, strict=True):
            np.testing.assert_array_equal(a, b)
        with self.assertRaises(ValueError):
            field.words_yx.setflags(write=True)

    def test_all_actual_recipes_preserve_direct_and_prepared_response(self):
        recipes = json.loads((ROOT / "data/world/ground-materials.json").read_bytes())[
            "classes"
        ]
        points = np.random.default_rng(1).uniform(0, 0.01, (16, 2)) + [1000.1, -200.2]
        for recipe in recipes:
            parameters = (
                recipe["grainSizeM"],
                recipe["heightAmplitudeM"],
                *recipe["detailScaleM"],
            )
            getters = []
            for scale in (parameters[2], parameters[3], parameters[0]):
                if scale <= 0:
                    getters.append(hash_words)
                    continue
                normalized = points / scale
                field = prepare([normalized.min(axis=0), normalized.max(axis=0)])
                getters.append(field.get)
            for footprint in (0.0001, 0.01, 4.0):
                dx, dy = (
                    np.tile([footprint, 0.25 * footprint], (16, 1)),
                    np.tile([-0.125 * footprint, footprint], (16, 1)),
                )
                with self.subTest(recipe=recipe["name"], footprint=footprint):
                    baseline = detail(points, dx, dy, parameters, 0.5)
                    lookup = detail(
                        points, dx, dy, parameters, 0.5, word_sources=getters
                    )
                    np.testing.assert_array_equal(baseline, lookup)
                    np.testing.assert_allclose(
                        baseline,
                        reference(points, dx, dy, parameters, 0.5),
                        rtol=0,
                        atol=2e-11,
                    )

    def test_far_and_inactive_grain_do_no_hash_or_lookup_work(self):
        points = np.array([[0.1, 0.2]])
        forbidden = Mock(side_effect=AssertionError("inactive work"))
        actual = detail(
            points,
            [[1e6, 0]],
            [[0, 1e6]],
            (0.01, 0.004, 1.6, 0.239),
            0.5,
            word_sources=(forbidden,) * 3,
        )
        np.testing.assert_array_equal(actual, [[1, 0, 0, 0]])
        forbidden.assert_not_called()
        detail(
            points,
            [[0.01, 0]],
            [[0, 0.01]],
            (0.01, 0.004, 1.6, 0.239),
            0.5,
            word_sources=(hash_words, hash_words, forbidden),
        )
        forbidden.assert_not_called()

    def test_word_capacity_and_query_misses_remain_explicit(self):
        with self.assertRaisesRegex(ValueError, "word budget"):
            prepare([[0, 0], [1e6, 1e6]])
        field = prepare([[0, 0], [1, 1]])
        self.assertEqual(field.retained_bytes, 4 * 4 * 4)
        with self.assertRaisesRegex(ValueError, "outside"):
            noise([[8, 8]], words=field.get)


if __name__ == "__main__":
    unittest.main()
