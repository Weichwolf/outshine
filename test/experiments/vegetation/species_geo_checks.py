"""Independent holes, overlapping memberships and geographic index boundaries."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from species_geo_index import CellRegions, DirectRegions, points, read_regions


class SpeciesGeographyTests(unittest.TestCase):
    def test_wrapped_source_sliver_is_retained_on_both_meridian_sides(self):
        geometry = shapely.box(-180.1, 60, -179.9, 61)
        source = json.dumps(
            {
                "type": "FeatureCollection",
                "features": [
                    {
                        "type": "Feature",
                        "properties": {"LEVEL3_COD": "OWN"},
                        "geometry": json.loads(shapely.to_geojson(geometry)),
                    }
                ],
            }
        )
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.species-wrap."
        ) as directory:
            path = Path(directory) / "regions.json"
            path.write_text(source)
            with self.assertRaisesRegex(ValueError, "periodic"):
                read_regions(path)
            codes, canonical, records = read_regions(path, wrap_longitude=True)
            self.assertEqual(path.read_text(), source)
        self.assertEqual(codes, ("OWN",))
        self.assertAlmostEqual(canonical[0].area, geometry.area, places=11)
        self.assertTrue(canonical[0].covers(shapely.Point(179.95, 60.5)))
        self.assertTrue(canonical[0].covers(shapely.Point(-179.95, 60.5)))
        self.assertFalse(canonical[0].covers(shapely.Point(0, 60.5)))
        self.assertEqual(records[0]["code"], "OWN")

    def test_source_coordinate_precision_survives_internal_cell_edges(self):
        longitude = np.array(
            (np.nextafter(180.0, 0), np.nextafter(-180.0, 0), -1e-15, 180)
        )
        actual = points(np.zeros(4), longitude)
        np.testing.assert_array_equal(actual[:3, 0], longitude[:3])
        self.assertEqual(actual[3, 0], -180)
        region = DirectRegions([shapely.box(-1, -1, -1e-15, 1)])
        index = CellRegions(region, level=2)
        np.testing.assert_array_equal(index.query(0, -1e-15), ((0,), (0,)))

    def test_holes_overlap_dateline_poles_and_exact_shared_edges(self):
        regions = np.array(
            (
                shapely.box(-180, -90, 0, 90),
                shapely.Polygon(
                    ((10, 10), (50, 10), (50, 50), (10, 50)),
                    holes=[((20, 20), (30, 20), (30, 30), (20, 30))],
                ),
                shapely.box(40, 10, 180, 90),
            ),
            dtype=object,
        )
        direct = DirectRegions(regions)
        index = CellRegions(direct, level=3)
        lat, lon = (
            (0, 0, 90, -90, 25, 25, 25, 10, 90),
            (-90, 180, -120, -30, 25, 20, 45, 40, 180),
        )
        expected = np.array(
            ((0, 1, 2, 3, 5, 6, 6, 7, 7, 8), (0, 0, 0, 0, 1, 1, 2, 1, 2, 0))
        )
        np.testing.assert_array_equal(direct.query(lat, lon), expected)
        np.testing.assert_array_equal(index.query(lat, lon), expected)
        rng = np.random.default_rng(17)
        lat, lon = rng.uniform(-90, 90, 5000), rng.uniform(-180, 180, 5000)
        np.testing.assert_array_equal(index.query(lat, lon), direct.query(lat, lon))

    def test_interiors_need_no_polygon_queries_and_boundary_budget_refuses(self):
        direct = DirectRegions([shapely.box(-180, -90, 180, 90)])
        index = CellRegions(direct, level=8)
        self.assertEqual(index.summary()["table_bytes"], 131072)
        self.assertEqual(index.summary()["interior_cells"], 65536)
        with patch(
            "species_geo_index.shapely.covers",
            side_effect=AssertionError("interior path queried polygons"),
        ):
            np.testing.assert_array_equal(
                index.query([0, 90], [0, 180]), ((0, 1), (0, 0))
            )
        boundary = CellRegions(DirectRegions([shapely.box(-1, -1, 1, 1)]), level=2)
        with self.assertRaisesRegex(ValueError, "before pair allocation"):
            boundary.query([0], [0], max_candidates=0)

    def test_nested_sibling_regions_are_union_not_holes_and_original_is_retained(self):
        outer, inner = shapely.box(-10, -10, 10, 10), shapely.box(-2, -2, 2, 2)
        geometry = shapely.MultiPolygon((outer, inner))
        self.assertFalse(geometry.is_valid)
        source = json.dumps(
            {
                "type": "FeatureCollection",
                "features": [
                    {
                        "type": "Feature",
                        "properties": {"LEVEL3_COD": "OWN"},
                        "geometry": json.loads(shapely.to_geojson(geometry)),
                    }
                ],
            }
        )
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.species-geo."
        ) as directory:
            path = Path(directory) / "regions.json"
            path.write_text(source)
            with self.assertRaises(ValueError):
                read_regions(path)
            codes, normalized, records = read_regions(path, union_multipart=True)
            self.assertEqual(path.read_text(), source)
        self.assertEqual(codes, ("OWN",))
        self.assertTrue(normalized[0].covers(shapely.Point(0, 0)))
        self.assertEqual(normalized[0].area, 400)
        self.assertEqual(records[0]["summed_source_area_deg2"], 416)


if __name__ == "__main__":
    unittest.main()
