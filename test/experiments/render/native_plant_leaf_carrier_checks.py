"""Normal moment adaptation must not fabricate shading directions or mix alpha into RGB."""

import unittest

import numpy as np
from native_plant_leaf_carrier import normal_directions


class NormalDirection(unittest.TestCase):
    def test_direction_is_independent_of_coverage_mass(self):
        direction = np.array((-0.6, 0, 0.8))
        mass = np.array((0.25, 1.0))
        encoded = np.column_stack(((direction + 1) * mass[:, None] / 2, mass))
        actual, length = normal_directions(encoded)
        np.testing.assert_allclose(actual, np.tile(direction, (2, 1)), atol=1e-15)
        np.testing.assert_allclose(length, mass, atol=1e-15)

    def test_normalization_exposes_the_discarded_resultant_length(self):
        actual, length = normal_directions(np.array((0.5, 0.5, 0.75, 1.0)))
        np.testing.assert_array_equal(actual, (0, 0, 1))
        self.assertEqual(length, 0.5)
        with self.assertRaisesRegex(ValueError, "cancelling"):
            normal_directions(np.array((0.5, 0.5, 0.5, 1.0)))

    def test_empty_support_and_invalid_mass_remain_distinct(self):
        actual, length = normal_directions(np.zeros(4))
        np.testing.assert_array_equal(actual, (0, 0, 1))
        self.assertEqual(length, 0)
        for encoded in ((0, 0, 1, 0.25), (0, 0, 0, -1), (0, 0, float("nan"), 1)):
            with self.assertRaises(ValueError):
                normal_directions(np.asarray(encoded))


if __name__ == "__main__":
    unittest.main()
