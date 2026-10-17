"""Coverage preconditions, topology and legacy tolerance for composition alternatives."""

import unittest

import numpy as np
import shapely
from stair_composition_boundary import equal_coverage, oriented_equal


def square(scale=(2, 2), diagonal=0):
    xy = np.array([[0, 0], [scale[0], 0], scale, [0, scale[1]]])
    faces = [[0, 1, 2], [0, 2, 3]] if diagonal == 0 else [[0, 1, 3], [1, 2, 3]]
    return shapely.polygons(xy[np.array(faces)])


class CompositionMethods(unittest.TestCase):
    def compare(self, before, after, expected, *, proof=None):
        self.assertTrue(shapely.coverage_is_valid(before))
        self.assertTrue(shapely.coverage_is_valid(after))
        for method in ("general", "coverage", "oriented"):
            self.assertEqual(equal_coverage(before, after, method), expected, method)
        if proof is not None:
            self.assertEqual(oriented_equal(before, after), proof)

    def test_different_interior_diagonal_has_equal_complete_boundary(self):
        self.compare(square(), square(diagonal=1), True, proof=True)

    def test_holes_and_point_touching_components_keep_their_domains(self):
        xy = np.array([[0, 0], [4, 0], [4, 4], [0, 4], [1, 1], [3, 1], [3, 3], [1, 3]])
        faces = [
            [0, 1, 5],
            [0, 5, 4],
            [1, 2, 6],
            [1, 6, 5],
            [2, 3, 7],
            [2, 7, 6],
            [3, 0, 4],
            [3, 4, 7],
        ]
        ring = shapely.polygons(xy[np.array(faces)])
        self.compare(ring, ring[::-1], True, proof=True)
        self.compare(ring, square((4, 4)), False, proof=False)
        tip = shapely.Polygon([[2, 2], [3, 2], [3, 3]])
        self.compare(
            np.r_[square(), tip], np.r_[square(diagonal=1), tip], True, proof=True
        )

    def test_separate_components_and_clockwise_input(self):
        after = shapely.transform(square(), lambda xy: xy + [4, 0])
        all_parts = np.r_[square(), after]
        backwards = shapely.polygons(
            shapely.get_coordinates(all_parts).reshape(-1, 4, 2)[:, ::-1]
        )
        self.compare(all_parts, backwards, True, proof=True)

    def test_small_valid_faces_and_short_boundaries(self):
        for size in ((3, 2**-40), (2**-30, 2**-60)):
            self.compare(square(size), square(size, 1), True, proof=True)

    def test_subdivided_outer_segment_declines_the_exact_edge_proof(self):
        xy = np.array([[0, 0], [1, 0], [2, 0], [2, 2], [0, 2]])
        after = shapely.polygons(xy[np.array([[0, 1, 4], [1, 3, 4], [1, 2, 3]])])
        self.compare(square(), after, True, proof=False)

    def test_unavailable_proof_preserves_the_existing_area_tolerance(self):
        for shift, expected in ((1e-9, True), (1e-4, False)):
            after = shapely.transform(
                square(), lambda xy, offset=shift: xy + [offset, 0]
            )
            self.compare(square(), after, expected, proof=False)

    def test_overlapping_input_is_outside_coverage_union_preconditions(self):
        overlapping = np.r_[square(), square()[:1]]
        self.assertFalse(shapely.coverage_is_valid(overlapping))
        self.assertFalse(oriented_equal(overlapping, square()))


if __name__ == "__main__":
    unittest.main()
