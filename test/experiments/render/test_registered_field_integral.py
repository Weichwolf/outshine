"""The native paired readback keeps its encoding, extent, border and low bits."""

import unittest

import numpy as np
from registered_field_integral import INTEGRAL_ENCODING, decode_integral


class RegisteredIntegral(unittest.TestCase):
    def setUp(self):
        self.packed = np.zeros((2, 4, 4), dtype=np.float32)
        self.packed[1, 2] = 2**24
        self.packed[1, 3] = 0.25
        self.report = {
            "integral_encoding": INTEGRAL_ENCODING,
            "integral_width": 4,
            "integral_height": 2,
        }

    def test_low_component_survives_readback_reconstruction(self):
        result = decode_integral(self.packed, self.report, 1, 1)
        self.assertEqual(result.shape, (2, 2, 4))
        self.assertEqual(result.dtype, np.float64)
        np.testing.assert_array_equal(result[1, 1], np.full(4, 2**24 + 0.25))

    def test_legacy_or_unknown_encoding_is_not_silently_reinterpreted(self):
        for encoding in [None, "rgba32f", "unknown"]:
            with self.subTest(encoding=encoding), self.assertRaises(ValueError):
                decode_integral(
                    self.packed, dict(self.report, integral_encoding=encoding), 1, 1
                )

    def test_physical_dimensions_are_bound_to_the_raw_raster(self):
        for report in [
            dict(self.report, integral_width=2),
            dict(self.report, integral_width=3),
            dict(self.report, integral_height=1),
        ]:
            with self.subTest(report=report), self.assertRaises(ValueError):
                decode_integral(self.packed, report, 1, 1)
        for width, height in [(0, 1), (-1, 1), (1, 0), (1, 2)]:
            with self.subTest(raw=(width, height)), self.assertRaises(ValueError):
                decode_integral(self.packed, self.report, width, height)

    def test_both_border_components_must_be_zero(self):
        for y, x in [(0, 0), (0, 3), (1, 0), (1, 1)]:
            packed = self.packed.copy()
            packed[y, x, 0] = 1
            with self.subTest(at=(y, x)), self.assertRaises(ValueError):
                decode_integral(packed, self.report, 1, 1)

    def test_shape_dtype_and_nonfinite_values_are_rejected(self):
        values = [self.packed[:, :3], self.packed.astype(np.float64)]
        for invalid in [np.nan, np.inf, -np.inf]:
            packed = self.packed.copy()
            packed[1, 3, 0] = invalid
            values.append(packed)
        for packed in values:
            with self.subTest(shape=packed.shape), self.assertRaises(ValueError):
                decode_integral(packed, self.report, 1, 1)


if __name__ == "__main__":
    unittest.main()
