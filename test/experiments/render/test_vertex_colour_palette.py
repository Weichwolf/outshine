import unittest

import numpy as np
from vertex_colour_palette import decode, encode


class VertexColourPalette(unittest.TestCase):
    def test_hdr_alpha_signed_zero_and_nan_payloads_remain_exact(self):
        words = np.array(
            [
                [0, 0x80000000, 0x3FC4EC4F, 0x3F000000],
                [0x7FC00001, 0x7FC00002, 0xFF800000, 0x3F800000],
            ],
            dtype="<u4",
        )
        for width in (16, 32):
            colours = words[[0, 1, 0, 1, 1]]
            np.testing.assert_array_equal(decode(encode(colours, width)), colours)
            empty = np.empty((0, 4), dtype="<u4")
            np.testing.assert_array_equal(decode(encode(empty, width)), empty)

    def test_palette_capacity_selects_a_valid_width_without_truncation(self):
        colours = np.zeros((65537, 4), dtype="<u4")
        colours[:, 0] = np.arange(len(colours), dtype="<u4")
        np.testing.assert_array_equal(decode(encode(colours[:-1], 16)), colours[:-1])
        with self.assertRaises(ValueError):
            encode(colours, 16)
        np.testing.assert_array_equal(decode(encode(colours, 32)), colours)

    def test_malformed_lengths_header_and_indices_fail(self):
        original = encode(np.zeros((1, 4), dtype="<u4"), 16)
        for payload in (original[:-1], original + bytes(16)):
            with self.assertRaises(ValueError):
                decode(payload)
        for word, value in ((0, 0), (1, 9), (2, 0), (3, 8), (4, 1)):
            damaged = np.frombuffer(original, "<u4").copy()
            damaged[word] = value
            with self.assertRaises(ValueError):
                decode(damaged.tobytes())


if __name__ == "__main__":
    unittest.main()
