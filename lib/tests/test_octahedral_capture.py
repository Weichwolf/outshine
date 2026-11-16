import unittest

import numpy as np

from outshine_ref.vegetation.octahedral_capture import capture_directions, octahedral_direction


class OctahedralCapture(unittest.TestCase):
    def test_native_y_up_axes_and_lower_hemisphere(self):
        points = [(0, 0), (1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1)]
        expected = [(0, 1, 0), (1, 0, 0), (-1, 0, 0), (0, 0, 1), (0, 0, -1),
                    (0, -1, 0), (0, -1, 0)]
        np.testing.assert_allclose(octahedral_direction(points), expected, atol=1e-15)
        direction = capture_directions()
        np.testing.assert_allclose(np.linalg.norm(direction, axis=1), 1, atol=1e-15)
        self.assertGreater(direction[:, 1].max(), .9)
        self.assertLess(direction[:, 1].min(), -.9)

    def test_folded_square_boundary_is_continuous_on_sphere(self):
        values = np.linspace(0, 1, 31)
        for fixed in (-1, 1):
            for axis in (0, 1):
                a = np.column_stack((values, np.full(len(values), fixed)))
                b = np.column_stack((-values, np.full(len(values), fixed)))
                if axis == 0:
                    a, b = a[:, ::-1], b[:, ::-1]
                np.testing.assert_allclose(octahedral_direction(a), octahedral_direction(b), atol=1e-15)
