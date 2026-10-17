"""Independent GEOS decisions and exact-orientation adversaries for the witness."""

import unittest
from fractions import Fraction

import numpy as np
import shapely
from scipy.spatial import Delaunay
from stair_coverage_boundary import GENERAL_COVERAGE, checked_coverage, polygon_witness


def triangles(xy, faces):
    return shapely.polygons(np.asarray(xy, float)[np.asarray(faces, int)])


class BoundaryCoverage(unittest.TestCase):
    def compare(self, polygons, *, proved=None):
        proof = polygon_witness(polygons)
        expected = bool(GENERAL_COVERAGE(polygons))
        self.assertEqual(checked_coverage(polygons), expected)
        if proof["proved"]:
            self.assertTrue(expected)
        if proved is not None:
            self.assertEqual(proof["proved"], proved, proof)

    def test_convex_and_concave_single_boundary(self):
        for xy in (
            [(0, 0), (3, 0), (3, 3), (0, 3)],
            [(0, 0), (3, 0), (1, 1), (0, 3)],
        ):
            self.compare(triangles(xy, [[0, 1, 2], [0, 2, 3]]), proved=True)

    def test_duplicate_xy_vertices_are_canonicalised(self):
        self.compare(
            triangles(
                [(0, 0), (2, 0), (2, 2), (0, 0), (2, 2), (0, 2)],
                [[0, 1, 2], [3, 4, 5]],
            ),
            proved=True,
        )

    def test_positive_orientation_alone_does_not_admit_overlap(self):
        cases = (
            [[(0, 0), (3, 0), (1, 3)], [(0, 1), (3, 1), (1, 4)]],
            [[(0, 0), (4, 0), (0, 4)], [(0.5, 0.5), (1, 0.5), (0.5, 1)]],
            [[(0, 0), (2, 0), (0, 2)], [(0, 0), (2, 0), (0, 2)]],
        )
        for xy in cases:
            polygons = shapely.polygons(xy)
            self.assertTrue(shapely.is_ccw(shapely.get_exterior_ring(polygons)).all())
            self.assertFalse(GENERAL_COVERAGE(polygons))
            self.compare(polygons, proved=False)

    def test_touching_components_and_holes_use_general_validator(self):
        for xy in (
            [[(0, 0), (1, 0), (0, 1)], [(2, 0), (3, 0), (2, 1)]],
            [[(0, 0), (1, 0), (0, 1)], [(1, 0), (2, 0), (2, 1)]],
        ):
            self.compare(shapely.polygons(xy), proved=False)
        xy = [[0, 0], [4, 0], [4, 4], [0, 4], [1, 1], [3, 1], [3, 3], [1, 3]]
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
        self.compare(triangles(xy, faces), proved=False)

    def test_t_junction_and_degenerate_edges_are_not_proofs(self):
        for xy in (
            [[(0, 0), (2, 0), (0, 2)], [(0, 0), (0, -1), (1, 0)]],
            [[(0, 0), (1, 0), (0, 0)]],
            [[(0, 0), (1, 0), (2, 0)]],
        ):
            self.compare(shapely.polygons(xy), proved=False)

    def test_reverse_orientation_never_becomes_a_fast_proof(self):
        self.compare(triangles([(0, 0), (0, 1), (1, 0)], [[0, 1, 2]]), proved=False)

    def test_twice_wrapped_positive_fan_has_a_noninjective_boundary(self):
        angles = np.deg2rad(np.arange(8) * 100)
        xy = np.vstack(([0, 0], np.c_[np.cos(angles), np.sin(angles)]))
        faces = [[0, i + 1, (i + 1) % 8 + 1] for i in range(8)]
        polygons = triangles(xy, faces)
        self.assertTrue(shapely.is_ccw(shapely.get_exterior_ring(polygons)).all())
        self.assertFalse(GENERAL_COVERAGE(polygons))
        self.compare(polygons, proved=False)

    def test_fixed_seed_meshes_and_corrupted_faces(self):
        random = np.random.default_rng(678)
        for _ in range(32):
            xy = random.uniform(-100, 100, (32, 2))
            faces = Delaunay(xy).simplices
            self.compare(triangles(xy, faces), proved=True)
            self.compare(triangles(xy, np.vstack([faces, faces[:1]])), proved=False)
            moved = xy.copy()
            moved[faces[0, 0]] = moved[faces[1, 0]]
            self.compare(triangles(moved, faces))

    def test_exact_orientation_of_ill_conditioned_triangles(self):
        random = np.random.default_rng(679)
        for _ in range(256):
            a = random.uniform(-1e8, 1e8, 2)
            b = random.uniform(-1e8, 1e8, 2)
            c = a + (b - a) * random.random()
            axis = random.integers(2)
            c[axis] = np.nextafter(c[axis], np.inf)
            xy = [list(map(Fraction, p)) for p in (a, b, c)]
            cross = (xy[1][0] - xy[0][0]) * (xy[2][1] - xy[0][1]) - (
                xy[1][1] - xy[0][1]
            ) * (xy[2][0] - xy[0][0])
            polygons = shapely.polygons([[a, b, c]])
            proof = polygon_witness(polygons)
            if proof["proved"]:
                self.assertGreater(cross, 0)
            self.compare(polygons)


if __name__ == "__main__":
    unittest.main()
