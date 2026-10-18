"""Hierarchy/whole equivalence, unknowns, ownership and finite-work fallbacks."""

import unittest

import numpy as np
from outshine_ref.geometry.occlusion import classify_boxes, convex_patch_umbra
from outshine_ref.geometry.occlusion_hierarchy import BoundsHierarchy

WALL = np.array([[-1, 0, -1], [1, 0, -1], [1, 0, 1], [-1, 0, 1]], dtype=float)
REGION = [[-0.25, -2.25, -0.25], [0.25, -1.75, 0.25]]


def boxes(count=1000):
    rng = np.random.default_rng(1212)
    centres = rng.uniform([-2, 10, -2], [2, 20, 2], (count, 3))
    return np.stack((centres - 0.05, centres + 0.05), axis=1)


def whole(bounds, umbra):
    return np.concatenate(
        [
            classify_boxes(bounds[i : i + 512], umbra)[0]
            for i in range(0, len(bounds), 512)
        ]
    )


class HierarchicalOcclusion(unittest.TestCase):
    def test_matches_whole_scan_with_less_geometric_query_work(self):
        source = boxes()
        umbra = convex_patch_umbra(WALL, REGION, certified_opaque=True)
        hierarchy = BoundsHierarchy(source, known=np.ones(len(source), dtype=bool))
        selection = hierarchy.select([(umbra, None)])
        np.testing.assert_array_equal(selection.hidden, whole(source, umbra))
        self.assertGreater(selection.hidden.sum(), 900)
        self.assertLess(selection.work["box_tests"], len(source) // 4)
        self.assertGreaterEqual(
            selection.work["source_flag_writes"], selection.hidden.sum()
        )

    def test_input_permutation_does_not_change_source_decisions(self):
        source = boxes(333)
        permutation = np.random.default_rng(12).permutation(len(source))
        umbra = convex_patch_umbra(WALL, REGION, certified_opaque=True)
        before = BoundsHierarchy(source, known=np.ones(len(source), dtype=bool)).select(
            [(umbra, None)]
        )
        after = BoundsHierarchy(
            source[permutation], known=np.ones(len(source), dtype=bool)
        ).select([(umbra, None)])
        np.testing.assert_array_equal(after.hidden, before.hidden[permutation])

    def test_unknown_sources_owning_occluder_and_protected_shadow_demands_remain(self):
        source = boxes(100)
        source[0] = [[-1, -0.01, -1], [1, 0.01, 1]]
        source[1] = np.nan
        known = np.ones(len(source), dtype=bool)
        known[1] = False
        protected = np.zeros(len(source), dtype=bool)
        protected[[3, 17, 42]] = True
        umbra = convex_patch_umbra(WALL, REGION, certified_opaque=True)
        hierarchy = BoundsHierarchy(source, known=known)
        selection = hierarchy.select([(umbra, 0)], protected=protected)
        self.assertFalse(selection.hidden[[0, 1, 3, 17, 42]].any())
        self.assertGreater(selection.hidden.sum(), 50)
        self.assertGreaterEqual(selection.work["protected_flag_reads"], known.sum())

    def test_work_exhaustion_keeps_unproved_sources(self):
        source = boxes(100)
        source[::3, :, 0] += 100
        hierarchy = BoundsHierarchy(
            source, known=np.ones(len(source), dtype=bool), leaf_size=1
        )
        umbra = convex_patch_umbra(WALL, REGION, certified_opaque=True)
        full = hierarchy.select([(umbra, None)])
        zero = hierarchy.select([(umbra, None)], max_box_tests=0)
        limited = hierarchy.select([(umbra, None)], max_box_tests=5)
        self.assertFalse(zero.hidden.any())
        self.assertLessEqual(limited.work["box_tests"], 5)
        self.assertFalse((limited.hidden & ~full.hidden).any())

    def test_mismatched_observer_regions_are_rejected_before_reuse(self):
        hierarchy = BoundsHierarchy(boxes(2), known=np.ones(2, dtype=bool))
        first = convex_patch_umbra(WALL, REGION, certified_opaque=True)
        second = convex_patch_umbra(
            WALL, [[0, -2, 0], [0, -2, 0]], certified_opaque=True
        )
        with self.assertRaises(ValueError):
            hierarchy.select([(first, None), (second, None)])
        selected = hierarchy.select([(first, None)])
        self.assertTrue(selected.contains_observer([0, -2, 0]))
        self.assertFalse(selected.contains_observer([10, -2, 0]))

    def test_unknown_opacity_and_empty_source_world_do_not_hide(self):
        empty = BoundsHierarchy(np.empty((0, 2, 3)), known=np.zeros(0, dtype=bool))
        self.assertEqual(len(empty.select([]).hidden), 0)
        unknown = convex_patch_umbra(None, None)
        hierarchy = BoundsHierarchy(boxes(3), known=np.ones(3, dtype=bool))
        selected = hierarchy.select([(unknown, None)])
        self.assertFalse(selected.hidden.any())
        self.assertTrue(selected.contains_observer([1e6, 1e6, 1e6]))

    def test_cache_neighbor_cannot_supply_occlusion_when_planned_in_isolation(self):
        source = np.concatenate(
            [np.array([[[-1, -0.01, -1], [1, 0.01, 1]]]), boxes(10)]
        )
        shared = BoundsHierarchy(source, known=np.ones(len(source), dtype=bool))
        umbra = convex_patch_umbra(WALL, REGION, certified_opaque=True)
        global_plan = shared.select([(umbra, 0)])
        isolated_neighbor = BoundsHierarchy(
            source[1:], known=np.ones(10, dtype=bool)
        ).select([])
        self.assertTrue(global_plan.hidden[1:].all())
        self.assertFalse(isolated_neighbor.hidden.any())

    def test_owned_arrays_and_invalid_input_limits(self):
        source = boxes(5)
        hierarchy = BoundsHierarchy(source, known=np.ones(5, dtype=bool))
        source[:] = 99
        self.assertFalse(np.array_equal(source, hierarchy.bounds))
        for array in (
            hierarchy.bounds,
            hierarchy.order,
            hierarchy.nodes,
            hierarchy.position,
        ):
            self.assertFalse(array.flags.writeable)
        with self.assertRaises(ValueError):
            BoundsHierarchy(np.full((1, 2, 3), np.nan), known=np.ones(1, dtype=bool))
        with self.assertRaises(ValueError):
            hierarchy.select([], max_box_tests=-1)
        with self.assertRaises(ValueError):
            hierarchy.select([], protected=np.zeros(6, dtype=bool))
        outside = convex_patch_umbra(WALL, REGION, certified_opaque=True)
        with self.assertRaises(ValueError):
            hierarchy.select([(outside, 0)])


if __name__ == "__main__":
    unittest.main()
