import unittest

import numpy as np
from outshine_ref.vegetation import AxisState, NeedlePattern


def axis(path, **options):
    return AxisState(
        path, 741, 0.03, 0.1, np.array((1.0, 2, 3)), np.eye(3), 0, 0.5, **options
    )


class AxisCapacityTests(unittest.TestCase):
    def test_default_remains_sixty_four_and_wider_paths_are_explicit(self):
        self.assertEqual(axis((63,)).path_capacity, 64)
        with self.assertRaises(ValueError):
            axis((64,))
        for path in ((239,), (239, 200, 0), (255,)):
            result = axis(path, path_capacity=256)
            self.assertEqual(result.path, path)
            self.assertEqual(result.id, 741)
            self.assertFalse(result.origin.flags.writeable)
            self.assertFalse(result.frame.flags.writeable)

    def test_invalid_capacities_and_members_stay_rejected(self):
        for capacity in (0, 65, 255, 257, True, 256.0):
            with self.subTest(capacity=capacity), self.assertRaises(ValueError):
                axis((0,), path_capacity=capacity)
        for path in ((-1,), (256,), (True,), (1.0,), (0,) * 8):
            with self.subTest(path=path), self.assertRaises(ValueError):
                axis(path, path_capacity=256)

    def test_physical_needles_do_not_change_with_slot_capacity(self):
        pattern = NeedlePattern(first=0.1, last=0.9, spacing=0.003, group_size=2)
        before = pattern.detail(axis((1, 2, 3)), (0.0003, 0.0001), 815, 100)
        for path in ((1, 2, 3), (239, 200, 0)):
            after = pattern.detail(
                axis(path, path_capacity=256), (0.0003, 0.0001), 815, 100
            )
            self.assertEqual(before.keys(), after.keys())
            for key in before:
                np.testing.assert_array_equal(before[key], after[key])


if __name__ == "__main__":
    unittest.main()
