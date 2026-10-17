"""An actual valid owner cell becomes self-intersecting under Float32 positions."""

import itertools
import json
import unittest
from fractions import Fraction
from pathlib import Path

import numpy as np
import shapely


def orientation(a, b, c):
    a, b, c = [tuple(Fraction.from_float(float(v)) for v in p) for p in (a, b, c)]
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def crossing_edges(points):
    result, n = [], len(points)
    for i, j in itertools.combinations(range(n), 2):
        if j == i + 1 or (i == 0 and j == n - 1):
            continue
        a, b, c, d = points[i], points[(i + 1) % n], points[j], points[(j + 1) % n]
        if (
            orientation(a, b, c) * orientation(a, b, d) < 0
            and orientation(c, d, a) * orientation(c, d, b) < 0
        ):
            result.append((i, j))
    return result


class ActualQuantizedCell(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = (
            Path(__file__).parent / "fixtures/stair-quantized-junction-cell-vienna.json"
        )
        cls.case = json.loads(path.read_text())
        cls.xy = np.array(cls.case["cell_xy_m"])
        cls.rounded = cls.xy.astype("f4").astype(float)

    def test_fixed_original_node_and_nonmerged_vertices_do_not_prove_valid_faces(self):
        first, second = self.case["sources"].values()
        self.assertEqual(first["mvt_xy"][0], self.case["shared_original_node"])
        self.assertEqual(second["mvt_xy"][-1], self.case["shared_original_node"])
        self.assertEqual(len(np.unique(self.rounded, axis=0)), len(self.xy))
        self.assertTrue(shapely.Polygon(self.xy).is_valid)
        self.assertFalse(shapely.Polygon(self.rounded).is_valid)

    def test_exact_rational_boundary_crossing_rules_out_triangulation_only_fix(self):
        self.assertEqual(crossing_edges(self.xy), [])
        self.assertTrue(crossing_edges(self.rounded))

    def test_actual_double_triangulation_has_an_inverted_float32_face(self):
        triangles = shapely.get_parts(
            shapely.constrained_delaunay_triangles(shapely.Polygon(self.xy))
        )
        points = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
        self.assertTrue(
            any(
                orientation(*p) * orientation(*p.astype("f4").astype(float)) < 0
                for p in points
            )
        )


if __name__ == "__main__":
    unittest.main()
