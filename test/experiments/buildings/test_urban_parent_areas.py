"""Independent rectangle intersections, analytic areas and source contracts."""

import unittest

import numpy as np
import shapely
from test_urban_patches import source_document
from urban_parent_areas import region_areas
from urban_parent_stats import compile_parent


class ParentAreasTest(unittest.TestCase):
    def test_holes_disconnected_parts_edges_and_odd_grids_match_direct_intersections(
        self,
    ):
        outer = shapely.box(-0.3, -0.5, 7.1, 6.4)
        holes = shapely.union_all([shapely.box(1, 1, 3, 4), shapely.box(5, 0, 5.2, 6)])
        triangle = shapely.Polygon([(0.1, 0.2), (6.3, 0.7), (1.9, 6.1)])
        shapes = [
            shapely.difference(outer, holes),
            triangle,
            shapely.union_all([shapely.box(0, 0, 1, 1), shapely.box(4, 4, 7, 7)]),
            shapely.box(10, 10, 11, 11),
            shapely.box(-10, -10, 20, 20),
        ]
        for geometry in shapes:
            for nx, ny in [(1, 1), (3, 7), (7, 3), (16, 16), (32, 17)]:
                x, y = np.linspace(0, 7, nx + 1), np.linspace(0, 7, ny + 1)
                direct, _ = region_areas(geometry, x, y, "direct")
                actual, work = region_areas(geometry, x, y, "hierarchical")
                np.testing.assert_allclose(actual, direct, rtol=0, atol=2e-14)
                self.assertAlmostEqual(
                    actual.sum(),
                    shapely.area(
                        shapely.intersection(geometry, shapely.box(0, 0, 7, 7))
                    ),
                    places=12,
                )
                self.assertGreater(work["intersection_tests"], 0)

    def test_covered_and_empty_subtrees_do_not_expand_to_leaf_rectangles(self):
        x, y = np.linspace(0, 64, 65), np.linspace(0, 32, 33)
        for geometry, expected in [
            (shapely.box(-1, -1, 70, 40), 1.0),
            (shapely.box(70, 40, 80, 50), 0.0),
        ]:
            areas, work = region_areas(geometry, x, y, "hierarchical")
            np.testing.assert_array_equal(areas, expected)
            self.assertEqual(work["intersection_tests"], 1)
            self.assertEqual(work["hierarchy_nodes"], 1)

    def test_large_projected_coordinates_and_fractional_boundaries(self):
        ox, oy = 15536896.117358062, 4246229.795298112
        geometry = shapely.Polygon(
            [
                (ox, oy),
                (ox + 19000, oy + 300),
                (ox + 9000, oy + 17000),
                (ox + 100, oy + 15000),
            ],
            holes=[
                [
                    (ox + 3000, oy + 3000),
                    (ox + 7000, oy + 3000),
                    (ox + 7000, oy + 7000),
                    (ox + 3000, oy + 7000),
                ]
            ],
        )
        x, y = (
            np.linspace(ox, ox + 19567.879241004586, 33),
            np.linspace(oy, oy + 19567.879241004586, 33),
        )
        direct, _ = region_areas(geometry, x, y, "direct")
        actual, _ = region_areas(geometry, x, y, "hierarchical")
        np.testing.assert_allclose(actual, direct, atol=4e-6, rtol=1e-12)

    def test_compiler_keeps_provenance_and_height_recipe(self):
        doc = source_document()
        for divisions in (1, 3, 8):
            direct, dm = compile_parent(doc, divisions=divisions, area_method="direct")
            actual, am = compile_parent(
                doc, divisions=divisions, area_method="hierarchical"
            )
            for name in direct.arrays():
                np.testing.assert_allclose(
                    actual.arrays()[name], direct.arrays()[name], atol=1e-12, rtol=0
                )
            self.assertEqual(am["source_records"], dm["source_records"])
            self.assertEqual(am["child_cells_generated"], 0)
            self.assertEqual(am["status"], dm["status"])
        with self.assertRaises(ValueError):
            compile_parent(doc, area_method="unknown")


if __name__ == "__main__":
    unittest.main()
