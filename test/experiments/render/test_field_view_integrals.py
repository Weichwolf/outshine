"""Independent clipped-cell integrals, precision profiles and bounded ownership."""

import unittest
from unittest.mock import patch

import numpy as np
from field_view_integrals import box_integrals, summed_field


def clipped_cells(image, low, high):
    height, width = image.shape[:2]
    x = np.maximum(
        0,
        np.minimum(np.arange(width) + 1, high[0])
        - np.maximum(np.arange(width), low[0]),
    )
    y = np.maximum(
        0,
        np.minimum(np.arange(height) + 1, high[1])
        - np.maximum(np.arange(height), low[1]),
    )
    return np.einsum("y,x,yxc->c", y, x, image.astype(float))


class IntegralTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rng = np.random.default_rng(1663)
        cls.image = rng.uniform(0, 20, (7, 11, 4))
        cls.image[:, :, 3] = rng.uniform(0, 1, (7, 11))

    def test_fractional_and_outside_rectangles_match_independent_cell_clipping(self):
        rng = np.random.default_rng(16631)
        first = rng.uniform((-4, -3), (15, 10), (100, 2))
        second = rng.uniform((-4, -3), (15, 10), (100, 2))
        low, high = np.minimum(first, second), np.maximum(first, second)
        actual = box_integrals(summed_field(self.image, dtype=np.float64), low, high)
        expected = np.array(
            [clipped_cells(self.image, a, b) for a, b in zip(low, high, strict=True)]
        )
        np.testing.assert_allclose(actual, expected, rtol=0, atol=2e-12)

    def test_exclusive_padding_preserves_single_pixel_edges_and_empty_regions(self):
        image = np.array([[[20, 3, 7, 1]]], dtype=np.float64)
        low = np.array([[-2, -2], [1, 0], [-1, -1], [0.25, 0.5], [0, 0]], float)
        high = np.array([[3, 3], [2, 1], [0, 0], [0.75, 1], [0, 1]], float)
        actual = box_integrals(summed_field(image), low, high)
        np.testing.assert_array_equal(
            actual,
            np.array(
                [
                    [20, 3, 7, 1],
                    [0, 0, 0, 0],
                    [0, 0, 0, 0],
                    [5, 0.75, 1.75, 0.25],
                    [0, 0, 0, 0],
                ]
            ),
        )

    def test_splitting_a_rectangle_does_not_change_energy_or_coverage(self):
        field = summed_field(self.image, dtype=np.float64)
        low = np.array([[0, 0], [3.17, 0], [0, 4.89], [3.17, 4.89]])
        high = np.array([[3.17, 4.89], [11, 4.89], [3.17, 7], [11, 7]])
        actual = box_integrals(field, low, high).sum(0)
        np.testing.assert_allclose(actual, self.image.sum((0, 1)), rtol=0, atol=2e-12)

    def test_prefix_owns_immutable_storage_and_retains_hdr(self):
        image = np.array([[[100, 10, 1, 0.5]]], dtype=np.float32)
        field = summed_field(image)
        image[:] = 0
        self.assertEqual(float(field.table[1, 1, 0]), 100)
        with self.assertRaises(ValueError):
            field.table.flags.writeable = True
        self.assertEqual(field.table.nbytes, 2 * 2 * 4 * 4)

    def test_float32_minification_profile_has_measured_small_error_for_hdr_cells(self):
        rng = np.random.default_rng(16632)
        image = rng.uniform(0, 10000, (128, 128, 4)).astype(np.float32)
        image[:, :, 3] = 1
        low = rng.uniform(0, 80, (32, 2))
        high = low + rng.uniform(16, 48, (32, 2))
        actual = box_integrals(summed_field(image), low, high)
        expected = np.array(
            [clipped_cells(image, a, b) for a, b in zip(low, high, strict=True)]
        )
        self.assertLess(float(np.max(abs(actual - expected) / expected)), 1e-4)

    def test_source_and_lookup_caps_precede_expansion(self):
        with (
            patch(
                "field_view_integrals.np.pad", side_effect=AssertionError("allocation")
            ),
            self.assertRaisesRegex(ValueError, "capacity"),
        ):
            summed_field(self.image, texels_most=76)
        field = summed_field(self.image)
        with (
            patch(
                "field_view_integrals.prefix_at", side_effect=AssertionError("lookup")
            ),
            self.assertRaisesRegex(ValueError, "capacity"),
        ):
            box_integrals(field, np.zeros((2, 2)), np.ones((2, 2)), boxes_most=1)

    def test_invalid_or_overflowing_input_does_not_become_a_valid_field(self):
        for change in ((0, np.nan), (1, -1), (3, 1.1)):
            image = self.image.copy()
            image[0, 0, change[0]] = change[1]
            with self.assertRaises(ValueError):
                summed_field(image)
        image = self.image.copy()
        image[0, 0, 3] = 0
        with self.assertRaises(ValueError):
            summed_field(image)
        image = np.ones((2, 2, 4), dtype=np.float32)
        image[:, :, :3] = 3e38
        with self.assertRaisesRegex(ValueError, "overflow"):
            summed_field(image)
        with self.assertRaises(ValueError):
            summed_field(self.image, dtype=np.float16)

    def test_invalid_box_is_not_silently_clipped_to_a_different_box(self):
        field = summed_field(self.image)
        for low, high in (([[2, 0]], [[1, 1]]), ([[0, np.inf]], [[1, 2]])):
            with self.assertRaises(ValueError):
                box_integrals(field, np.array(low, float), np.array(high, float))


if __name__ == "__main__":
    unittest.main()
