"""Scalar minimal-capacity decisions, exact unchanged fields and invalid catalogs."""

import copy
import unittest

import numpy as np
from outshine_ref.vegetation import NeedlePattern
from window_capacity_catalog import rebind


def metadata():
    pattern = NeedlePattern(group_size=1, short_length=0.0006).recipe()
    sizes = (256, 128, 64, 32, 16, 8, 4, 2)
    records = {
        str(i): {"family": i, "needles": n, "phase": 0.37, "pattern": pattern}
        for i, n in enumerate(sizes)
    }
    return {"source": records["0"]}, records


class CapacityBindingTests(unittest.TestCase):
    def test_smallest_sufficient_capacity_preserves_all_other_arrays_and_inputs(self):
        old, new = metadata()
        n = np.arange(1, 257, dtype=np.uint32)
        plan = {
            "needle_counts": n,
            "capacities": np.full(256, 256, np.uint32),
            "families": np.zeros(256, np.int32),
            "weights": n / 256,
            "origins": np.arange(768, dtype=float).reshape(-1, 3),
            "first_needle_ids": np.r_[np.uint64(0), np.cumsum(n[:-1], dtype=np.uint64)],
        }
        before = {k: v.copy() for k, v in plan.items()}
        result = rebind(plan, old, new)
        by_size = {v["needles"]: v["family"] for v in new.values()}
        for i, count in enumerate(n):
            expected = min(size for size in by_size if size >= int(count))
            self.assertEqual(result["capacities"][i], expected)
            self.assertEqual(result["families"][i], by_size[expected])
            self.assertEqual(result["weights"][i], int(count) / expected)
        self.assertGreaterEqual(float(result["weights"].min()), 0.5)
        for key, value in plan.items():
            np.testing.assert_array_equal(value, before[key])
            if key not in ("capacities", "families", "weights"):
                self.assertIs(result[key], value)
        self.assertEqual(result["families"][-1], 0)
        self.assertEqual(result["weights"][-1], 1)

    def test_incompatible_or_ambiguous_catalogs_and_counts_are_rejected(self):
        old, new = metadata()
        plan = {
            "needle_counts": np.array((1, 256), np.uint32),
            "capacities": np.array((256, 256), np.uint32),
            "families": np.zeros(2, np.int32),
            "weights": np.array((1 / 256, 1)),
        }
        changed = copy.deepcopy(new)
        changed["1"]["phase"] += 0.1
        missing = {k: v for k, v in new.items() if v["needles"] != 256}
        duplicate = copy.deepcopy(new)
        duplicate["extra"] = copy.deepcopy(new["1"])
        for values in (changed, missing, duplicate):
            with self.assertRaises(ValueError):
                rebind(plan, old, values)
        for count in (0, 257):
            bad = copy.deepcopy(plan)
            bad["needle_counts"][0] = count
            with self.assertRaises(ValueError):
                rebind(bad, old, new)


if __name__ == "__main__":
    unittest.main()
