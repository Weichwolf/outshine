"""Independent piecewise probabilities, source-key replay and empty-support controls."""

import hashlib
import json
import struct
import unittest
from unittest.mock import patch

import numpy as np
from species_mixture import HeightMixture, site_uniforms


class SpeciesMixtureTests(unittest.TestCase):
    def test_subnormal_total_cannot_select_an_index_outside_the_taxa(self):
        a = HeightMixture(
            ("a", "b"), np.array((0, 1)), np.array(((0, 5e-324), (0, 5e-324)))
        )
        for method in ("linear", "binary"):
            np.testing.assert_array_equal(
                a.choose(0, (0, np.nextafter(1, 0)), method=method), (1, 1)
            )

    def test_independent_half_open_intervals_at_knots_and_between(self):
        mixture = HeightMixture.prepare(
            ("a", "b", "c"), (0, 100), ((1, 1, 0), (0, 1, 3))
        )
        u = np.array((0, np.nextafter(0.5, 0), 0.5, np.nextafter(1, 0)))
        for method in ("linear", "binary"):
            np.testing.assert_array_equal(
                mixture.choose(0, u, method=method), (0, 0, 1, 1)
            )
            np.testing.assert_array_equal(
                mixture.choose(100, (0, 0.249, 0.25, 0.999), method=method),
                (1, 1, 2, 2),
            )
            # Raw midpoint weights are (0.5,1,1.5), not an average of normalized endpoint probabilities.
            np.testing.assert_array_equal(
                mixture.choose(50, (0.16, 0.17, 0.49, 0.5), method=method), (0, 1, 1, 2)
            )

    def test_zero_support_overflow_scaled_inputs_and_order_invariance(self):
        a = HeightMixture.prepare(("b", "a"), (0, 1), ((1e308, 1e308), (0, 0)))
        b = HeightMixture.prepare(("a", "b"), (0, 1), ((1, 1), (0, 0)))
        self.assertEqual(a.recipe(), b.recipe())
        np.testing.assert_array_equal(a.choose((0, 1, 2), (0, 0, 0.9)), (0, -1, -1))
        with self.assertRaises(ValueError):
            a.cumulative.setflags(write=True)
        with self.assertRaises(ValueError):
            a.choose(0, 1)
        with self.assertRaises(ValueError):
            HeightMixture(
                ("a", "b"), np.array((0, 1)), np.array(((1, 0), (1, 0)), dtype=np.uint8)
            )

    def test_prepared_recipe_reloads_without_weight_preparation(self):
        a = HeightMixture.prepare(("a", "b"), (0, 100), ((0, 1), (1, 0)))
        recipe = json.loads(json.dumps(a.recipe()))
        with patch.object(
            HeightMixture,
            "prepare",
            side_effect=AssertionError("cache repeated preparation"),
        ):
            b = HeightMixture.from_recipe(recipe)
        np.testing.assert_array_equal(a.cumulative, b.cumulative)
        u = np.linspace(0, np.nextafter(1, 0), 1001)
        np.testing.assert_array_equal(a.choose(40, u), b.choose(40, u))

    def test_site_hash_is_canonical_and_subset_chunk_order_independent(self):
        keys = np.array(
            ((14, 8653, 5752, 12, 32), (14, 8653, 5752, 13, 32)), dtype=np.uint64
        )
        u = site_uniforms(keys, 17)
        message = b"outshine-species-1\0" + struct.pack(
            "<QH5Q", 17, 5, *map(int, keys[0])
        )
        expected = (
            int.from_bytes(hashlib.blake2b(message, digest_size=8).digest(), "little")
            >> 11
        ) / 2**53
        self.assertEqual(u[0], expected)
        np.testing.assert_array_equal(site_uniforms(keys[::-1], 17), u[::-1])
        np.testing.assert_array_equal(site_uniforms(keys.astype(">u8"), 17), u)
        self.assertEqual(site_uniforms(keys[:1], 17)[0], u[0])
        self.assertNotEqual(site_uniforms(keys, 18)[0], u[0])
        with self.assertRaises(ValueError):
            site_uniforms(np.array(((-1,),)), 17)

    def test_binary_matches_bounded_matrix_for_all_sizes_and_sparse_support(self):
        rng = np.random.default_rng(17)
        for count in (1, 2, 3, 7, 8, 9, 30, 64):
            weights = rng.random((5, count))
            weights[weights < 0.5] = 0
            weights[-1] = 0
            a = HeightMixture.prepare(
                tuple(f"species-{i:03}" for i in range(count)),
                (0, 100, 200, 400, 800),
                weights,
            )
            h, u = rng.uniform(-10, 900, 2000), rng.random(2000)
            np.testing.assert_array_equal(
                a.choose(h, u), a.choose(h, u, method="linear")
            )


if __name__ == "__main__":
    unittest.main()
