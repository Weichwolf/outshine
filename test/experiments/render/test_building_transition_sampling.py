"""Complementary cut ownership, expected measures and pre-material work control."""

import itertools
import unittest

import numpy as np
from building_mass_materials import policy
from building_material_reference import face_materials
from building_transition_sampling import owned_masks, ownership_ranks, sum_owned
from building_tree_transfer import transfers


class BuildingTransitionSampling(unittest.TestCase):
    def test_per_pixel_strata_are_complementary_and_nested(self):
        ranks, size = ownership_ranks(3, 2, rate=4)
        previous = np.zeros(len(ranks), bool)
        for weight in (0, 0.25, 0.5, 0.75, 1):
            first, second, actual = owned_masks(ranks, size, weight)
            self.assertEqual(actual, weight)
            self.assertTrue(np.all(first ^ second))
            self.assertFalse(np.any(previous & ~second))
            counts = second.reshape(2, 4, 3, 4).sum(axis=(1, 3))
            np.testing.assert_array_equal(counts, np.full((2, 3), weight * 16))
            previous = second
        np.testing.assert_array_equal(ranks, ownership_ranks(3, 2, rate=4)[0])
        self.assertLessEqual(abs(owned_masks(ranks, size, 0.13)[2] - 0.13), 1 / 32)
        with self.assertRaises(ValueError):
            owned_masks(ranks, size, 1.1)
        with self.assertRaises(ValueError):
            ownership_ranks(0, 2)

    def test_pixel_ownership_keeps_every_pixel_indivisible(self):
        ranks, size = ownership_ranks(3, 2, rate=4, granularity="pixel")
        _, mask, actual = owned_masks(ranks, size, 0.5)
        counts = mask.reshape(2, 4, 3, 4).sum(axis=(1, 3))
        self.assertTrue(np.all((counts == 0) | (counts == 16)))
        self.assertEqual(np.count_nonzero(counts), 3)
        self.assertEqual(actual, 0.5)

    def test_uniform_subset_expectation_is_complete_layer_mix(self):
        first = {"C": np.arange(12).reshape(4, 3), "T": np.array([0, 1, 0, 1.0])}
        second = {"C": np.arange(12, 24).reshape(4, 3), "T": np.array([1, 0, 1, 0.0])}
        values = []
        for subset in itertools.combinations(range(4), 2):
            mask = np.zeros(4, bool)
            mask[list(subset)] = True
            a = {k: v[~mask].sum(axis=0) / 4 for k, v in first.items()}
            b = {k: v[mask].sum(axis=0) / 4 for k, v in second.items()}
            values.append(sum_owned(a, b))
        for key in ("C", "T"):
            expected = (first[key].mean(axis=0) + second[key].mean(axis=0)) / 2
            np.testing.assert_array_equal(
                np.mean([v[key] for v in values], axis=0), expected
            )

    def test_owned_original_and_group_measures_sum_without_opaque_holes(self):
        camera = (np.zeros(3), np.diag([-1, 1, 1]), 1, 1, 1)
        box = np.array([[0, 0], [1, 1]])
        mesh = {
            "positions": np.array([[-10, -10, 4], [0, 10, 4], [10, -10, 4.0]]),
            "indices": np.array([[0, 1, 2]]),
            "uv": np.zeros((3, 2)),
            "colour": np.ones((3, 4)),
            "binding": np.array([[0, 1, 0, 0, 0, 0]]),
        }
        at = policy()
        fields, needs = face_materials(mesh, at)
        self.assertFalse(needs)
        weights = np.zeros((1, 2, len(at["palette"])))
        weights[:, :, 0] = 1
        tables = {
            "shape": (2, 2),
            "kGgxAlbedo": np.ones(4),
            "kGgxAlbedoAverage": np.ones(2),
        }
        first, second, _ = owned_masks(*ownership_ranks(1, 1, rate=4), 0.5)
        for native in (False, True):
            product = {
                "indices": np.empty((0, 3), int) if native else mesh["indices"],
                "needs_sources": np.array([1] if native else [], np.uint32),
                "triangle_groups": np.empty(0, int) if native else np.array([0]),
                "group_coverage": np.empty((0, 3)) if native else np.ones((1, 3)),
            }
            appearance = {
                "albedo": np.empty((0, 2, 3)) if native else np.full((1, 2, 3), 0.2),
                "roughness_weights": weights[:0] if native else weights,
                "roughness_palette": at["palette"],
                "supported": np.ones(0 if native else 1, bool),
            }

            def render(mask, product=product, appearance=appearance):
                return transfers(
                    mesh,
                    mesh,
                    fields,
                    product,
                    appearance,
                    camera,
                    box,
                    tables,
                    4,
                    light=[0, 0, -1],
                    sample_mask=mask,
                )

            full, full_work = render(None)
            a, aw = render(first)
            b, bw = render(second)
            joined = sum_owned(a, b)
            np.testing.assert_allclose(joined["C"], full["C"], rtol=0, atol=1e-14)
            np.testing.assert_array_equal(joined["T"], full["T"])
            np.testing.assert_array_equal(joined["T"], 0)
            self.assertGreater(full_work["ggx_lobe_evaluations"], 0)
            self.assertEqual(
                aw["ggx_lobe_evaluations"] + bw["ggx_lobe_evaluations"],
                full_work["ggx_lobe_evaluations"],
            )
            empty, work = render(np.zeros(16, bool))
            np.testing.assert_array_equal(empty["C"], 0)
            np.testing.assert_array_equal(empty["T"], 0)
            self.assertEqual(work["ggx_lobe_evaluations"], 0)
            with self.assertRaises(ValueError):
                render(np.zeros(16, float))


if __name__ == "__main__":
    unittest.main()
