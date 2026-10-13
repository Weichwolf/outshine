import unittest

import numpy as np

from outshine_ref.buildings.modelling import Frame, Relative, Scope, cga_to_enu, enu_to_cga


class BuildingScope(unittest.TestCase):
    def test_noncommuting_rotations_use_euler_addition(self):
        scope = Scope().r(90, 0, 0).r(0, 90, 0).r(-90, 0, 0)
        np.testing.assert_allclose(scope.axes @ [1, 2, 3], [3, 2, -1], atol=1e-12)
        np.testing.assert_allclose(Scope().r(90, 0, 0).axes @ [1, 2, 3], [1, -3, 2], atol=1e-12)

    def test_rotated_translation_and_signed_relative_sizes(self):
        scope = Scope(size=(2, 3, 4)).r(0, 90, 0).t(Relative(.5), 2, Relative(.5))
        np.testing.assert_allclose(scope.origin, [2, 2, -1], atol=1e-12)
        self.assertEqual(scope.s(Relative(-2), 0, 7).size, (-4, 0, 7))

    def test_centre_rotation_keeps_centre_in_tilted_frames(self):
        for size in ((2, 3, 4), (-2, 3, 4), (2, 0, 4)):
            scope = Scope((1, -4, 7), (20, 35, -17), size,
                          Frame((3, 2, -1), (35, -20, 12)), Frame((100, 10, -80), (-5, 70, 9)))
            np.testing.assert_allclose(scope.r(31, -22, 63, "scopeCenter").centre, scope.centre, atol=1e-12)
            np.testing.assert_array_equal(scope.r(31, -22, 63).origin, scope.origin)

    def test_absolute_frames_and_relative_offsets(self):
        scope = Scope((1, 2, 3), pivot=Frame((5, 0, 0), (0, 90, 0)), object_frame=Frame((10, 20, 30)))
        for frame, expected in (("world", [1, 2, 3]), ("object", [11, 22, 33]), ("pivot", [18, 22, 29])):
            np.testing.assert_allclose(scope.translate("abs", frame, 1, 2, 3).origin, expected, atol=1e-12)
        np.testing.assert_allclose(scope.translate("rel", "pivot", 1, 2, 3).origin,
                                   scope.origin + [3, 2, -1], atol=1e-12)

    def test_cached_transforms_and_invalid_parameters(self):
        scope = Scope()
        for array in (scope.matrix, scope.axes, scope.origin, scope.centre, scope.pivot.axes):
            self.assertFalse(array.flags.writeable)
        for value in (True, "3", float("nan"), float("inf")):
            with self.subTest(value=value), self.assertRaises(ValueError):
                scope.t(value, 0, 0)
        with self.assertRaises(ValueError):
            scope.translate("abs", "missing", 0, 0, 0)


class CoordinateBoundary(unittest.TestCase):
    def test_enu_axes_are_right_handed_and_preserve_winding(self):
        basis = cga_to_enu(np.eye(3))
        self.assertEqual(np.linalg.det(basis), 1)
        np.testing.assert_array_equal(basis, [[1, 0, 0], [0, 0, 1], [0, -1, 0]])
        triangle = np.array([[2., 3., 4.], [5, 3, 4], [2, 7, 4]])
        converted = cga_to_enu(triangle)
        normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
        np.testing.assert_array_equal(np.cross(converted[1] - converted[0], converted[2] - converted[0]),
                                      cga_to_enu(normal))
        np.testing.assert_array_equal(enu_to_cga(converted), triangle)

    def test_batches_empty_streams_height_and_input_ownership(self):
        enu = np.array([[[10., 20., -2.], [11., 22., 8.]]])
        before = enu.copy()
        converted = enu_to_cga(enu)
        np.testing.assert_array_equal(converted[..., 1], enu[..., 2])
        converted[0, 0] = 99
        np.testing.assert_array_equal(enu, before)
        self.assertEqual(cga_to_enu(np.empty((0, 3))).shape, (0, 3))

    def test_malformed_coordinate_streams_are_rejected(self):
        for values in ([], [1, 2], [1, 2, float("nan")], ["1", "2", "3"], [True] * 3, [1j, 2, 3]):
            with self.subTest(values=values), self.assertRaises(ValueError):
                cga_to_enu(values)


if __name__ == "__main__":
    unittest.main()
