"""Exact source partitions and skipped source/mesh work, without geometry oracles."""

import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from building_mass_inputs import MassSource
from building_source_tree import build_tree, select_cut, source_bounds, sources_at
from building_tree_products import band_mesh, build_cut, prepare_sources
from parent_surface_inputs import ParentInputs, open_parent


def tree_and_frame():
    tree = build_tree([1, 2], [[[0, 0, 0], [1, 1, 5]], [[1.1, 0, 0], [2.1, 1, 5]]])
    frame = {
        "basis": np.eye(3),
        "anchor": np.zeros(3),
        "selected_sources": np.array([1, 2], np.uint32),
    }
    masses = {
        i: MassSource(i, shapely.box(x, 0, x + 1, 1), (0, 0), (5, 5))
        for i, x in ((1, 0), (2, 1.1))
    }
    return tree, frame, masses


class BuildingSourceTree(unittest.TestCase):
    def test_summary_bounds_already_share_the_parent_coordinate_frame(self):
        plans = {
            "records": [{"cell": 1}],
            "summaries": [
                {
                    "supported": True,
                    "frame": np.array(
                        [[1000, 2000, 3000], [1, 0, 0], [0, 1, 0], [0, 0, 1.0]]
                    ),
                    "bounds": np.array([[999, 1999, 2999], [1001, 2001, 3001.0]]),
                }
            ],
        }
        ids, bounds, frame = source_bounds(plans, {1})
        np.testing.assert_array_equal(ids, [0])
        np.testing.assert_array_equal(bounds, [[[-1, -1, -1], [1, 1, 1]]])
        np.testing.assert_array_equal(frame["anchor"], [1000, 2000, 3000])

    def test_source_partition_bounds_and_input_permutation(self):
        ids = np.array([7, 3, 19, 11])
        b = np.array([[[x, 0, 0], [x + 2, 1, 5]] for x in (0, 5, 10, 15)])
        a = build_tree(ids, b)
        other = build_tree(ids[::-1], b[::-1])
        for key in a:
            np.testing.assert_array_equal(a[key], other[key])
        np.testing.assert_array_equal(np.sort(a["source_order"]), np.sort(ids))
        for node, (left, right) in enumerate(a["node_children"]):
            if left < 0:
                self.assertEqual(len(sources_at(a, node)), 1)
            else:
                self.assertEqual(
                    set(sources_at(a, node)),
                    set(sources_at(a, left)) | set(sources_at(a, right)),
                )
                for child in (left, right):
                    self.assertTrue(
                        np.all(a["node_bounds"][node, 0] <= a["node_bounds"][child, 0])
                    )
                    self.assertTrue(
                        np.all(a["node_bounds"][node, 1] >= a["node_bounds"][child, 1])
                    )

    def test_near_and_far_cuts_cover_every_source_without_repetition(self):
        tree, _, _ = tree_and_frame()
        for eye in ([1, 0, 2], [1, -10000, 2]):
            cut, _ = select_cut(tree, eye, 900)
            covered = np.concatenate([sources_at(tree, n) for n in cut])
            np.testing.assert_array_equal(np.sort(covered), [1, 2])
        near, _ = select_cut(tree, [1, 0, 2], 900)
        far, _ = select_cut(tree, [1, -10000, 2], 900)
        self.assertEqual(len(near), 2)
        self.assertEqual(len(far), 1)

    def test_empty_form_request_does_not_open_a_source_block(self):
        parent = SimpleNamespace(iter_forms=lambda _: self.fail("unexpected form read"))
        _, _, work = prepare_sources(parent, {33}, set(), {})
        self.assertEqual(work["form_reads"], 0)

    def test_unrequested_sources_receive_no_contour_work(self):
        _tree, frame, masses = tree_and_frame()
        parent = SimpleNamespace(
            iter_forms=lambda _: [(33, [1, 2], [object(), None])],
            form_reads=1,
            form_bytes=20,
        )
        with patch(
            "building_tree_products.describe", return_value=(masses[1], 0)
        ) as called:
            got, _, work = prepare_sources(parent, {33}, {1}, frame)
        self.assertEqual(list(got), [1])
        self.assertEqual(called.call_count, 1)
        self.assertEqual(called.call_args.args[0], 1)
        self.assertEqual(work["sources_without_contour_work"], 1)

    def test_near_originals_and_reused_parent_products(self):
        tree, frame, masses = tree_and_frame()
        leaves, _ = select_cut(tree, [1, 0, 2], 900)
        product, _ = build_cut(tree, leaves, {}, frame)
        self.assertEqual(len(product["indices"]), 0)
        np.testing.assert_array_equal(product["needs_sources"], [1, 2])
        ready = {}
        first, work = build_cut(tree, [0], masses, frame, ready_nodes=ready)
        self.assertEqual(work["new_node_products"], 1)
        self.assertGreater(len(first["indices"]), 0)
        self.assertEqual(len(first["needs_sources"]), 0)
        with patch(
            "building_tree_products.build_node",
            side_effect=AssertionError("node rebuilt"),
        ):
            second, again = build_cut(tree, [0], masses, frame, ready_nodes=ready)
        self.assertEqual(again["new_node_products"], 0)
        self.assertEqual(again["reused_node_products"], 1)
        for key in first:
            np.testing.assert_array_equal(first[key], second[key])

    def test_source_courtyard_and_occupied_island_remain_separate(self):
        empty = shapely.box(1, 1, 3, 3)
        island = shapely.box(1.75, 1.75, 2.25, 2.25)
        ring = shapely.difference(shapely.box(0, 0, 4, 4), empty)
        group = [
            MassSource(1, ring, (0, 0), (5, 5)),
            MassSource(2, island, (0, 0), (5, 5)),
        ]
        mesh, _, work = band_mesh(group, 1)
        self.assertGreater(len(mesh["indices"]), 0)
        self.assertAlmostEqual(work["protected_hole_area_m2"], 3.75)

    def test_bad_bounds_nonfinite_detail_and_overlapping_cut_are_rejected(self):
        with self.assertRaises(ValueError):
            build_tree([1, 1], np.zeros((2, 2, 3)))
        tree, frame, masses = tree_and_frame()
        for scale in (0, float("nan"), float("inf")):
            with self.assertRaises(ValueError):
                select_cut(tree, [0, 0, 0], scale)
        with self.assertRaises(ValueError):
            build_cut(tree, [0, 1], masses, frame)

    def test_actual_index_build_opens_no_form_or_native_view_package(self):
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        original = ParentInputs.member
        reads = []

        def member(parent, name):
            self.assertEqual(name, parent.metadata["index"])
            reads.append(name)
            return original(parent, name)

        with patch.object(ParentInputs, "member", member), open_parent(path) as parent:
            ids, bounds, frame = source_bounds(parent.plans, set(parent.forms))
            tree = build_tree(ids, bounds)
            self.assertEqual(parent.form_reads, 0)
            np.testing.assert_array_equal(
                np.sort(np.r_[tree["source_order"], frame["unbounded_sources"]]),
                frame["selected_sources"],
            )
        self.assertEqual(len(reads), 1)


if __name__ == "__main__":
    unittest.main()
