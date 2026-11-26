import unittest

from tile_coverage_cut import children, coverage_cut


class CoverageCut(unittest.TestCase):
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
