"""Source-cut coverage rejects gaps and transverse contacts without polygon buffering."""

import unittest

import numpy as np
import shapely
from native_planar_arrangement import segment_covered


class CutCoverage(unittest.TestCase):
    def test_split_segment_and_rigid_translation(self):
        edges = np.array([[[0, 0], [0.25, 0]], [[0.25, 0], [1, 0]]])
        for shift in [[0, 0], [100000, -120000]]:
            moved = edges + shift
            tree = shapely.STRtree(shapely.linestrings(moved))
            self.assertTrue(segment_covered(moved[0, 0], moved[1, 1], moved, tree))

    def test_real_gap_and_crossing_do_not_count_as_edges(self):
        for edges in [
            [[[0, 0], [0.49, 0]], [[0.51, 0], [1, 0]]],
            [[[0, 0], [0, 1]], [[1, 0], [1, 1]], [[0.5, -1], [0.5, 1]]],
        ]:
            edges = np.asarray(edges, dtype=float)
            tree = shapely.STRtree(shapely.linestrings(edges))
            self.assertFalse(segment_covered([0, 0], [1, 0], edges, tree))


if __name__ == "__main__":
    unittest.main()
