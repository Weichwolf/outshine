"""Fixed 3D products preserve NumPy arithmetic, broadcasting and frame invariants."""

import unittest

import numpy as np
from outshine_ref.vegetation.shoot_plan import normal_frame
from outshine_ref.vegetation.vector3 import cross3


class VectorProducts(unittest.TestCase):
    def test_numpy_outputs_and_dtype_are_exact_for_scalar_batch_and_broadcast(self):
        rng = np.random.default_rng(319)
        for shape_a, shape_b in (
            ((3,), (3,)),
            ((513, 3), (3,)),
            ((4, 1, 3), (1, 7, 3)),
            ((0, 3), (3,)),
        ):
            for left_type, right_type in (
                (np.float64, np.float64),
                (np.float32, np.float64),
                (np.int16, np.float64),
                (np.int16, np.int32),
                (np.complex64, np.complex128),
            ):
                left = (rng.normal(size=shape_a) * 20).astype(left_type)
                right = (rng.normal(size=shape_b) * 20).astype(right_type)
                saved_a, saved_b = left.copy(), right.copy()
                actual, expected = cross3(left, right), np.cross(left, right)
                np.testing.assert_array_equal(actual, expected)
                self.assertEqual(actual.dtype, expected.dtype)
                np.testing.assert_array_equal(left, saved_a)
                np.testing.assert_array_equal(right, saved_b)
                self.assertTrue(actual.flags.owndata)

    def test_independent_determinant_definition_orthogonality_and_orientation(self):
        left = np.array([2.0, -3.0, 5.0])
        right = np.array([-7.0, 11.0, 13.0])
        product = cross3(left, right)
        for axis in range(3):
            basis = np.eye(3)[axis]
            self.assertAlmostEqual(
                product[axis], np.linalg.det(np.stack([basis, left, right]))
            )
        self.assertEqual(np.dot(product, left), 0)
        self.assertEqual(np.dot(product, right), 0)
        np.testing.assert_array_equal(cross3(right, left), -product)
        np.testing.assert_array_equal(cross3([1.0, 0, 0], [0.0, 1, 0]), [0, 0, 1])

    def test_near_parallel_frame_hints_and_rigid_rotation_keep_valid_frames(self):
        direction = np.array([[0.0, 0, 1], [1.0, 1e-14, 0], [1e-14, 1.0, 0]])
        hint = direction + [1e-13, 0, 0]
        frames = normal_frame(direction, hint)
        np.testing.assert_allclose(
            frames.transpose(0, 2, 1) @ frames,
            np.tile(np.eye(3), (3, 1, 1)),
            atol=1e-14,
        )
        np.testing.assert_allclose(np.linalg.det(frames), 1, atol=1e-14)
        rotation = np.array([[0.0, -1, 0], [1.0, 0, 0], [0, 0, 1]])
        directions = np.array([[1.0, 2, 3], [2.0, -3, 5]])
        hints = np.array([0.0, 0, 1])
        actual = normal_frame(directions @ rotation.T, hints @ rotation.T)
        expected = rotation @ normal_frame(directions, hints)
        np.testing.assert_allclose(actual, expected, atol=1e-14)

    def test_invalid_dimensions_and_non_numeric_products_refuse(self):
        for a, b in (
            ([1, 2], [1, 2]),
            (0, [1, 2, 3]),
            ([True] * 3, [True] * 3),
            (["x"] * 3, [1, 2, 3]),
        ):
            with self.assertRaises(ValueError):
                cross3(a, b)
        with self.assertRaises(ValueError):
            cross3(np.ones((2, 3)), np.ones((4, 3)))


if __name__ == "__main__":
    unittest.main()
