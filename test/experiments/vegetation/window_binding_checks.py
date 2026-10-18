"""Per-order binding work, lazy catalog use and independent physical field controls."""

import math
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from conifer_windows import bind_patterns, pattern_key
from outshine_ref.vegetation import NeedlePattern
from spruce_growth_product import pack_axes, windows


class PatternBindingTests(unittest.TestCase):
    def test_only_occupied_orders_are_serialized_and_fields_keep_scalar_meaning(self):
        patterns = (
            None,
            NeedlePattern(),
            NeedlePattern(spacing=0.002),
            NeedlePattern(spacing=0.003),
        )
        catalog = {
            pattern_key(patterns[1]): {"family": 7, "phase": 0.3, "needles": 256},
            pattern_key(patterns[2]): {"family": 9, "phase": -0.2, "needles": 128},
        }
        plan = {
            "orders": np.array([2, 1, 1, 2], np.uint8),
            "capacities": np.array([128, 256, 256, 128]),
            "needle_counts": np.array([8, 256, 27, 128]),
            "phases": np.array([0.1, 0.5, -0.7, 1.2]),
            "frames": np.broadcast_to(np.eye(3), (4, 3, 3)),
            "origins": np.zeros((4, 3)),
        }
        with patch("conifer_windows.pattern_key", wraps=pattern_key) as keys:
            actual = bind_patterns(patterns, plan, catalog)
        self.assertEqual(keys.call_count, 2)
        self.assertEqual(
            [c.args[0] for c in keys.call_args_list], [patterns[2], patterns[1]]
        )
        np.testing.assert_array_equal(actual["families"], [9, 7, 7, 9])
        self.assertEqual(actual["families"].dtype, np.dtype(np.int32))
        for i, phase in enumerate(plan["phases"]):
            a = -(phase - (-0.2 if plan["orders"][i] == 2 else 0.3))
            c, s = math.cos(a), math.sin(a)
            np.testing.assert_allclose(
                actual["frames"][i], [[c, 0, s], [0, 1, 0], [-s, 0, c]], atol=1e-15
            )
        np.testing.assert_array_equal(actual["weights"], [8 / 128, 1, 27 / 256, 1])
        self.assertIs(actual["origins"], plan["origins"])
        np.testing.assert_array_equal(
            plan["frames"], np.broadcast_to(np.eye(3), (4, 3, 3))
        )

    def test_empty_plan_needs_no_catalog_entries(self):
        plan = {
            "orders": np.array([], np.uint8),
            "capacities": np.array([]),
            "needle_counts": np.array([]),
            "phases": np.array([]),
            "frames": np.empty((0, 3, 3)),
        }
        with patch(
            "conifer_windows.pattern_key", side_effect=AssertionError("unused pattern")
        ):
            result = bind_patterns((None, NeedlePattern()), plan, {})
        self.assertEqual(result["families"].shape, (0,))
        self.assertEqual(result["frames"].shape, (0, 3, 3))

    def test_capacity_error_is_still_checked_for_every_window(self):
        p = NeedlePattern()
        plan = {
            "orders": np.array([1, 1], np.uint8),
            "capacities": np.array([256, 128]),
        }
        with self.assertRaisesRegex(ValueError, "every physical window"):
            bind_patterns((None, p), plan, {pattern_key(p): {"needles": 256}})

    def test_axis_window_key_work_follows_patterns_not_axis_count(self):
        p = NeedlePattern()
        poses = [
            SimpleNamespace(
                path=((0, 0), (1, 0)),
                origin=np.zeros(3),
                frame=np.eye(3),
                scale=1.0,
                length=1.0,
                bend=0.0,
            )
        ]
        poses += [
            SimpleNamespace(
                path=((0, 0), (1, 0), (2, i)),
                origin=np.zeros(3),
                frame=np.eye(3),
                scale=1.0,
                length=0.4 + i / 100,
                bend=0.0,
            )
            for i in range(30)
        ]
        counts = [0] + [
            math.floor((p.last - p.first) * pose.length / p.spacing) + 1
            for pose in poses[1:]
        ]
        axes = pack_axes(poses, counts)
        needed = sum((n + 255) // 256 for n in counts)
        catalog = {pattern_key(p): {"needles": 256, "phase": 0.2, "family": 4}}
        with (
            patch("spruce_growth_product.pattern_key", wraps=pattern_key) as axis_keys,
            patch("conifer_windows.pattern_key", wraps=pattern_key) as binding_keys,
        ):
            result = windows(axes, (None, p), catalog, 815, needed)
        self.assertEqual(axis_keys.call_count, 1)
        self.assertEqual(binding_keys.call_count, 1)
        self.assertEqual(len(result["origins"]), needed)
        self.assertEqual(int(result["needle_counts"].sum()), sum(counts))
        np.testing.assert_array_equal(
            result["first_needle_ids"][1:],
            result["first_needle_ids"][:-1] + result["needle_counts"][:-1],
        )
        with self.assertRaises(ValueError):
            windows(axes, (None, p), catalog, 815, needed - 1)


if __name__ == "__main__":
    unittest.main()
