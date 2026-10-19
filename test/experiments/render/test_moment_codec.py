"""Exact channel identities, carry boundaries, coverage and half subnormals."""

import unittest
from fractions import Fraction

import numpy as np
from moment_codec import decode, encode, fixtures, half, sample, unorm


class MomentCodecTests(unittest.TestCase):
    def test_all_16_bit_words_and_byte_carry_reconstruct_same_integer(self):
        q = np.arange(65536, dtype=np.uint32)
        fields = np.column_stack(
            (q.astype(float) / 65535, np.ones(len(q)), np.full(len(q), 0.3))
        ).astype(np.float32)
        words = encode(fields, 2)
        channels = words.view("<f4")
        recovered = np.rint(channels[:, 0] * 255).astype(np.uint32) + (
            np.rint(channels[:, 1] * 255).astype(np.uint32) << 8
        )
        np.testing.assert_array_equal(recovered, q)
        np.testing.assert_allclose(
            decode(words, 2)[:, 0],
            q.astype(float) / 65535,
            rtol=0,
            atol=np.finfo(np.float32).eps,
        )
        for left, right in ((0, 1), (255, 256), (32767, 32768), (65534, 65535)):
            w = Fraction(37, 100)
            filtered_low = (1 - w) * Fraction(left & 255, 255) + w * Fraction(
                right & 255, 255
            )
            filtered_high = (1 - w) * Fraction(left >> 8, 255) + w * Fraction(
                right >> 8, 255
            )
            self.assertEqual(
                (256 * filtered_high + filtered_low) / 257,
                ((1 - w) * left + w * right) / 65535,
            )

    def test_formats_do_not_invent_precision_for_small_moments(self):
        fields = np.array(
            [[[r**4, 1, 0.3] for r in (0.01, 0.02, 0.04, 0.06, 0.12, 0.2)]], np.float32
        )
        u16, fp16 = decode(encode(fields, 2), 2), decode(encode(fields, 4), 4)
        self.assertTrue((u16[0, :3, 0] == 0).all())
        self.assertEqual(fp16[0, 0, 0], 0)
        self.assertGreater(fp16[0, 1, 0], 0)
        self.assertGreater(u16[0, 3, 0], 0)

    def test_coverage_and_metal_channels_unchanged_by_rg_pair(self):
        fields, weights, _ = fixtures()
        old, new = encode(fields, 1), encode(fields, 2)
        np.testing.assert_array_equal(old[..., 2:], new[..., 2:])
        np.testing.assert_array_equal(decode(old, 1)[..., 1:], decode(new, 2)[..., 1:])
        self.assertTrue((fields[..., 0] <= fields[..., 1]).all())
        self.assertTrue((fields[..., 2] <= fields[..., 1]).all())
        np.testing.assert_allclose(
            weights.sum(1), 1, rtol=0, atol=2 * np.finfo(np.float32).eps
        )

    def test_half_subnormals_and_clamped_sampling(self):
        words = np.array([0, 1, 2, 1023, 1024, 15360], np.uint32)
        np.testing.assert_array_equal(
            half(words), [0, 2**-24, 2**-23, 1023 * 2**-24, 2**-14, 1]
        )
        level = np.arange(12, dtype=np.float32).reshape(2, 2, 3)
        for uv, expected in (
            ((0, 0), level[0, 0]),
            ((1, 1), level[1, 1]),
            ((0.5, 0.5), level.mean((0, 1))),
        ):
            pixels, weights = sample(level, uv)
            np.testing.assert_array_equal((pixels * weights[:, None]).sum(0), expected)
        np.testing.assert_array_equal(
            unorm(np.array([0, 1], np.float32), 16), [0, 65535]
        )


if __name__ == "__main__":
    unittest.main()
