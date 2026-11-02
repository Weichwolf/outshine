"""Direct coarse generation against the independently sampled fine pyramid."""

import unittest
from fractions import Fraction
from unittest.mock import Mock

import numpy as np
from terrain_litter_field import bake
from terrain_litter_mip import bake_mip, unorm8
from terrain_material_plane import hash_words


class DirectLitterMipTests(unittest.TestCase):
    def test_direct_levels_match_the_original_fine_sample_pyramid(self):
        for tile in ((0, 0), (-65, -64), (18, -31), (12345, -23456)):
            source = bake(np.array(tile) * 16 - 8, 1, dtype="uint8")
            for level, expected in enumerate(source.levels):
                np.testing.assert_array_equal(bake_mip(tile, level), expected)

    def test_coarse_level_uses_only_the_small_lattice(self):
        words = Mock(side_effect=hash_words)
        mip = bake_mip((-65, -64), 7, words=words)
        self.assertEqual(mip.shape, (4, 4))
        words.assert_called_once()
        self.assertEqual(words.call_args.args[0].shape, (33 * 33, 2))
        with self.assertRaises(ValueError):
            mip.flags.writeable = True

    def test_quantizer_matches_exact_rational_rounding_including_ties(self):
        for denominator in (2, 10, 65793, 16384**2 * 65793):
            numerators = np.array(
                [
                    0,
                    denominator // 2,
                    denominator * 3 // 2,
                    denominator * 5 // 2,
                    denominator * 255,
                ],
                dtype=np.uint64,
            )
            np.testing.assert_array_equal(
                unorm8(numerators, denominator),
                [round(Fraction(int(n), denominator)) for n in numerators],
            )

    def test_rejects_unsupported_coordinates_and_levels_before_hashing(self):
        words = Mock(
            side_effect=AssertionError("invalid request must not read lattice")
        )
        for tile, level in (((0.5, 0), 0), ((2**27, 0), 0), ((0, 0), -1), ((0, 0), 10)):
            with self.assertRaises(ValueError):
                bake_mip(tile, level, words=words)
        words.assert_not_called()


if __name__ == "__main__":
    unittest.main()
