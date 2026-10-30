"""Count preservation and literal preprint parameter checks, no SAT benchmark."""

import itertools
import unittest

from recipe_cnf_screen import choice_instance, exactly_one, parameters


class RecipeScreenTests(unittest.TestCase):
    def test_unique_auxiliary_extension_preserves_every_exactly_one_choice(self):
        for width in range(1, 9):
            clauses, next_variable = exactly_one(tuple(range(1, width + 1)), width + 1)
            counts = {}
            for bits in itertools.product((False, True), repeat=next_variable - 1):
                if all(
                    any(bits[abs(v) - 1] == (v > 0) for v in clause)
                    for clause in clauses
                ):
                    key = bits[:width]
                    counts[key] = counts.get(key, 0) + 1
            expected = {
                tuple(i == chosen for i in range(width)): 1 for chosen in range(width)
            }
            self.assertEqual(counts, expected)
            self.assertTrue(all(1 <= len(c) <= 3 for c in clauses))
            self.assertEqual(next_variable - 1, width + max(0, width - 3))

    def test_actual_whole_station_request_is_not_mislabeled_as_physical_sat(self):
        groups, clauses, count, report = choice_instance()
        self.assertEqual(report["stations"], 94)
        self.assertEqual(len(report["source_ids"]), 3)
        self.assertEqual(report["physical_candidates_most"], 470)
        self.assertEqual(report["choice_variables"], 564)
        self.assertEqual(report["unique_or_auxiliaries"], 282)
        self.assertEqual(count, 846)
        self.assertFalse(report["physical_feasibility_known"])
        self.assertTrue(all(len(g["choice_variables"]) == 6 for g in groups))
        self.assertEqual(len(clauses), 2350 + report["source_domain_forbidden"])
        self.assertTrue(all(1 <= abs(v) <= count for c in clauses for v in c))
        check = parameters(count, clauses)
        self.assertFalse(check["interpolation"]["necessary_r_at_least_2"])
        self.assertEqual(check["tensor"]["root_u"], 0)

    def test_tensor_wire_count_and_direct_summation_gate(self):
        result = parameters(3, [(1, 2), (-1, 3)])
        self.assertEqual(result["tensor"]["s_n_plus_m"], 5)
        self.assertEqual(result["tensor"]["fixed_density_bipartite_wires"], 8)
        self.assertEqual(result["tensor"]["extension_degree"], 12)
        self.assertEqual(result["tensor"]["root_u"], 0)

    def test_a_single_necessary_parameter_threshold_is_not_a_crossover(self):
        # Exact integer checks without constructing an instance of this size.
        result = parameters((1 << 521283) + 1, [(1,)])
        self.assertEqual(result["interpolation"]["r"], 2)
        self.assertFalse(result["interpolation"]["necessary_Q_at_least_6R"])
        result = parameters(1 << 131071, [(1,)])
        self.assertEqual(result["tensor"]["lambda"], 131072)
        self.assertEqual(result["tensor"]["root_u"], 1)

    def test_invalid_empty_domains_do_not_create_a_counting_result(self):
        with self.assertRaises(ValueError):
            exactly_one((), 1)
        for n, clauses in ((0, [(1,)]), (1, [])):
            with self.assertRaises(ValueError):
                parameters(n, clauses)


if __name__ == "__main__":
    unittest.main()
