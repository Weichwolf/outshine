"""Priority preserves proposals, identities, budget opportunities and true predicate."""

import unittest
from types import SimpleNamespace

import numpy as np
from site_order import envelope_radius, priority
from site_order_query import QueryCounter


class SiteOrderChecks(unittest.TestCase):
    def test_radius_encloses_all_corners_of_an_offset_prototype(self):
        bounds = np.array(((-2, -1, 0), (1, 3, 5)))
        radius = envelope_radius(bounds)
        self.assertGreaterEqual(radius, np.sqrt(13))
        for x in bounds[:, 0]:
            for y in bounds[:, 1]:
                self.assertLessEqual(np.linalg.norm((x, y)), radius)
        for invalid in (
            np.zeros((3, 2)),
            ((1, 1, 1), (0, 0, 0)),
            np.full((2, 3), np.nan),
        ):
            with self.assertRaises(ValueError):
                envelope_radius(invalid)

    def test_priority_keeps_zero_distance_and_original_objects_with_stable_ties(self):
        pool = [
            (3.0, "c", (1,), object()),
            (0.0, "a", (2,), object()),
            (2.0, "b", (3,), object()),
            (2.0, "a", (4,), object()),
        ]
        ordered = priority(pool, ((-1, 0, 0), (1, 0, 3)), "boundary")
        self.assertEqual([r[1] for r in ordered], ["a", "b", "c", "a"])
        self.assertEqual({id(r) for r in pool}, {id(r) for r in ordered})
        self.assertIs(ordered[-1], pool[1])
        self.assertEqual(pool[0][1], "c")

    def test_counter_retains_original_conflict_and_cost_results(self):
        result = object()
        cost = {
            "query_rows": 10,
            "xy_candidates": 2,
            "height_candidates": 1,
            "positive_conflicts": 1,
        }
        calls = []

        def conflict(shape, **options):
            calls.append((shape, options))
            return result, cost

        query = SimpleNamespace(shapes=object(), tree=object(), conflict=conflict)
        counted = QueryCounter(query)
        shape = object()
        value, measured = counted.conflict(shape, method="scan")
        self.assertIs(value, result)
        self.assertIs(measured, cost)
        self.assertEqual(calls, [(shape, {"method": "scan"})])
        self.assertEqual(counted.counts, dict(cost, calls=1))


if __name__ == "__main__":
    unittest.main()
