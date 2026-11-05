import unittest

from source_product_admission import admit


def child(first, count, kind="fine", **extra):
    return {"source_ranges": [[first, count]], "kind": kind, **extra}


class AdmissionTests(unittest.TestCase):
    def test_unknown_blocks_only_its_replacement_component(self):
        result = admit([[[0, 4]], [[4, 2]]],
                       [child(0, 3), child(3, 1, "unknown"), child(4, 2)])
        self.assertEqual(result, {"generate": [2], "keep_parents": [0]})

    def test_cross_parent_recipe_requires_all_connected_coverage(self):
        parents = [[[0, 2]], [[2, 2]], [[4, 2]]]
        good = [child(0, 1), child(1, 2), child(3, 1), child(4, 2)]
        self.assertEqual(admit(parents, good), {"generate": [0, 1, 2, 3], "keep_parents": []})
        self.assertEqual(admit(parents, good[:2] + good[3:]),
                         {"generate": [2], "keep_parents": [0, 1]})

    def test_unavailable_child_preserves_all_connected_originals(self):
        result = admit([[[0, 2]], [[2, 2]]],
                       [child(0, 1), child(1, 2, available=False), child(3, 1)])
        self.assertEqual(result, {"generate": [], "keep_parents": [0, 1]})

    def test_foreign_and_overlapping_coverage_fails(self):
        for parents, children in [([[[0, 2]], [[1, 2]]], []),
                                  ([[[0, 2]]], [child(0, 3)]),
                                  ([[[0, 2]]], [child(0, 2), child(1, 1)])]:
            with self.assertRaises(ValueError):
                admit(parents, children)

    def test_sparse_parent_coverage_and_missing_child_preserve_original(self):
        parents = [[[0, 1], [8, 2]], [[4, 2]]]
        children = [child(0, 1), child(8, 1), child(4, 2)]
        self.assertEqual(admit(parents, children),
                         {"generate": [2], "keep_parents": [0]})
        children.append(child(9, 1))
        self.assertEqual(admit(parents, children),
                         {"generate": [0, 1, 2, 3], "keep_parents": []})

    def test_transitive_component_blocks_even_without_direct_unknown_overlap(self):
        parents = [[[0, 2]], [[2, 2]], [[4, 2]], [[6, 1]]]
        children = [child(0, 1, "unknown"), child(1, 2), child(3, 2), child(5, 1), child(6, 1)]
        self.assertEqual(admit(parents, children),
                         {"generate": [4], "keep_parents": [0, 1, 2]})


if __name__ == "__main__":
    unittest.main()
