"""Outer bounds retain independent ray hits and preserve source preparation."""

import copy
import unittest

import numpy as np
from building_trace_bounds import (
    local_part_bounds,
    ray_candidates,
    source_local_rays,
    trace_after_require,
    union_bound,
)
from test_cube_capture_mask import exact_ray_box


def surface():
    return {
        "projectable": True,
        "origin": [0, 0, 0],
        "east": [1, 0, 0],
        "north": [0, 1, 0],
        "up": [0, 0, 1],
        "shapes": [
            {
                "ring": [[0, 0], [2, 0], [2, 1], [0, 1]],
                "holes": [],
                "bottom_m": -2,
                "roof_deck_m": 8,
                "rise_m": 3,
                "roof_kind": 0,
            }
        ],
    }


class BuildingTraceBounds(unittest.TestCase):
    def test_flat_deck_already_contains_rise_and_inputs_stay_unchanged(self):
        original = surface()
        before = copy.deepcopy(original)
        parts = local_part_bounds(original, padding_m=0)
        self.assertEqual(original, before)
        self.assertLess(parts[0, 0, 2], -2)
        self.assertGreater(parts[0, 1, 2], 8)
        self.assertLess(parts[0, 1, 2], 8.001)
        self.assertFalse(parts.flags.writeable)
        original["shapes"][0]["roof_kind"] = 1
        self.assertGreater(local_part_bounds(original, padding_m=0)[0, 1, 2], 11)

    def test_original_holes_are_outer_bound_inputs_not_filled_coverage(self):
        original = surface()
        original["shapes"][0]["holes"] = [[[-4, -3], [-3, -3], [-3, -2]]]
        bound = union_bound(local_part_bounds(original, padding_m=0.25))
        self.assertLessEqual(bound[0, 0], -4.25)
        self.assertLessEqual(bound[0, 1], -3.25)
        self.assertTrue(ray_candidates(bound, [[-3.5, -4, 0]], [[0, 1, 0]])[0])

    def test_unknown_or_unsupported_data_delegates_to_the_existing_trace(self):
        self.assertIsNone(local_part_bounds(None, padding_m=0.25))
        self.assertIsNone(local_part_bounds(surface(), padding_m=None))
        original = surface()
        original["shapes"][0]["roof_kind"] = 6
        self.assertIsNone(local_part_bounds(original, padding_m=0.25))
        self.assertTrue(ray_candidates(None, [[100, 100, 100]], [[1, 0, 0]])[0])

    def test_local_transform_is_batch_independent(self):
        original = surface()
        original.update(
            origin=[4e6, -3e6, 2e6], east=[0.6, 0.8, 0], north=[-0.8, 0.6, 0]
        )
        origins = np.array([[4e6 + 3, -3e6 - 4, 2e6 + 2], [4e6 - 5, -3e6 + 7, 2e6 - 1]])
        directions = np.array([[0.25, -0.75, 1], [0, 0, 1.0]])
        a, b = source_local_rays(original, origins, directions)
        single = [
            source_local_rays(original, o, d)
            for o, d in zip(origins, directions, strict=True)
        ]
        np.testing.assert_array_equal(a, [row[0] for row in single])
        np.testing.assert_array_equal(b, [row[1] for row in single])

    def test_fraction_ray_hits_are_never_rejected(self):
        rng = np.random.default_rng(1125)
        origins = rng.uniform(-4, 4, (1024, 3))
        directions = rng.uniform(-1, 1, (1024, 3))
        directions[::8, 0] = 0
        box = np.array([[-1.0, -2, -0.5], [1.0, 2, 0.5]])
        actual = ray_candidates(box, origins, directions)
        exact = np.array(
            [exact_ray_box(box, o, d) for o, d in zip(origins, directions, strict=True)]
        )
        self.assertGreater(exact.sum(), 20)
        self.assertFalse((exact & ~actual).any())

    def test_parallel_tangent_and_finite_maximum_are_conservative(self):
        box = np.array([[0.0, 0, 0], [1.0, 1, 1]])
        origins = np.array([[-1, 1, 1], [-1, 2, 1], [0, 0, 0], [-1, 1, 1.0]])
        directions = np.array([[1, 0, 0], [1, 0, 0], [0, 0, 0], [1, 0, 0.0]])
        result = ray_candidates(box, origins, directions, maximum=[1.0, 1.0, 1.0, 0.5])
        np.testing.assert_array_equal(result, [True, False, True, False])

    def test_subnormal_and_invalid_ray_state_is_not_an_empty_geometry_claim(self):
        tiny = np.nextafter(0.0, 1.0)
        box = np.array([[tiny, -1, tiny], [tiny, 1, tiny]])
        self.assertTrue(ray_candidates(box, [[0.0, 0, 0]], [[0.7, -0.1, 1]])[0])
        self.assertTrue(ray_candidates(box, [[np.nan, 0, 0]], [[1.0, 0, 0]])[0])
        self.assertTrue(
            ray_candidates(box, [[100.0, 0, 0]], [[1.0, 0, 0]], maximum=np.nan)[0]
        )

    def test_shared_block_failure_precedes_even_a_proven_empty_guard(self):
        events = []
        error = RuntimeError("block-B sources 7/9: corrupt shape payload")

        def require():
            events.append("require")
            raise error

        def prepare():
            self.fail("must not construct a rejection before original preparation")

        with self.assertRaises(RuntimeError) as caught:
            trace_after_require(
                require, prepare, lambda: self.fail("failed block must not trace")
            )
        self.assertIs(caught.exception, error)
        self.assertEqual(events, ["require"])

    def test_unknown_guard_preserves_original_hit_identity_and_errors(self):
        events = []
        hit = {"source": 9, "part": 2, "face": 7, "along": 12.0}

        def prepare():
            events.append("guard")
            return None, [0, 0, 0], [1, 0, 0], 0.0, np.inf

        def original():
            events.append("trace")
            return hit

        actual = trace_after_require(
            lambda: events.append("require"), prepare, original
        )
        self.assertIs(actual, hit)
        self.assertEqual(events, ["require", "guard", "trace"])
        error = RuntimeError("source 9: original trace failure")

        def failure():
            raise error

        with self.assertRaises(RuntimeError) as caught:
            trace_after_require(lambda: None, prepare, failure)
        self.assertIs(caught.exception, error)

    def test_ready_source_only_skips_the_expensive_trace_for_an_empty_ray(self):
        events = []
        box = np.array([[0.0, 0, 0], [1.0, 1, 1]])

        def prepare():
            events.append("guard")
            return box, [3.0, 3, 3], [1.0, 0, 0], 0.0, np.inf

        result = trace_after_require(
            lambda: events.append("require"),
            prepare,
            lambda: self.fail(
                "empty local ray must skip only the original surface query"
            ),
        )
        self.assertIsNone(result)
        self.assertEqual(events, ["require", "guard"])


if __name__ == "__main__":
    unittest.main()
