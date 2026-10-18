from pathlib import Path
import unittest

import numpy as np
from shapely.geometry import Polygon

from river_surface import source_body, surface_mesh


class RiverSurface(unittest.TestCase):
    def test_concave_surface_and_island_keep_their_exact_coverage(self):
        body = Polygon([(0, 0), (20, 0), (20, 10), (10, 10), (10, 20), (0, 20)],
                       [[(2, 2), (4, 2), (4, 4), (2, 4)]])
        mesh, report = surface_mesh(body, spacing_m=2)
        self.assertEqual(report["holes"], 1)
        self.assertEqual(report["missing_or_excess_area_m2"], 0)
        self.assertLess(abs(report["overlap_area_m2"]), 1e-10)
        self.assertGreater(report["shared_edges"], 0)
        points = mesh.vertices[:, :2]
        triangles = points[mesh.triangles]
        u, v = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        self.assertTrue(np.all(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0] > 0))
        self.assertEqual(len(points), len(np.unique(points, axis=0)))

    def test_refinement_is_rejected_before_an_unbounded_mesh_is_built(self):
        body = Polygon([(0, 0), (100, 0), (100, 100), (0, 100)])
        with self.assertRaisesRegex(ValueError, "boundary refinement"):
            surface_mesh(body, spacing_m=.001, vertex_limit=100)

    def test_real_source_rings_retain_the_ill_island(self):
        with np.load(Path(__file__).parent / "fixtures/feldkirch-ill-profile.npz") as data:
            body = source_body(data)
            mesh, report = surface_mesh(body)
        self.assertEqual(report["components"], 1)
        self.assertEqual(report["holes"], 1)
        self.assertEqual(report["missing_or_excess_area_m2"], 0)
        self.assertLess(abs(report["overlap_area_m2"]), 1e-6)
        self.assertLessEqual(len(mesh.vertices), report["planned_vertices_most"])
        self.assertLessEqual(len(mesh.triangles), report["planned_faces_most"])


if __name__ == "__main__":
    unittest.main()
