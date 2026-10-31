"""Recorded binding identity, empty/duplicate queries and integer edge cases."""

import unittest

import numpy as np
from source_binding_columns import encode


class BindingColumnsTest(unittest.TestCase):
    def test_bit_tail_negative_and_full_int32_roundtrip(self):
        values = np.array(
            [
                [i, 42, -1, -1, i % 2, (-2147483648 if i == 0 else 2147483647)]
                for i in range(11)
            ],
            dtype="<i4",
        )
        query = np.array([10, 0, 9, 10, 1], dtype="<i4")
        for bitpack in (False, True):
            product = encode(values, bitpack=bitpack)
            np.testing.assert_array_equal(product.gather(query), values[query])
            self.assertEqual(product.gather(np.array([], dtype="<i4")).shape, (0, 6))
            self.assertTrue(
                all(not column.flags.writeable for column in product.columns)
            )

    def test_bad_indices_and_unsigned_input_are_refused(self):
        values = np.zeros((2, 6), dtype="<i4")
        product = encode(values)
        for query in (np.array([-1]), np.array([2]), np.array([0.5])):
            with self.assertRaises(ValueError):
                product.gather(query)
        with self.assertRaises(ValueError):
            encode(values.view("<u4"))


if __name__ == "__main__":
    unittest.main()
