"""Independent categorical boundaries, immutable prepared cache and canonical IDs."""

import hashlib
import json
import struct
import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import SpeciesHeightMixture, species_site_uniforms


class SpeciesMixtureTests(unittest.TestCase):
    def test_half_open_probabilities_interpolate_raw_weights_before_normalizing(self):
        a = SpeciesHeightMixture.prepare(
            ("a", "b", "c"), (0, 100), ((1, 1, 0), (0, 1, 3))
        )
        np.testing.assert_array_equal(a.choose(0, (0, 0.499, 0.5, 0.999)), (0, 0, 1, 1))
        np.testing.assert_array_equal(
            a.choose(100, (0, 0.249, 0.25, 0.999)), (1, 1, 2, 2)
        )
        np.testing.assert_array_equal(
            a.choose(50, (0.16, 0.17, 0.49, 0.5)), (0, 1, 1, 2)
        )

    def test_subnormal_total_and_zero_support_do_not_select_invalid_taxa(self):
        a = SpeciesHeightMixture(
            ("a", "b"), np.array((0, 1)), np.array(((0, 5e-324), (0, 5e-324)))
        )
        np.testing.assert_array_equal(a.choose(0, (0, np.nextafter(1, 0))), (1, 1))
        empty = SpeciesHeightMixture.prepare(("a", "b"), (0, 1), ((0, 0), (0, 0)))
        np.testing.assert_array_equal(
            empty.choose((0, 1, 2), (0, 0.5, 0.99)), (-1, -1, -1)
        )
        with self.assertRaises(ValueError):
            a.choose(0, 1)
        with self.assertRaises(ValueError):
            SpeciesHeightMixture(
                ("a", "b"), np.array((0, 1)), np.array(((1, 0), (1, 0)), dtype=np.uint8)
            )

    def test_scaled_order_invariant_inputs_and_immutable_cached_recipe(self):
        a = SpeciesHeightMixture.prepare(("b", "a"), (0, 1), ((1e308, 1e308), (0, 0)))
        b = SpeciesHeightMixture.prepare(("a", "b"), (0, 1), ((1, 1), (0, 0)))
        self.assertEqual(a.recipe(), b.recipe())
        recipe = json.loads(json.dumps(a.recipe()))
        with patch.object(
            SpeciesHeightMixture,
            "prepare",
            side_effect=AssertionError("cache repeated preparation"),
        ):
            cached = SpeciesHeightMixture.from_recipe(recipe)
        np.testing.assert_array_equal(cached.cumulative, a.cumulative)
        np.testing.assert_array_equal(cached.choose((0, 1), (0.5, 0)), (1, -1))
        with self.assertRaises(ValueError):
            cached.cumulative.setflags(write=True)

    def test_standard_hash_message_and_endian_subset_order_replay(self):
        keys = np.array(
            ((14, 8653, 5752, 12, 32), (14, 8653, 5752, 13, 32)), dtype=np.uint64
        )
        actual = species_site_uniforms(keys, 17)
        message = b"outshine-species-1\0" + struct.pack(
            "<QH5Q", 17, 5, *map(int, keys[0])
        )
        expected = (
            int.from_bytes(hashlib.blake2b(message, digest_size=8).digest(), "little")
            >> 11
        ) / 2**53
        self.assertEqual(actual[0], expected)
        np.testing.assert_array_equal(
            species_site_uniforms(keys[::-1], 17), actual[::-1]
        )
        np.testing.assert_array_equal(
            species_site_uniforms(keys.astype(">u8"), 17), actual
        )
        self.assertEqual(species_site_uniforms(keys[:1], 17)[0], actual[0])
        self.assertNotEqual(species_site_uniforms(keys, 18)[0], actual[0])
        with self.assertRaises(ValueError):
            species_site_uniforms(np.array(((-1,),)), 17)


if __name__ == "__main__":
    unittest.main()
