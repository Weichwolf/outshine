from pathlib import Path
import hashlib
import json
import unittest

import numpy as np
from shapely.geometry import Polygon

from river_surface import source_body, surface_mesh


class RiverSurface(unittest.TestCase):
    def test_real_fixture_keeps_source_classes_and_exact_identity(self):
        path = Path(__file__).parent / "fixtures/feldkirch-ill-profile.npz"
        manifest = json.loads(path.with_suffix(".json").read_text())
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), manifest["input_sha256"])
        observation = manifest["source_semantics_observation"]
        with np.load(path) as data:
            self.assertEqual([r["fixture_index"] for r in observation["surfaces"]],
                             data["surface_source_indices"].tolist())
        self.assertEqual([r["fixture_index"] for r in observation["courses"]],
                         manifest["selection"]["selected_course_indices"])
        self.assertEqual({r["kind"] for r in observation["surfaces"]}, {"river"})
        for family, layer in (("surfaces", "water_polygons"), ("courses", "water_lines")):
            for row in observation[family]:
                source = observation["sources"][str(row["source"])]
                self.assertGreaterEqual(row["feature"], 0)
                self.assertLess(row["feature"], source["features"])
                self.assertEqual(row["layer"], layer)
                self.assertEqual(source["provider"], "openfreemap.openmaptiles")
                self.assertTrue(source["revision"])
                if row["provider_feature_id"] is not None:
                    self.assertIsInstance(row["provider_feature_id"], str)
                    identity = int(row["provider_feature_id"])
                    self.assertGreaterEqual(identity, 0)
                    self.assertLess(identity, 1 << 64)
                self.assertIsInstance(row["bridge"], bool)
                self.assertIsInstance(row["tunnel"], bool)
                self.assertIsInstance(row["intermittent"], (bool, type(None)))

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
