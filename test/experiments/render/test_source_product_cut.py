"""Source-product selection/coverage counterexamples; no native image oracle."""

import math
import struct
import unittest
from dataclasses import replace

from source_product_cut import Product, SourceProducts, centred_error


def index(cells=b"\1\1\1\1"):
    nodes = []

    def build(first, count):
        at = len(nodes)
        nodes.append(None)
        left = right = 0
        if count > 1:
            half = count // 2
            left, right = build(first, half), build(first + half, count - half)
        nodes[at] = (first * 10, 0, 0, (first + count - 1) * 10 + 2, 2, 2,
                     first, count, left, right)
        return at

    build(0, len(cells))
    data = struct.pack("<QII", 0x3158444945474150, len(nodes), len(cells))
    data += b"".join(struct.pack("<6d4I", *node) for node in nodes)
    data += struct.pack("<" + "I" * len(cells), *range(len(cells)))
    return data, cells, bytes([1]) * len(cells)


class SourceProductTests(unittest.TestCase):
    def setUp(self):
        self.tree = SourceProducts(*index(), revision="fixture")
        self.near, _ = self.tree.cut((15, 1, 1), 720, 1)
        self.far, _ = self.tree.cut((15, 1, 100000), 720, 1)

    def test_near_geometry_and_far_group_preserve_every_source(self):
        self.assertEqual([p.kind for p in self.near], ["geometry"] * 4)
        self.assertEqual([p.node for p in self.far], [0])
        self.assertEqual(self.far[0].kind, "field")
        for cut in (self.near, self.far):
            self.tree.validate_cut(cut)
            members = [self.tree.order[i] for p in cut
                       for i in range(self.tree.nodes[p.node][6],
                                      sum(self.tree.nodes[p.node][6:8]))]
            self.assertEqual(sorted(members), [0, 1, 2, 3])

    def test_semantic_cells_can_keep_independent_complete_cohorts(self):
        tree = SourceProducts(*index(b"\1\1\2\2"), revision="fixture")
        split, _ = tree.cut((15, 1, 100000), 720, 1)
        shared, _ = tree.cut((15, 1, 100000), 720, 1, cell_cohorts=False)
        self.assertEqual([p.node for p in split], [1, 4])
        self.assertEqual([p.node for p in shared], [0])
        tree.validate_cut(split)

    def test_refinement_keeps_parent_until_all_children_are_ready(self):
        partial = set(self.near[:-1])
        self.assertEqual(
            self.tree.publish(self.far, self.near, partial, valid_previous=self.far), self.far)
        ready = set(self.near)
        self.assertEqual(
            self.tree.publish(self.far, self.near, ready, valid_previous=self.far), self.near)

    def test_coarsening_waits_for_the_complete_new_parent(self):
        self.assertEqual(
            self.tree.publish(self.near, self.far, set(), valid_previous=self.near), self.near)
        self.assertEqual(
            self.tree.publish(self.near, self.far, set(self.far), valid_previous=self.near), self.far)

    def test_unchanged_cut_needs_no_replacement(self):
        self.assertEqual(self.tree.publish(self.far, self.far, set(), valid_previous=self.far), self.far)

    def test_missing_duplicate_foreign_sources_and_readiness_are_rejected(self):
        for cut in (self.near[:-1], self.near + self.near[:1], [Product(-1, "field", "fixture")]):
            with self.assertRaises(ValueError):
                self.tree.validate_cut(cut)
        with self.assertRaises(ValueError):
            self.tree.publish(self.far, self.near, set(self.far), valid_previous=self.far)

    def test_zero_error_never_qualifies_an_approximate_field(self):
        exact, _ = self.tree.cut((15, 1, 100000), 720, 0)
        self.assertTrue(all(p.kind == "geometry" for p in exact))

    def test_centred_bound_matches_the_worst_capture_corner(self):
        for focal in (1, 720, 1000):
            for half in (.1, 1, 100):
                for distance in (3 * half, 10 * half, 1000 * half):
                    worst = 0
                    for u in (-half, 0, half):
                        for v in (-half, 0, half):
                            for depth in (-2 * half, 0, 2 * half):
                                error = focal * (abs(u) + abs(v)) * abs(depth)
                                error /= distance * (distance - depth)
                                worst = max(worst, error)
                    self.assertTrue(math.isclose(centred_error(focal, half, distance), worst))
        self.assertEqual(centred_error(720, 1, 2), math.inf)

    def test_invalid_metadata_never_becomes_empty_coverage(self):
        data, cells, supported = index()
        for broken in (b"", data[:-1], b"broken!!" + data[8:]):
            with self.assertRaises(ValueError):
                SourceProducts(broken, cells, supported, revision="fixture")
        with self.assertRaises(ValueError):
            SourceProducts(data, [-1, 1, 1, 1], supported, revision="fixture")
        for eye, focal, error in (((math.nan, 0, 0), 720, 1), ((0, 0, 0), 0, 1),
                                  ((0, 0, 0), 720, -1)):
            with self.assertRaises(ValueError):
                self.tree.cut(eye, focal, error)

    def test_unknown_sources_are_explicit_and_do_not_block_supported_neighbors(self):
        data, cells, _ = index()
        unknown = SourceProducts(data, cells, revision="fixture")
        cut, _ = unknown.cut((15, 1, 100000), 720, 1)
        self.assertEqual([p.kind for p in cut], ["unresolved"] * 4)
        mixed = SourceProducts(data, cells, b"\1\0\1\1", revision="fixture")
        cut, _ = mixed.cut((15, 1, 100000), 720, 1)
        mixed.validate_cut(cut)
        self.assertEqual(sum(p.kind == "unresolved" for p in cut), 1)
        self.assertEqual(sum(p.kind == "field" for p in cut), 2)

    def test_stale_revision_never_satisfies_the_new_product_cut(self):
        stale = SourceProducts(*index(), revision="stale")
        previous, _ = stale.cut((15, 1, 100000), 720, 1)
        with self.assertRaises(ValueError):
            self.tree.publish(previous, self.near, set(self.near), valid_previous=previous)
        with self.assertRaises(ValueError):
            self.tree.publish(self.far, self.far, set(previous), valid_previous=self.far)

    def test_a_different_representation_is_not_a_ready_replacement(self):
        wrong = replace(self.far[0], pixels=self.far[0].pixels * 2)
        with self.assertRaises(ValueError):
            self.tree.publish(self.near, self.far, {wrong}, valid_previous=self.near)

    def test_expired_parents_cannot_hide_missing_children(self):
        with self.assertRaises(ValueError):
            self.tree.publish(self.far, self.near, set(self.near[:-1]), valid_previous=set())
        self.assertEqual(
            self.tree.publish(self.far, self.near, set(self.near), valid_previous=set()),
            self.near)

    def test_unknown_sources_cannot_be_published_as_ready_or_current(self):
        unresolved = Product(0, "unresolved", "fixture")
        for ready, previous in (({unresolved}, set()), (set(), {unresolved})):
            with self.assertRaises(ValueError):
                self.tree.publish([unresolved], [unresolved], ready, valid_previous=previous)

    def test_cohort_tags_are_opaque_and_not_limited_to_building_cells(self):
        data, _, supported = index()
        tree = SourceProducts(data, [1000, 1000, 1000, 1000], supported, revision="other-source")
        cut, _ = tree.cut((15, 1, 100000), 720, 1)
        self.assertEqual([p.node for p in cut], [0])

    def test_zero_size_source_is_retained_without_an_invalid_capture(self):
        data, cells, supported = index(b"\1")
        data = data[:16] + struct.pack("<6d", *([0] * 6)) + data[64:]
        tree = SourceProducts(data, cells, supported, revision="point-source")
        cut, _ = tree.cut((0, 0, 100000), 720, 1)
        self.assertEqual([p.kind for p in cut], ["geometry"])
        tree.validate_cut(cut)


if __name__ == "__main__":
    unittest.main()
