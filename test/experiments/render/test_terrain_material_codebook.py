"""Reusable joint labels, roughness separation and bounded static fitting."""

import unittest

import numpy as np
from relief_shading_reference import direction
from terrain_material_codebook import fit
from terrain_material_filter import response as reference
from terrain_material_moments import accumulate, response


class MaterialCodebookTests(unittest.TestCase):
    def test_constant_and_four_discrete_materials_need_no_extra_labels(self):
        raw = np.array(
            [
                [1, 0, 0, 0.05, 0, 0],
                [1, 0, 0, 0.8, 0, 0],
                [0, 0, 1, 0.2, 1, 0],
                [0, 1, 0, 0.6, -1, 0],
            ],
            dtype=float,
        )
        book = fit(raw)
        self.assertEqual(len(np.unique(book.classify(raw))), 4)
        self.assertEqual(len(fit(raw[:1]).centres), 1)
        with self.assertRaises(ValueError):
            book.centres.flags.writeable = True
        lights, view = np.array([direction(0), direction(60, 30)]), direction(0)
        weights = np.full(4, 0.25)
        np.testing.assert_allclose(
            response(accumulate(raw, weights, codebook=book), lights, view),
            reference(raw, weights, lights, view),
            rtol=1e-11,
            atol=1e-12,
        )

    def test_same_normal_different_roughness_is_not_one_sector(self):
        raw = np.array([[0.4, 0.4, 0.4, r, 0, 0] for r in (0.05, 0.8)])
        light, view, weights = np.array([direction(0)]), direction(0), np.full(2, 0.5)
        expected = reference(raw, weights, light, view)
        learned = response(accumulate(raw, weights, codebook=fit(raw)), light, view)
        fixed = response(accumulate(raw, weights), light, view)
        np.testing.assert_allclose(learned, expected, rtol=1e-10)
        self.assertGreater(float(np.max(np.abs(fixed - expected))), 100)

    def test_fit_is_repeatable_and_classification_is_order_independent(self):
        rng = np.random.default_rng(5)
        raw = rng.random((256, 6))
        first, second = fit(raw), fit(raw)
        np.testing.assert_array_equal(first.centres, second.centres)
        labels = first.classify(raw)
        permutation = rng.permutation(len(raw))
        np.testing.assert_array_equal(
            first.classify(raw[permutation]), labels[permutation]
        )
        self.assertTrue(np.all((labels >= 0) & (labels < 4)))

    def test_fit_rejects_missing_and_nonfinite_source_data(self):
        for raw in (np.empty((0, 6)), np.zeros((2, 5)), np.full((2, 6), np.nan)):
            with self.assertRaises(ValueError):
                fit(raw)


if __name__ == "__main__":
    unittest.main()
