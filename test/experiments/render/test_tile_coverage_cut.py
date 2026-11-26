import unittest

from tile_coverage_cut import children, coverage_cut, edge_coordinate


class CoverageCut(unittest.TestCase):
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
