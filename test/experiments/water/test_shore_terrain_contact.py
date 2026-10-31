import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from conforming_body_contact import grid
from shore_bank_strip import build, combine
from shore_contact_audit import audit, boundary_edges
from shore_contact_edges import attach
from shore_native_terrain import load
from shore_terrain_contact_study import canonical_faces, prior_obligations, reconcile
from shore_terrain_cut import cut

FIXTURE = Path(__file__).parents[1] / "transport/fixtures/feldkirch-shore-contact"


class WholeBankTerrainContact(unittest.TestCase):
    def inputs(self):
        xy, faces = grid(np.arange(5) / 2, np.arange(-2, 5) / 2)
        vertices = np.c_[xy, 12 + xy[:, 0] / 8 + xy[:, 1] / 4]
        shore = np.array([[-1, 0, 10], [3, 0, 10.5]])
        road = np.array([[-1, 1, 16], [0.6, 1, 16.2], [3, 1, 16.5]])
        body = build(shore, [15, 15.5], road, bed_depth_m=2)
        footprint = shapely.coverage_union_all(
            shapely.polygons(body["vertices"][body["roof_faces"], :2])
        )
        return vertices, faces, body, footprint

    def test_local_and_global_cut_preserve_affine_exterior_and_source_facets(self):
        vertices, faces, _, footprint = self.inputs()
        outcomes = [
            cut(vertices, faces, footprint, method=m) for m in ("local", "global")
        ]
        for xyz, output, _, note in outcomes:
            np.testing.assert_allclose(
                xyz[:, 2], 12 + xyz[:, 0] / 8 + xyz[:, 1] / 4, atol=1e-12, rtol=0
            )
            self.assertTrue(
                shapely.coverage_is_valid(shapely.polygons(xyz[output, :2]))
            )
            self.assertLessEqual(note["exterior_boundary_hausdorff_m"], 1e-12)
        self.assertLess(
            outcomes[0][-1]["boolean_faces"], outcomes[1][-1]["boolean_faces"]
        )
        with self.assertRaises(ValueError):
            cut(vertices, faces, footprint, max_faces=1)

    def test_complete_edges_share_words_and_both_body_incident_faces_are_split(self):
        vertices, faces, body, footprint = self.inputs()
        results = [
            reconcile(body, vertices, faces, footprint, method=m)
            for m in ("local", "global")
        ]
        self.assertEqual(
            canonical_faces(results[0][0]).tobytes(),
            canonical_faces(results[1][0]).tobytes(),
        )
        for output, note in results:
            self.assertGreater(note["interfaces"]["complete_shared_contact_edges"], 1)
            self.assertEqual(len(note["interfaces"]["outer_obligations"]), 4)
            self.assertEqual(len(boundary_edges(output["body_faces"])), 0)
            self.assertEqual(
                output["body_vertices"][: len(body["vertices"])].tobytes(),
                body["vertices"].tobytes(),
            )
            for row in output["contacts"]:
                self.assertEqual(
                    output["terrain_vertices"][row["terrain"]].tobytes(),
                    output["body_vertices"][row["body"]].tobytes(),
                )

    def test_unrefined_body_t_junction_and_unsupported_internal_bank_end_decline(self):
        vertices, faces, body, footprint = self.inputs()
        before, output_faces, _, _ = cut(vertices, faces, footprint)
        outer = shapely.union_all(
            shapely.linestrings(vertices[boundary_edges(faces), :2])
        )
        terrain, refined, _refined_faces, links, _parents, _ = attach(
            body, before, output_faces, outer
        )
        with self.assertRaisesRegex(ValueError, "T-junction"):
            audit(
                body,
                vertices,
                faces,
                before,
                terrain,
                output_faces,
                refined,
                body["faces"],
                np.arange(len(body["faces"])),
                links,
            )
        short = build(
            [[0.2, 0, 10], [1.8, 0, 10]],
            [15, 15],
            [[0.2, 1, 16], [1.8, 1, 16]],
            bed_depth_m=2,
        )
        short_foot = shapely.Polygon([[0.2, 0], [1.8, 0], [1.8, 1], [0.2, 1]])
        with self.assertRaisesRegex(ValueError, "continuation"):
            reconcile(short, vertices, faces, short_foot)

    def test_captured_four_pages_keep_all_original_height_words(self):
        manifest = json.loads((FIXTURE / "input.json").read_bytes())
        vertices, faces, heights = load(FIXTURE, [9.598371, 47.232575], manifest)
        self.assertEqual(vertices.shape, (65 * 65, 3))
        self.assertEqual(faces.shape, (64 * 64 * 2, 3))
        with np.load(FIXTURE / "deformed-pages.npz", allow_pickle=False) as source:
            for page in manifest["pages"]:
                row, column = (page["y"] - 367802) * 32, (page["x"] - 552238) * 32
                self.assertEqual(
                    heights[row : row + 33, column : column + 33].tobytes(),
                    source[page["array"]][1:34, 1:34].tobytes(),
                )

    def test_separate_banks_share_an_owner_without_connecting_their_gap(self):
        xy, faces = grid(np.arange(9) / 2, np.arange(-4, 7) / 2)
        vertices = np.c_[xy, np.full(len(xy), 12.0)]
        north = build(
            [[-1, 1, 10], [5, 1, 10.5]],
            [15, 15.5],
            [[-1, 2, 16], [5, 2, 16.5]],
            bed_depth_m=2,
        )
        south = build(
            [[-1, 0, 10], [5, 0, 10.5]],
            [15, 15.5],
            [[-1, -1, 16], [5, -1, 16.5]],
            bed_depth_m=2,
        )
        body = combine([north, south])
        edges = body["contact_edges"]
        self.assertTrue(
            np.all(
                (edges[:, 0] < len(north["vertices"]))
                == (edges[:, 1] < len(north["vertices"]))
            )
        )
        footprint = shapely.coverage_union_all(
            shapely.polygons(body["vertices"][body["roof_faces"], :2])
        )
        output, _ = reconcile(body, vertices, faces, footprint)
        self.assertEqual(set(output["body_source_components"]), {0, 1})
        self.assertEqual(len(boundary_edges(output["body_faces"])), 0)
        with self.assertRaises(ValueError):
            combine([north, north])

    def test_prior_edge_requires_identical_words_and_both_actual_neighbors(self):
        vertices, faces, body, footprint = self.inputs()
        products, _ = reconcile(body, vertices, faces, footprint)
        ids = np.flatnonzero(
            np.all(products["terrain_vertices"][:, :2] == [1, 0], axis=1)
        )
        self.assertEqual(len(ids), 1)
        point = products["terrain_vertices"][ids[0]].copy()
        prior = {
            "commit": "prior",
            "geometry": {
                "interfaces": {"outer_obligations": [{"required_xyz": point.tolist()}]}
            },
        }
        adaptive = {
            "vertices": vertices,
            "faces": faces,
            "source_pages": np.where(vertices[faces, 0].min(axis=1) < 1, 1, 2),
        }
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.outshine_research."
        ) as tmp:
            root = Path(tmp)
            (root / "report.json").write_text(json.dumps(prior))
            result = prior_obligations(root, products, adaptive)
            self.assertEqual(result["word_equal_and_coupled"], 1)
            self.assertEqual(result["nodes"][0]["source_page_ids"], [1, 2])
            prior["geometry"]["interfaces"]["outer_obligations"][0]["required_xyz"][
                2
            ] += 0.01
            (root / "report.json").write_text(json.dumps(prior))
            with self.assertRaisesRegex(ValueError, "word-identical"):
                prior_obligations(root, products, adaptive)


if __name__ == "__main__":
    unittest.main()
