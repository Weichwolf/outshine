"""Analytic area integrals distinguish mass quadrature from vertex lumping."""

import unittest

import numpy as np
from native_profile_fit import area_mass, area_weights
from outshine_ref.contracts import Mesh


class ProfileMass(unittest.TestCase):
    def test_triangle_integrates_a_linear_field_squared_exactly(self):
        mesh = Mesh(
            np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], float),
            np.array([[0, 1, 2]], np.uint32),
        )
        mass = area_mass(mesh)
        x = mesh.vertices[:, 0]
        self.assertAlmostEqual(float(x @ mass @ x / mass.sum()), 1 / 6)
        self.assertGreater(np.linalg.eigvalsh(mass.toarray()).min(), 0)

    def test_rectangle_has_no_artificial_axis_covariance(self):
        mesh = Mesh(
            np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]], float),
            np.array([[0, 1, 2], [1, 3, 2]], np.uint32),
        )
        x, y = mesh.vertices[:, 0] - 0.5, mesh.vertices[:, 1] - 0.5
        mass = area_mass(mesh)
        self.assertAlmostEqual(float(x @ mass @ y / mass.sum()), 0)
        self.assertAlmostEqual(float(x @ mass @ x / mass.sum()), 1 / 12)
        lumped = area_weights(mesh)
        self.assertAlmostEqual(float(np.average(x * y, weights=lumped)), -1 / 12)

    def test_subdivision_preserves_the_same_continuous_integral(self):
        coarse = Mesh(
            np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [1, 1, 0]], float),
            np.array([[0, 1, 2], [1, 3, 2]], np.uint32),
        )
        refined = Mesh(
            np.r_[coarse.vertices, [[0.5, 0.5, 0]]],
            np.array([[0, 1, 4], [1, 3, 4], [3, 2, 4], [2, 0, 4]], np.uint32),
        )
        values = []
        for mesh in (coarse, refined):
            height = 2 * mesh.vertices[:, 0] - 3 * mesh.vertices[:, 1] + 0.7
            mass = area_mass(mesh)
            values.append(float(height @ mass @ height / mass.sum()))
        self.assertAlmostEqual(values[0], values[1])


if __name__ == "__main__":
    unittest.main()
