"""Own corolla jets, globally regular surface and correctly oriented bounded meshes."""

import unittest

import numpy as np
from outshine_ref.vegetation import Bellflower


class BellflowerChecks(unittest.TestCase):
    def test_corolla_differentials_and_outward_triangles(self):
        flower = Bellflower()
        t, a = np.linspace(0.01, 0.99, 37), np.linspace(0, 2 * np.pi, 37)
        _p, n, dt, da = flower.surface(t, a)
        h = 1e-6
        for shifted, derivative in (((t + h, a), dt), ((t, a + h), da)):
            minus = (t - h, a) if derivative is dt else (t, a - h)
            finite = (flower.surface(*shifted)[0] - flower.surface(*minus)[0]) / (2 * h)
            np.testing.assert_allclose(finite, derivative, rtol=0, atol=2e-11)
        for segments, sides in ((3, 18), (6, 24), (12, 48)):
            mesh = flower.mesh(segments, sides)
            v, f, normals = mesh["vertices"], mesh["faces"], mesh["normals"]
            points = v[f]
            area = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
            self.assertTrue(
                (np.einsum("ij,ikj->ik", area, normals.reshape(-1, 3, 3)) > 0).all()
            )
            self.assertEqual(mesh["corner_uv"].shape, (f.size, 2))
            self.assertTrue(
                (
                    np.ptp(mesh["corner_uv"][: -3 * sides, 1].reshape(-1, 3), axis=1)
                    < 0.5
                ).all()
            )
        np.testing.assert_allclose(np.linalg.norm(n, axis=1), 1, atol=2e-14)

    def test_regular_side_has_a_global_positive_radial_jacobian_component(self):
        rng = np.random.default_rng(438)
        t, angle = rng.random(2048), rng.uniform(-2 * np.pi, 2 * np.pi, 2048)
        for lobes in (3, 4, 6, 12):
            flower = Bellflower(0.0075, 0.0035, lobes)
            p, _n, dt, da = flower.surface(t, angle)
            jacobian = np.cross(dt, da)
            radial = np.c_[np.cos(angle), np.sin(angle), np.zeros(len(angle))]
            projection = np.sum(jacobian * radial, axis=1)
            self.assertTrue(
                (
                    projection >= 0.0448 * flower.radius * flower.length * (1 - 1e-13)
                ).all()
            )
            self.assertTrue((dt[:, 2] < 0).all())
            self.assertTrue(
                (
                    np.linalg.norm(p[:, :2], axis=1)
                    >= 0.16 * flower.radius * (1 - 1e-13)
                ).all()
            )

    def test_bad_inputs_and_unresolved_facets_are_refused(self):
        for args in (
            (np.nan, 0.003),
            (0, 0.003),
            (1e-8, 0.003),
            (0.01, 20),
            ([1], 0.003),
        ):
            with self.assertRaises(ValueError):
                Bellflower(*args)
        for values in ((3, 5), (0, 24), (3, 1000)):
            with self.assertRaises(ValueError):
                Bellflower().mesh(*values)
        with self.assertRaisesRegex(ValueError, "subdivision"):
            Bellflower(0.00035, 0.0035).mesh(3, 18)
        for t, angle in ((-1, 0), (1.1, 0), (0.5, np.inf), (0.5, 1e300)):
            with self.assertRaises(ValueError):
                Bellflower().surface(t, angle)


if __name__ == "__main__":
    unittest.main()
