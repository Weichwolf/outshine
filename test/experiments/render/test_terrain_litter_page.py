"""Page cores/gutters against independent canonical fine-pyramid observations."""

import unittest
from unittest.mock import Mock

import numpy as np
from terrain_litter_field import bake
from terrain_litter_mip import bake_mip
from terrain_litter_page import bake_page
from terrain_material_plane import hash_words


class LitterPageTests(unittest.TestCase):
    def test_pages_keep_the_independent_fine_pyramid_in_their_shared_region(self):
        independent = bake([-8, -8], 1, dtype="uint8")
        for core in (64, 128):
            for level in range(6):
                page = bake_page([0, 0], level, core_texels=core)
                first = (128 >> level) - 1
                extent = min(core + 2, len(independent.levels[level]) - first)
                np.testing.assert_array_equal(
                    page[:extent, :extent],
                    independent.levels[level][
                        first : first + extent, first : first + extent
                    ],
                )
                self.assertFalse(page.flags.writeable)

    def test_negative_pages_preserve_shared_bilinear_gutters(self):
        for core in (64, 128):
            for level in range(6):
                page = bake_page([-1, -1], level, core_texels=core)
                right = bake_page([0, -1], level, core_texels=core)
                lower = bake_page([-1, 0], level, core_texels=core)
                np.testing.assert_array_equal(page[:, -2:], right[:, :2])
                np.testing.assert_array_equal(page[-2:, :], lower[:2, :])

    def test_high_and_negative_coordinates_match_the_canonical_mip_lattice(self):
        for tile in ((12345, -23456), (-12345, 23456)):
            for level in range(6):
                page = bake_page(tile, level)
                step = 2.0**level / 16
                corner_m = (np.asarray(tile) * 64 - 1) * step
                old_tile = np.floor((corner_m + 8) / 16).astype(int)
                old_origin = old_tile * 16 - 8
                begin = np.rint((corner_m - old_origin) / step).astype(int)
                original = bake_mip(old_tile, level)
                width = min(66, len(original) - begin[0])
                height = min(66, len(original) - begin[1])
                self.assertGreater(min(width, height), 0)
                np.testing.assert_array_equal(
                    page[:height, :width],
                    original[begin[1] : begin[1] + height, begin[0] : begin[0] + width],
                )

    def test_invalid_inputs_never_request_noise_words(self):
        words = Mock(side_effect=AssertionError("invalid page queried its source"))
        for tile, level, core in (
            ([0, 0], 6, 64),
            ([0, 0], 0, 32),
            ([2**31 - 1, 0], 5, 128),
        ):
            with self.assertRaises(ValueError):
                bake_page(tile, level, core_texels=core, words=words)
        words.assert_not_called()
        words = Mock(wraps=hash_words)
        page = bake_page([0, 0], 0, words=words)
        self.assertEqual(page.shape, (66, 66))
        self.assertEqual(words.call_args.args[0].shape, (49, 2))


if __name__ == "__main__":
    unittest.main()
