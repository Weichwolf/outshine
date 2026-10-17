"""Independent positive-part identities, sharp bounds and actual source coupling."""

import unittest

import numpy as np
from needle_area_checks import sheet
from needle_area_light import integrate
from normal_lobes import (
    NormalMeasure,
    error_bound,
    evaluate,
    mass_preserving_bound,
    mass_preserving_vectors,
    quantization_bound,
)
from scatter_scene import ScatterScene


class NormalLobeChecks(unittest.TestCase):
    def test_flat_grazing_and_backlight_have_no_leak(self):
        measure = NormalMeasure(1, np.eye(3))
        measure.add(np.array([0]), np.array([1.0]), np.array([[0.0, 0, 1]]))
        for field in measure.moments.values():
            values = evaluate(
                field[..., 1:], np.array([[1, 0, 0], [0, 0, 1], [0, 0, -1.0]])
            )
            np.testing.assert_array_equal(values[:, 0], [0, 1, 0])
            np.testing.assert_array_equal(error_bound(field), [0])

    def test_same_mean_different_distributions_remain_distinguishable(self):
        first = np.array([[np.sqrt(3) / 2, 0, 0.5], [-np.sqrt(3) / 2, 0, 0.5]])
        second = first[:, [1, 0, 2]]
        results = []
        for normals in (first, second):
            measure = NormalMeasure(1, [[1.0, 0, 0]])
            measure.add(np.array([0, 0]), np.array([0.5, 0.5]), normals)
            results.append(evaluate(measure.moments[6][..., 1:], [[1.0, 0, 0]])[0, 0])
        self.assertAlmostEqual(results[0], np.sqrt(3) / 4)
        self.assertEqual(results[1], 0)

    def test_tight_within_bin_jensen_gap(self):
        a = 0.2
        z = np.sqrt(1 - a * a)
        measure = NormalMeasure(1, [[1.0, 0, 0]])
        measure.add(
            np.array([0, 0]), np.array([0.5, 0.5]), np.array([[a, 0, z], [-a, 0, z]])
        )
        field = measure.moments[6]
        self.assertEqual(evaluate(field[..., 1:], [[1.0, 0, 0]])[0, 0], 0)
        self.assertAlmostEqual(measure.exact[0, 0], a / 2)
        self.assertAlmostEqual(error_bound(field)[0], a / 2)

    def test_random_bounds_nested_bins_and_scalar_reference(self):
        rng = np.random.default_rng(643)
        normals = rng.normal(size=(71, 3))
        normals /= np.linalg.norm(normals, axis=1)[:, None]
        directions = rng.normal(size=(33, 3))
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        weights = rng.random(71) / 71
        measure = NormalMeasure(1, directions)
        measure.add(np.zeros(71, int), weights, normals)
        exact = np.array(
            [
                sum(
                    w * max(float(n @ d), 0)
                    for w, n in zip(weights, normals, strict=True)
                )
                for d in directions
            ]
        )
        np.testing.assert_allclose(measure.exact[:, 0], exact, atol=1e-15)
        for field in measure.moments.values():
            candidate = evaluate(field[..., 1:], directions)[:, 0]
            self.assertTrue(np.all(candidate >= 0))
            self.assertLessEqual(float(np.max(candidate - exact)), 1e-14)
            self.assertLessEqual(
                float(np.max(exact - candidate - error_bound(field)[0])), 1e-14
            )
        coarse = evaluate(measure.moments[6][..., 1:], directions)
        fine = evaluate(measure.moments[24][..., 1:], directions)
        self.assertGreaterEqual(float((fine - coarse).min()), -1e-14)

    def test_quantization_and_rigid_transform_bounds(self):
        measure = NormalMeasure(1, np.eye(3))
        measure.add(
            np.array([0, 0]),
            np.array([0.371, 0.229]),
            np.array([[0.6, 0, 0.8], [0, 0.8, 0.6]]),
        )
        vectors = measure.moments[6][..., 1:]
        packed = vectors.astype(np.float16)
        delta = abs(evaluate(vectors, np.eye(3)) - evaluate(packed, np.eye(3)))
        self.assertLessEqual(
            float((delta - quantization_bound(vectors, packed)).max()), 1e-15
        )
        rotation = np.array([[0.0, 1, 0], [0, 0, 1], [1, 0, 0]])
        np.testing.assert_allclose(
            evaluate(vectors, np.eye(3)),
            evaluate(vectors @ rotation, np.eye(3) @ rotation),
            atol=1e-15,
        )

    def test_existing_source_measure_is_shared_without_changing_response(self):
        source = sheet()
        scene = ScatterScene(source["vertices"], source["faces"])
        measure = NormalMeasure(4, np.eye(3))
        base, _ = integrate(source, scene, np.array([0.0, 0, 1]), [[0, 0, 1]], size=4)
        result, _ = integrate(
            source,
            scene,
            np.array([0.0, 0, 1]),
            [[0, 0, 1]],
            size=4,
            normal_measure=measure,
        )
        np.testing.assert_array_equal(base, result)
        np.testing.assert_allclose(
            measure.exact[2].reshape(4, 4), result[1], atol=1e-15
        )

    def test_invalid_unit_measure_and_corrupted_cone(self):
        measure = NormalMeasure(1, np.eye(3))
        with self.assertRaises(ValueError):
            measure.add(np.array([0]), np.array([1.0]), np.zeros((1, 3)))
        bad = np.zeros((1, 6, 4))
        bad[0, 0, 1] = 1
        with self.assertRaises(ValueError):
            error_bound(bad)

    def test_mass_preservation_and_bound_without_fitted_lights(self):
        rng = np.random.default_rng(12643)
        normals = rng.normal(size=(83, 3))
        normals /= np.linalg.norm(normals, axis=1)[:, None]
        directions = rng.normal(size=(41, 3))
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        weights = rng.random(83) / 83
        measure = NormalMeasure(1, directions)
        measure.add(np.zeros(83, int), weights, normals)
        for field in measure.moments.values():
            vectors = mass_preserving_vectors(field)
            self.assertAlmostEqual(
                float(np.linalg.norm(vectors, axis=-1).sum()),
                float(weights.sum()),
                places=14,
            )
            error = abs(evaluate(vectors, directions) - measure.exact)
            self.assertLessEqual(
                float((error - mass_preserving_bound(field)).max()), 1e-14
            )


if __name__ == "__main__":
    unittest.main()
