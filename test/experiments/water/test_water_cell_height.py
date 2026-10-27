"""Analytic shared-owner contacts and refusal without partial publication."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from test_water_cell_edges import example
from water_cell_edges import align_edges, edge_knots
from water_cell_height import project, publish_contacts


def inputs():
    products = example()
    knots = edge_knots(products)
    products, _ = align_edges(products, knots)
    records = [
        (t, p, c.parents[t + (p.feature, p.part)], c.snapshot.source)
        for t, c in products.items()
        for p in c.snapshot.pieces
    ]
    anchor = np.array([9.6, 47.2])
    coordinates = np.concatenate(
        [shapely.get_coordinates(r[1].polygon) for r in records]
    )
    xy = project(coordinates, records[0][0][0], anchor)[:, :2]
    lo, hi = xy.min(0) - 10, xy.max(0) + 10
    rectangle = np.array(
        [[lo[0], lo[1]], [hi[0], lo[1]], [hi[0], hi[1]], [lo[0], hi[1]]]
    )
    mesh = Mesh(np.c_[rectangle, plane(rectangle)], [[0, 1, 2], [0, 2, 3]])
    return mesh, anchor, records, knots


def plane(xy):
    return 100 + xy[:, 0] * 0.001 - xy[:, 1] * 0.002


class WaterCellHeight(unittest.TestCase):
    def test_analytic_planes_context_and_reversed_consumer_arrival(self):
        mesh, anchor, records, knots = inputs()
        original = mesh.vertices.tobytes()
        with tempfile.TemporaryDirectory(prefix="outshine-water-cell-height-") as tmp:
            path = Path(tmp) / "contacts.npz"
            result = publish_contacts(mesh, anchor, "model-r1", records, knots, path)
            self.assertEqual(result["state"], "ready_vertex_contacts")
            self.assertEqual(result["shared_edge_vertices"], 3)
            self.assertEqual(result["independent_consumer_gap_m"], 0)
            with np.load(path) as saved:
                points = saved["points"]
                np.testing.assert_allclose(
                    points[:, 2], plane(points[:, :2]), atol=1e-10, rtol=0
                )
                self.assertEqual(saved["owner_version"].item(), "model-r1")
                self.assertEqual(saved["owner_context"].item(), result["owner_context"])
            second = publish_contacts(
                mesh, anchor, "model-r2", records, knots, Path(tmp) / "changed.npz"
            )
            self.assertNotEqual(result["owner_context"], second["owner_context"])
        self.assertEqual(mesh.vertices.tobytes(), original)

    def test_outside_points_are_unknown_and_no_partial_file_is_published(self):
        mesh, anchor, records, knots = inputs()
        elsewhere = Mesh(mesh.vertices + [100000, 100000, 0], mesh.triangles)
        with tempfile.TemporaryDirectory(prefix="outshine-water-cell-height-") as tmp:
            path = Path(tmp) / "absent.npz"
            result = publish_contacts(
                elsewhere, anchor, "wrong-region", records, knots, path
            )
            self.assertEqual(result["state"], "unresolved_contacts")
            self.assertFalse(path.exists())

    def test_caps_and_missing_edge_descriptors_refuse(self):
        mesh, anchor, records, knots = inputs()
        with tempfile.TemporaryDirectory(prefix="outshine-water-cell-height-") as tmp:
            path = Path(tmp) / "absent.npz"
            for cap in (0, 1, 4097):
                with self.assertRaisesRegex(ValueError, "query budget"):
                    publish_contacts(
                        mesh, anchor, "model", records, knots, path, query_limit=cap
                    )
            with self.assertRaisesRegex(ValueError, "missing from published"):
                publish_contacts(mesh, anchor, "model", records, {}, path)
            self.assertFalse(path.exists())

    def test_grid_height_outside_precise_mask_is_never_published_as_water(self):
        mesh, anchor, records, knots = inputs()
        with tempfile.TemporaryDirectory(prefix="outshine-water-cell-height-") as tmp:
            path = Path(tmp) / "absent.npz"
            result = publish_contacts(
                mesh,
                anchor,
                "masked",
                records,
                knots,
                path,
                mask=shapely.box(1e6, 1e6, 1e6 + 10, 1e6 + 10),
            )
            self.assertEqual(result["state"], "unresolved_contacts")
            self.assertFalse(path.exists())


if __name__ == "__main__":
    unittest.main()
