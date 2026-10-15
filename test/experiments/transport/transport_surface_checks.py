"""Whole-area, unknown, priority, holes and level-bound source counterexamples."""

import unittest

import numpy as np
import shapely
from transport_surface_areas import centroid_areas, family_areas
from transport_surface_families import family_domains, old_ground_records, source_family


class SurfaceFamilies(unittest.TestCase):
    def test_paved_is_evidence_not_surveyed_asphalt(self):
        self.assertEqual(
            source_family({"class": "minor", "surface": "paved"}),
            (1, "tag_paved_recipe_asphalt"),
        )
        self.assertEqual(source_family({"class": "path"})[0], 6)
        self.assertEqual(source_family({"class": "path", "surface": "alien"})[0], 0)

    def test_upper_and_deep_sources_do_not_paint_ground(self):
        sources = [
            {
                "role": role,
                "source_id": role,
                "properties": {"class": "rail"},
                "xy": [[0, 0], [2, 0]],
                "width_m": 3.8,
            }
            for role in ["ground", "deck", "approach_ground", "unresolved"]
        ]
        records = old_ground_records(sources)
        self.assertEqual([r["sources"] for r in records], [["ground"]])

    def test_priority_preserves_disjoint_family_area(self):
        a, b = shapely.box(0, 0, 2, 2), shapely.box(1, 0, 3, 2)
        domains, report = family_domains(
            [{"family": 1, "shape": a}, {"family": 5, "shape": b}]
        )
        self.assertAlmostEqual(domains[1].area, 4)
        self.assertAlmostEqual(domains[5].area, 2)
        self.assertAlmostEqual(report[5]["precedence_removed_m2"], 2)
        self.assertEqual(domains[1].intersection(domains[5]).area, 0)

    def test_interior_island_missed_by_centroid_has_real_area(self):
        triangle = np.array([[[0, 0], [10, 0], [0, 10]]], float)
        domains = [shapely.Polygon(), shapely.box(1, 1, 2, 2)]
        exact, reason, _ = family_areas(triangle, domains)
        sampled = centroid_areas(triangle, domains)
        self.assertEqual(int(reason[0]), 0)
        np.testing.assert_allclose(exact, [[49, 1]], rtol=0, atol=1e-12)
        np.testing.assert_array_equal(sampled, [[50, 0]])

    def test_hole_is_unknown_not_material(self):
        domain = shapely.box(0, 0, 10, 10).difference(shapely.box(1, 1, 2, 2))
        triangle = np.array([[[0, 0], [10, 0], [0, 10]]], float)
        exact, reason, _ = family_areas(triangle, [shapely.Polygon(), domain])
        np.testing.assert_allclose(exact, [[1, 49]], rtol=0, atol=1e-12)
        self.assertEqual(int(reason[0]), 0)

    def test_whole_and_split_cells_preserve_same_areas(self):
        domain = (
            shapely.Point(0, 0)
            .buffer(4, quad_segs=128)
            .difference(shapely.box(-1, -1, 1, 1))
        )
        triangles = np.array(
            [[[-5, -5], [5, -5], [5, 5]], [[-5, -5], [5, 5], [-5, 5]]], float
        )
        a, _, _ = family_areas(triangles, [shapely.Polygon(), domain], method="whole")
        b, _, work = family_areas(
            triangles, [shapely.Polygon(), domain], method="cells"
        )
        self.assertGreater(work["splits"], 0)
        np.testing.assert_allclose(a, b, rtol=0, atol=1e-12)

    def test_query_budget_falls_back_locally_without_losing_geometry(self):
        triangle = np.array([[[0, 0], [10, 0], [0, 10]]], float)
        a, reason, _ = family_areas(
            triangle, [shapely.Polygon(), shapely.box(0, 0, 10, 10)], max_pairs=0
        )
        np.testing.assert_array_equal(a, [[50, 0]])
        self.assertEqual(int(reason[0]), 2)

    def test_tiny_unresolved_face_stays_explicitly_unknown(self):
        triangle = np.array([[[0, 0], [1e-6, 0], [0, 1e-6]]])
        a, reason, _ = family_areas(
            triangle, [shapely.Polygon(), shapely.box(0, 0, 10, 10)]
        )
        self.assertEqual(int(reason[0]), 3)
        np.testing.assert_array_equal(a[:, 1], 0)


if __name__ == "__main__":
    unittest.main()
