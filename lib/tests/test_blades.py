"""Independent leaf/grass metric, sheet topology and quadrature ownership cases."""

import unittest
from dataclasses import replace

import numpy as np
from outshine_ref.vegetation import Blade
from outshine_ref.vegetation.blade_surface import quadrature


class BladeTests(unittest.TestCase):
    def test_flat_unlobed_area_has_an_analytic_reference(self):
        blade = Blade(
            widest=0.5, lobes=0, base_fill=0, bend=0, fold=0, twist=0, petiole=0
        )
        self.assertAlmostEqual(
            blade.area(), 2 * blade.length * blade.width / np.pi, places=14
        )
        scale = replace(blade, length=7 * blade.length, width=7 * blade.width)
        self.assertAlmostEqual(scale.area() / blade.area(), 49, places=12)

    def test_metric_matches_independent_position_differences(self):
        for power in (1, 2):
            blade = Blade(fold_power=power, bend=-0.7, twist=1.3)
            t = np.linspace(0.03, 0.97, 47)
            for s in (-0.63, 0.31):
                side, step = np.sign(s), 1e-6
                _, normal, density, du, dv = blade.surface_jet(t, s, side)
                dt = (
                    blade.evaluate(t + step, s, side)[0]
                    - blade.evaluate(t - step, s, side)[0]
                ) / (2 * step)
                ds = (
                    blade.evaluate(t, s + step, side)[0]
                    - blade.evaluate(t, s - step, side)[0]
                ) / (2 * step)
                cross = np.cross(ds, dt)
                lengths = np.linalg.norm(cross, axis=1)
                np.testing.assert_allclose(du, 2 * ds, atol=1e-10)
                np.testing.assert_allclose(dv, dt, atol=1e-10)
                np.testing.assert_allclose(normal, cross / lengths[:, None], atol=2e-8)
                np.testing.assert_allclose(density, lengths, rtol=2e-8)

    def test_open_sheet_has_one_boundary_without_degenerate_faces(self):
        for blade in (
            Blade(),
            Blade(base_fill=0, petiole=0),
            Blade(fold_power=1, width=0.009, length=0.45),
        ):
            near, coarse = blade.mesh(64, 4), blade.mesh(16, 2)
            for mesh in (near, coarse):
                faces = mesh["faces"]
                edges = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
                unique, counts = np.unique(
                    np.sort(edges, axis=1), axis=0, return_counts=True
                )
                self.assertTrue(np.all((counts == 1) | (counts == 2)))
                boundary = unique[counts == 1]
                _, degrees = np.unique(boundary, return_counts=True)
                np.testing.assert_array_equal(degrees, 2)
                self.assertEqual(len(mesh["vertices"]) - len(unique) + len(faces), 1)
                p = mesh["vertices"][faces]
                cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
                self.assertTrue(np.all(np.linalg.norm(cross, axis=1) > 0))
                self.assertTrue(
                    np.all(
                        np.sum(
                            cross[:, None] * mesh["normals"].reshape(-1, 3, 3), axis=2
                        )
                        > 0
                    )
                )
                np.testing.assert_allclose(
                    np.linalg.norm(mesh["normals"], axis=1), 1, atol=1e-12
                )
            by_parameter = {
                tuple(uv): p
                for uv, p in zip(near["parameters"], near["vertices"], strict=True)
            }
            for uv, p in zip(coarse["parameters"], coarse["vertices"], strict=True):
                np.testing.assert_array_equal(p, by_parameter[tuple(uv)])

    def test_fold_and_petiole_have_one_sided_corner_normals(self):
        blade = Blade(fold_power=1, fold=0.2, twist=0)
        p, left, _ = blade.evaluate(0.4, 0, -1)
        q, right, _ = blade.evaluate(0.4, 0, 1)
        np.testing.assert_array_equal(p, q)
        self.assertGreater(np.linalg.norm(left - right), 0.1)
        base = blade.evaluate(0, 0.4, 1, petiole_side=True)[0]
        end = blade.evaluate(-blade.petiole / blade.length, 0.4, 1)[0]
        np.testing.assert_allclose(base - end, [0, blade.petiole, 0], atol=1e-15)
        np.testing.assert_allclose(
            Blade().evaluate(0.4, 0, -1)[1], Blade().evaluate(0.4, 0, 1)[1], atol=0
        )

    def test_cached_quadrature_cannot_be_poisoned(self):
        nodes, weights = quadrature(16)
        for value in (nodes, weights):
            with self.assertRaises(ValueError):
                value.setflags(write=True)
        self.assertAlmostEqual(float(np.sum(weights)), 2, places=14)
        for degree in range(12):
            integral = float(weights @ nodes**degree)
            self.assertAlmostEqual(
                integral, 0 if degree % 2 else 2 / (degree + 1), places=14
            )
        quadrature(np.int64(1))
        for order in (True, 0, 1025, 2.5):
            with self.assertRaises(ValueError):
                quadrature(order)

    def test_underresolved_and_boolean_subdivisions_are_rejected(self):
        blade = Blade(twist=100, lobes=0, bend=0)
        with self.assertRaisesRegex(ValueError, "subdivision"):
            blade.mesh(16, 4)
        self.assertGreater(len(blade.mesh(256, 4)["faces"]), 0)
        for params in ({"lobes": True}, {"fold_power": True}):
            with self.assertRaises(ValueError):
                Blade(**params)
        for params in (
            {"segments": True},
            {"across": True},
            {"across": 0},
            {"segments": 1},
        ):
            with self.assertRaises(ValueError):
                Blade().mesh(**params)


if __name__ == "__main__":
    unittest.main()
