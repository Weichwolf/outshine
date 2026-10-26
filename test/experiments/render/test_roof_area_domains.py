"""Clipped source roofs retain area and keep zero-area contacts out of walls."""

import unittest
from pathlib import Path

import numpy as np
import shapely
from building_mass_appearance import source_measure
from building_mass_materials import policy
from building_source_tree import source_bounds
from column_roof_patches import polygonal_domain, roof_patches
from parent_surface_inputs import open_parent

INPUT = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/render/building-full-view/original-inputs.tar.gz"
)


class RoofAreaDomains(unittest.TestCase):
    def test_boundary_only_hip_facets_do_not_satisfy_roof_coverage(self):
        shape = {
            "ring": [(0, 0), (4, 0), (4, 4), (0, 4)],
            "holes": [],
            "roof_kind": 2,
            "rise_m": 1,
            "seat_m": 0,
            "foot_m": 0,
            "eaves_m": 3,
            "hip": {
                "maximum_distance_m": 1,
                "origin": [0, 0],
                "facets": [
                    {
                        "gradient": [0, 0],
                        "intercept_m": 0,
                        "ring": [(4, 0), (6, 0), (6, 4), (4, 4)],
                    }
                ],
            },
        }
        with self.assertRaisesRegex(ValueError, "hip facets do not cover"):
            roof_patches(shape)

    def test_polygon_words_and_real_holes_are_preserved(self):
        polygon = shapely.Polygon(
            [(0, 0), (5, 0), (5, 5), (0, 5)], [[(1, 1), (1, 2), (2, 2), (2, 1)]]
        )
        self.assertIs(polygonal_domain(polygon), polygon)
        multi = shapely.MultiPolygon([polygon, shapely.box(8, 0, 9, 1)])
        self.assertIs(polygonal_domain(multi), multi)
        collection = shapely.GeometryCollection(
            [
                multi,
                shapely.GeometryCollection(
                    [shapely.LineString([(0, 0), (-1, 0)]), shapely.Point(7, 7)]
                ),
            ]
        )
        result = polygonal_domain(collection)
        self.assertTrue(result.equals(multi))
        self.assertFalse(result.covers(shapely.Point(1.5, 1.5)))
        self.assertTrue(polygonal_domain(shapely.LineString([(0, 0), (1, 0)])).is_empty)

    def test_original_source_1414_has_complete_roof_area_and_materials(self):
        with open_parent(INPUT) as parent:
            _, _, frame = source_bounds(parent.plans, {18})
            found = None
            for _, identities, forms in parent.iter_forms({18}):
                for identity, surface in zip(identities, forms, strict=True):
                    if int(identity) == 1414:
                        found = surface
            self.assertIsNotNone(found)
            shape = found["shapes"][0]
            patches = roof_patches(shape)
            self.assertTrue(patches)
            self.assertTrue(
                all(
                    isinstance(p, (shapely.Polygon, shapely.MultiPolygon))
                    and p.area > 0
                    for p, _ in patches
                )
            )
            footprint = shapely.Polygon(shape["ring"], shape["holes"])
            self.assertEqual(
                shapely.union_all([p for p, _ in patches])
                .symmetric_difference(footprint)
                .area,
                0,
            )
            measure, count, calls = source_measure(1414, found, frame, policy())
            self.assertTrue(np.isfinite(measure).all())
            self.assertTrue((measure[:, -1] > 0).all())
            self.assertGreater(count, 0)
            self.assertGreater(calls, 0)


if __name__ == "__main__":
    unittest.main()
