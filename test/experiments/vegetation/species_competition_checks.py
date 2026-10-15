"""Independent score/probability, order, zero-support and partial-cache controls."""

import hashlib
import json
import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import SpeciesHeightMixture
from scipy.special import ndtr
from species_competition import CompetingMarks, gumbel_scores, taxon_seed
from species_spatial import FourierMarks


class CompetingMarksChecks(unittest.TestCase):
    def test_taxon_seed_message_and_roles_are_explicit(self):
        message = (
            b"outshine-species-race-1\0"
            + (17).to_bytes(8, "little")
            + b"field\0taxon-A"
        )
        self.assertEqual(
            taxon_seed("taxon-A", 17, "field"),
            int.from_bytes(hashlib.blake2b(message, digest_size=8).digest(), "little"),
        )
        self.assertNotEqual(
            taxon_seed("taxon-A", 17, "field"), taxon_seed("taxon-A", 17, "site")
        )
        self.assertNotEqual(
            taxon_seed("taxon-A", 17, "field"), taxon_seed("taxon-B", 17, "field")
        )

    def test_log_scores_match_independent_exponential_race_and_tiny_support(self):
        w, u = np.array((1, 3, 6.0)), np.array((0.2, 0.7, 0.3))
        times = -np.log(u) / w
        self.assertEqual(int(np.argmax(gumbel_scores(w, u))), int(np.argmin(times)))
        np.testing.assert_allclose(gumbel_scores(w, u), -np.log(times), atol=1e-15)
        self.assertTrue(np.isneginf(gumbel_scores((0,), (0.9,))[0]))
        self.assertTrue(np.isfinite(gumbel_scores((5e-324,), (0,))[0]))
        with self.assertRaises(ValueError):
            gumbel_scores((1,), (1,))

    def test_independent_gaussian_marginals_give_weighted_categories(self):
        u = ndtr(np.random.default_rng(73).normal(size=(32768, 3)))
        w = np.broadcast_to((1, 3, 6), u.shape)
        selected = np.argmax(gumbel_scores(w, u), axis=1)
        np.testing.assert_allclose(
            np.bincount(selected, minlength=3) / len(u),
            (0.1, 0.3, 0.6),
            atol=0.015,
            rtol=0,
        )

    def test_entry_order_and_zero_weight_added_taxon_preserve_named_winner(self):
        mix = SpeciesHeightMixture.prepare(("a", "b"), (0, 100), ((1, 3), (3, 1)))
        marks = CompetingMarks.prepare(("b", "a"), 73, count=8)
        keys = np.arange(200, dtype=np.uint64)[:, None]
        p = np.column_stack((keys[:, 0] * 3, np.zeros(200), np.zeros(200))) + (
            4090000,
            800000,
            4800000,
        )
        h = np.linspace(-20, 120, 200)
        expected = marks.choose(mix, p, keys, h)
        np.testing.assert_array_equal(
            expected, CompetingMarks(marks.entries[::-1]).choose(mix, p, keys, h)
        )
        np.testing.assert_array_equal(
            expected, marks.choose(mix, p, keys, h, score_mode="matrix")
        )
        bigger = SpeciesHeightMixture.prepare(
            ("a", "b", "aa"), (0, 100), ((1, 3, 0), (3, 1, 0))
        )
        more = CompetingMarks.prepare(("aa", "b", "a"), 73, count=8)
        actual = more.choose(bigger, p, keys, h)
        self.assertEqual(
            [mix.taxa[i] for i in expected], [bigger.taxa[i] for i in actual]
        )

    def test_prepared_replay_and_fixed_tie_break_are_order_independent(self):
        mix = SpeciesHeightMixture.prepare(("a", "b"), (0, 1), ((1, 1), (1, 1)))
        field = FourierMarks(((0, 0, 0),), ((0, 0),))
        marks = CompetingMarks((("b", field, 17), ("a", field, 17)))
        p, key = np.zeros((10, 3)), np.arange(10)[:, None]
        np.testing.assert_array_equal(
            marks.choose(mix, p, key, np.zeros(10)), np.zeros(10)
        )
        recipe = json.loads(json.dumps(marks.recipe()))
        with (
            patch.object(
                CompetingMarks, "prepare", side_effect=AssertionError("rebuild")
            ),
            patch.object(
                FourierMarks, "prepare", side_effect=AssertionError("rebuild")
            ),
        ):
            cached = CompetingMarks.from_recipe(recipe)
            np.testing.assert_array_equal(
                cached.choose(mix, p[::3], key[::3], np.zeros(4)), np.zeros(4)
            )

    def test_zero_support_skips_field_work_and_retains_missing_result(self):
        mix = SpeciesHeightMixture.prepare(("a",), (0, 1), ((0,), (0,)))
        marks = CompetingMarks.prepare(("a",), 3, count=8)
        with patch.object(
            FourierMarks, "uniforms", side_effect=AssertionError("unused field")
        ):
            np.testing.assert_array_equal(
                marks.choose(mix, np.zeros((2, 3)), np.array(((1,), (2,))), (0, 1)),
                (-1, -1),
            )
        with self.assertRaises(ValueError):
            marks.choose(mix, np.zeros((2, 3)), np.array(((-1,), (2,))), (0, 1))


if __name__ == "__main__":
    unittest.main()
