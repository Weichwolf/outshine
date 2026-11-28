import unittest

from outshine.rendering.tile_coverage_cut import (ancestors_for_hits, children, coverage_cut,
                               coverage_intervals, edge_coordinate)


class CoverageCut(unittest.TestCase):
    def test_spatial_hits_can_refine_using_outside_sibling_metadata(self):
        root = (10, 5, 7)
        family = set(children(root))
        hot = min(family)
        unrelated = (10, 40, 40)
        stored = {root, unrelated, *family}
        roots, opened = ancestors_for_hits(stored, {hot})
        self.assertEqual(roots, {root})
        self.assertEqual(opened, {root, hot})
        self.assertEqual(set(coverage_cut(roots, stored, opened.__contains__)[0]), family)
        absent = next(k for k in family if k != hot)
        self.assertEqual(coverage_cut(roots, stored - {absent}, opened.__contains__)[0], [root])

    def test_atomic_replacement_preserves_coverage_at_every_level(self):
        roots = [(4, 15, 8), (4, 0, 8), (4, 7, 1)]
        refined = [q for t in roots for q in children(t)]
        refined.extend(children(refined.pop(-2)))
        self.assertEqual(coverage_intervals(roots), coverage_intervals(refined))
        self.assertNotEqual(coverage_intervals(roots), coverage_intervals(refined[:-1]))
        with self.assertRaises(ValueError):
            coverage_intervals(refined + roots)

    def test_a_shared_boundary_is_sampled_on_the_coarser_edge(self):
        half = [edge_coordinate(4, 1, n, 4) for n in range(5)]
        other = [edge_coordinate(5, 1, n, 4) for n in range(5)]
        self.assertEqual(half, [0, .5, 1, 1.5, 2])
        self.assertEqual(other, [2, 2.5, 3, 3.5, 4])
        self.assertEqual(edge_coordinate(32766, 3, 4, 4),
                         edge_coordinate(32767, 3, 0, 4))

    def test_partial_branch_keeps_its_parent(self):
        root = (9, 270, 181)
        cells = children(root)
        finer = children(cells[-1])
        ready = {root, *cells[:-1], *finer}
        select = lambda: coverage_cut([root], ready, lambda _: True)
        self.assertEqual(set(select()[0]), {*cells[:-1], *finer})

        ready.remove(finer[-1])
        self.assertEqual(select(), ([root], []))

        ready.remove(root)
        self.assertEqual(select(), ([], [root]))


if __name__ == "__main__":
    unittest.main()
