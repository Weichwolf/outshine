import unittest

import numpy as np
from outshine_ref.buildings.facade_cells import cell_complement
from outshine_ref.buildings.facades import FacadeRepeat


class FacadeCells(unittest.TestCase):
    def test_complement_covers_every_retained_cell_exactly_once(self):
        repeat = FacadeRepeat(24, 18, 8, 6, 0.48, 0.53)
        excluded = ((2, 0), (1, 2), (5, 2), (1, 3), (5, 3))
        for sequence in (excluded, tuple(reversed(excluded))):
            counts = np.zeros((6, 8), dtype=int)
            ranges = cell_complement(repeat, sequence)
            for x0, y0, x1, y1 in ranges:
                counts[y0:y1, x0:x1] += 1
            expected = np.ones_like(counts)
            for c, r in excluded:
                expected[r, c] = 0
            np.testing.assert_array_equal(counts, expected)
            self.assertEqual(ranges, cell_complement(repeat, excluded))

    def test_large_metadata_is_not_expanded_and_invalid_ids_fail(self):
        repeat = FacadeRepeat(24, 18, 2**31, 2**31, 0.48, 0.53)
        self.assertLessEqual(len(cell_complement(repeat, ((100, 100),))), 4)
        for excluded in (
            [(0, 0)],
            ((True, 0),),
            ((-1, 0),),
            ((0, 0), (0, 0)),
            ((2**31, 0),),
        ):
            with self.assertRaises(ValueError):
                cell_complement(repeat, excluded)
        with self.assertRaises(ValueError):
            cell_complement(repeat, ((0, 0),), max_excluded=0)
