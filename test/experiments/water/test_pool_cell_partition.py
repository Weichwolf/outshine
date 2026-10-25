"""Exact shared-edge nodes survive interval rejection in all edge directions."""

import unittest
from fractions import Fraction

import numpy as np
from pool_cell_partition import _area, _triangulate


class PoolCellPartition(unittest.TestCase):
    def test_exact_interval_search_keeps_edge_nodes_and_excludes_subfloat_offsets(self):
        f = Fraction
        tiny = f(1, 2**2000)
        polygon = [(f(0), f(0)), (f(1), f(0)), (f(1), f(1)), (f(0), f(1))]
        requested = {(f(1, 3), f(0)), (f(2, 3), tiny), (-tiny, f(1, 3))}
        _, faces, points, work, _, determinants, search = _triangulate(
            [polygon], None, requested
        )
        self.assertIn((f(1, 3), f(0)), points)
        self.assertNotIn((f(2, 3), f(0)), points)
        self.assertNotIn((f(0), f(1, 3)), points)
        self.assertEqual(len(faces), 5)
        self.assertEqual(determinants, 1)
        self.assertLess(search["edge_node_interval_visits"], work)
        self.assertEqual(sum(_area([points[i] for i in face]) for face in faces), 2)

    def test_interval_skips_do_not_relax_original_candidate_budget(self):
        f = Fraction
        polygon = [(f(0), f(0)), (f(1), f(0)), (f(1), f(1)), (f(0), f(1))]
        far = {(f(i + 2), f(2)) for i in range(1000)}
        with self.assertRaisesRegex(ValueError, "edge-node work budget exhausted"):
            _triangulate([polygon] * 66, None, far)

    def test_exact_third_on_shared_edge_survives_rotation_shear_and_outside_nodes(self):
        f = Fraction
        pieces = [
            [(0, 0), (1, 0), (1, 1), (0, 1)],
            [(1, 0), (2, 0), (2, 1), (1, 1), (1, f(1, 3))],
            [(0, -1), (1, -1), (1, 0), (0, 0)],
            [(0, 1), (1, 1), (1, 2), (0, 2)],
        ]
        for matrix in (
            ((1, 0), (0, 1)),
            ((0, -1), (1, 0)),
            ((1, 1), (0, 1)),
            ((-1, 0), (0, -1)),
        ):
            with self.subTest(matrix=matrix):

                def transform(p, matrix=matrix):
                    return tuple(
                        sum(f(a) * f(b) for a, b in zip(row, p)) for row in matrix
                    )

                polygons = [[transform(p) for p in poly] for poly in pieces]
                _, faces, points, _, _, _, _ = _triangulate(polygons, None)
                ids = {p: i for i, p in enumerate(points)}
                a, mid, b = [ids[transform(p)] for p in ((1, 0), (1, f(1, 3)), (1, 1))]
                edges = np.sort(
                    faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1
                )
                unique, counts = np.unique(edges, axis=0, return_counts=True)
                incidence = dict(zip(map(tuple, unique), counts))
                self.assertEqual(incidence[tuple(sorted((a, mid)))], 2)
                self.assertEqual(incidence[tuple(sorted((mid, b)))], 2)
                self.assertNotIn(tuple(sorted((a, b))), incidence)
                self.assertTrue(
                    all(_area([points[i] for i in face]) > 0 for face in faces)
                )
                self.assertEqual(
                    sum(_area([points[i] for i in face]) for face in faces),
                    sum(_area(poly) for poly in polygons),
                )


if __name__ == "__main__":
    unittest.main()
