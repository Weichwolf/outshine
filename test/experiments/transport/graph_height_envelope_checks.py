"""Independent scalar-model checks; no geometry or source admission."""

import unittest

import numpy as np
from graph_height_envelope_audit import lazy_witness
from graph_height_envelopes import envelope, lp_reference, query


def chain(weights):
    graph = [[] for _ in range(len(weights) + 1)]
    for i, weight in enumerate(weights):
        graph[i].append((i + 1, weight))
        graph[i + 1].append((i, weight))
    return graph


class HeightEnvelopeChecks(unittest.TestCase):
    def test_two_observations_have_the_independent_expected_optimal_error(self):
        values = np.array([0.0, 10.0])
        upper, _ = envelope(chain([2.0]), values)
        minus_lower, _ = envelope(chain([2.0]), -values)
        np.testing.assert_array_equal(upper, [0, 2])
        np.testing.assert_array_equal(-minus_lower, [8, 10])
        np.testing.assert_array_equal((upper - minus_lower) / 2, [4, 6])
        self.assertEqual(float((-minus_lower - upper).max()) / 2, 4)

    def test_missing_prior_is_not_a_zero_observation(self):
        values = np.array([0.0, np.nan, 10.0])
        upper, _ = envelope(chain([2.0, 2.0]), values)
        negative, _ = envelope(chain([2.0, 2.0]), -values)
        np.testing.assert_array_equal((upper - negative) / 2, [3, 5, 7])

    def test_unsampled_disconnected_component_has_no_invented_absolute_height(self):
        upper, _ = envelope([[], []], np.array([3.0, np.nan]))
        negative, _ = envelope([[], []], np.array([-3.0, np.nan]))
        self.assertEqual(upper[0], 3)
        self.assertEqual(negative[0], -3)
        self.assertTrue(np.isinf(upper[1]))
        self.assertTrue(np.isinf(negative[1]))

    def test_variable_weights_match_independent_lp_and_bounded_queries(self):
        rng = np.random.default_rng(240)
        for count in (3, 7, 12):
            weights = rng.uniform(0.01, 4, count - 1)
            values = rng.uniform(-5, 20, count)
            values[1] = np.nan
            graph = chain(weights)
            upper, _ = envelope(graph, values)
            negative, _ = envelope(graph, -values)
            rho = float(max(0.0, (-negative - upper).max())) / 2
            lp, _ = lp_reference(
                values, np.c_[np.arange(count - 1), np.arange(1, count)], weights
            )
            self.assertAlmostEqual(rho, lp[-1], places=7)
            for origin in range(count):
                lo, hi, work = query(
                    graph,
                    values,
                    origin,
                    float(np.nanmin(values)),
                    float(np.nanmax(values)),
                    1e-8,
                    10000,
                )
                self.assertTrue(work["complete"])
                self.assertAlmostEqual(lo, -negative[origin], places=7)
                self.assertAlmostEqual(hi, upper[origin], places=7)

    def test_lazy_negative_witness_uses_no_global_extrema(self):
        calls = []

        def sample(node):
            calls.append(node)
            return [0.0, 10.0, 30.0][node]

        result = lazy_witness(
            chain([2.0, 20.0]), sample, 0, 0.5, max_samples=3, max_visits=10
        )
        self.assertEqual(result["state"], "infeasible")
        self.assertEqual(calls, [0, 1])
        self.assertFalse(result["global_extrema_used"])
        self.assertAlmostEqual(result["empty_interval_gap_m"], 7.0)

    def test_cycle_with_branches_matches_lp_and_every_bounded_query(self):
        edges = np.array([[0, 1], [1, 2], [2, 0], [1, 3], [3, 4]])
        weights = np.array([1.0, 0.4, 1.2, 3.0, 2.0])
        values = np.array([-3.0, np.nan, 7.0, 1.0, 20.0])
        graph = [[] for _ in values]
        for (a, b), weight in zip(edges, weights):
            graph[a].append((int(b), float(weight)))
            graph[b].append((int(a), float(weight)))
        upper, _ = envelope(graph, values)
        negative, _ = envelope(graph, -values)
        fit = (upper - negative) / 2
        rho = float((-negative - upper).max()) / 2
        lp, _ = lp_reference(values, edges, weights)
        self.assertAlmostEqual(rho, lp[-1], places=7)
        self.assertLessEqual(
            float(np.max(abs(fit[edges[:, 0]] - fit[edges[:, 1]]) - weights)), 1e-8
        )
        for origin in range(len(values)):
            lo, hi, work = query(graph, values, origin, -3.0, 20.0, 1e-8, 100)
            self.assertTrue(work["complete"])
            self.assertAlmostEqual(lo, -negative[origin], places=7)
            self.assertAlmostEqual(hi, upper[origin], places=7)

    def test_query_on_unobserved_component_has_unbounded_interval(self):
        lower, upper, work = query(
            [[], []], np.array([3.0, np.nan]), 1, 3.0, 3.0, 1e-8, 0
        )
        self.assertTrue(work["complete"])
        self.assertEqual(lower, -np.inf)
        self.assertEqual(upper, np.inf)

    def test_lazy_missing_observation_is_not_an_absolute_height(self):
        result = lazy_witness(
            chain([2.0]), lambda node: None, 0, 0.5, max_samples=2, max_visits=2
        )
        self.assertEqual(result["state"], "exhausted")
        self.assertEqual(result["observations"], [])
        self.assertEqual(result["partial_lower_m"], -np.inf)
        self.assertEqual(result["partial_upper_m"], np.inf)

    def test_lazy_budget_does_not_invent_a_certificate(self):
        result = lazy_witness(
            chain([2.0, 20.0]),
            lambda node: [0.0, 10.0, 30.0][node],
            0,
            0.5,
            max_samples=1,
            max_visits=10,
        )
        self.assertEqual(result["state"], "budget")
        self.assertEqual(result["sample_attempts"], 1)

    def test_exhausted_work_budget_is_incomplete(self):
        _, _, work = query(
            chain([1.0, 1.0, 1.0]), np.array([0.0, 1.0, 2.0, 3.0]), 0, 0.0, 3.0, 1e-8, 1
        )
        self.assertFalse(work["complete"])
        self.assertEqual(work["edge_visits"], 1)


if __name__ == "__main__":
    unittest.main()
