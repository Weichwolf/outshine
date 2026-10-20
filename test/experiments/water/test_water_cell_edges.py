"""A shared Mercator edge needs shared vertices before the curved ENU mapping."""

import dataclasses
import unittest

import numpy as np
import shapely
from outshine_ref.places.sources import tile_geography
from outshine_ref.water.geodetic import local_points
from test_owned_water_cells import cell
from water_cell_edges import align_edges, edge_knots
from water_cell_handoff import route_cells

A, B = (14, 8628, 5745), (14, 8629, 5745)


def example():
    a = shapely.box(8628.2, 5745.2, 8629, 5745.8)
    b = shapely.Polygon(
        [
            (8629, 5745.2),
            (8629.8, 5745.2),
            (8629.8, 5745.8),
            (8629, 5745.8),
            (8629, 5745.5),
        ]
    )
    snapshots = {A: cell(A, a), B: cell(B, b)}
    products, _ = route_cells(
        snapshots, source_tiles=(A, B), recipients=(A, B), scope="curved-cell-edge-v1"
    )
    return products


def border(product):
    xy = np.concatenate(
        [shapely.get_coordinates(p.polygon) for p in product.snapshot.pieces]
    )
    return np.unique(xy[xy[:, 0] == 8629], axis=0)


def project(points):
    lat, lon = tile_geography(points, 1, (14, 0, 0))
    return local_points([9.6, 47.2], np.c_[lon, lat], 0)


class WaterCellEdges(unittest.TestCase):
    def test_geographic_projection_exposes_raw_mismatched_subdivision(self):
        products = example()
        left, right = border(products[A]), border(products[B])
        self.assertEqual(len(left), 2)
        self.assertEqual(len(right), 3)
        # The geographic middle lies off the straight ECEF/ENU endpoint chord.
        a, b = project(left)
        p = project(right)[1]
        t = np.dot(p - a, b - a) / np.dot(b - a, b - a)
        self.assertGreater(np.linalg.norm(p - (a + t * (b - a))), 0.01)
        fixed, work = align_edges(products, edge_knots(products))
        self.assertEqual(work["inserted_shared_border_vertices"], 1)
        np.testing.assert_array_equal(border(fixed[A]), border(fixed[B]))
        np.testing.assert_array_equal(
            project(border(fixed[A])), project(border(fixed[B]))
        )

    def test_published_descriptors_make_independent_consumer_windows_identical(self):
        products = example()
        knots = edge_knots(products)
        together, _ = align_edges(products, knots)
        for tile in (A, B):
            alone, _ = align_edges({tile: products[tile]}, knots)
            self.assertEqual(together[tile].snapshot, alone[tile].snapshot)
            for before, after in zip(
                products[tile].snapshot.pieces, alone[tile].snapshot.pieces, strict=True
            ):
                self.assertTrue(before.polygon.equals(after.polygon))
        reversed_cohort = dict(reversed(list(products.items())))
        self.assertEqual(edge_knots(reversed_cohort), knots)

    def test_stale_incomplete_and_bounded_descriptors_refuse(self):
        products = example()
        with self.assertRaisesRegex(ValueError, "knot budget"):
            edge_knots(products, knot_limit=1)
        with self.assertRaisesRegex(ValueError, "coordinate budget"):
            align_edges(products, edge_knots(products), coordinate_limit=1)
        with self.assertRaisesRegex(ValueError, "omits"):
            align_edges(products, {})
        products[A] = dataclasses.replace(
            products[A], state="pending_sources", snapshot=None
        )
        with self.assertRaisesRegex(ValueError, "complete declared"):
            edge_knots(products)


if __name__ == "__main__":
    unittest.main()
