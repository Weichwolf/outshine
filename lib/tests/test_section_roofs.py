"""Actual metrical roof folds, developed UVs, bounded work and closed consumers."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.buildings.modelling import heightfield_envelope
from outshine_ref.buildings.section_roofs import RoofSection, section_surface


class SectionRoofs(unittest.TestCase):
    def test_repeated_symmetric_and_asymmetric_roof_volume_and_exact_closure(self):
        for peak in (0.5, 0.8):
            knots = [(0, 5)]
            for i in range(3):
                knots.extend((((i + peak) * 8, 7), ((i + 1) * 8, 5)))
            mesh = section_surface(shapely.box(0, 0, 24, 30), RoofSection(tuple(knots)))
            closed = heightfield_envelope(mesh, 0)
            self.assertAlmostEqual(closed.volume_m3, 24 * 30 * 6, places=8)
            triangles = closed.vertex_roots[closed.mesh["faces"]]
            edges = np.concatenate(
                [triangles[:, [0, 1]], triangles[:, [1, 2]], triangles[:, [2, 0]]]
            )
            _, inverse, counts = np.unique(
                np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
            )
            np.testing.assert_array_equal(counts, 2)
            np.testing.assert_array_equal(
                np.bincount(
                    inverse, weights=np.where(edges[:, 0] < edges[:, 1], 1, -1)
                ),
                0,
            )
            np.testing.assert_array_equal(mesh["materials"], 9)
            self.assertEqual(set(mesh["parts"]), set(range(6)))

    def test_normals_and_developed_metric_uvs_match_every_actual_triangle(self):
        section = RoofSection(((0, 3), (4, 7), (12, 3)), (0.6, 0.8), (123, -45))
        polygon = shapely.transform(
            shapely.box(0, -10, 12, 20),
            lambda xy: xy @ section.axes + section.origin_xy,
        )
        mesh = section_surface(polygon, section, material=41)
        points, uv = mesh["vertices"][mesh["faces"]], mesh["uv"][mesh["faces"]]
        for a, b in ((0, 1), (1, 2), (2, 0)):
            np.testing.assert_allclose(
                np.linalg.norm(points[:, a] - points[:, b], axis=1),
                np.linalg.norm(uv[:, a] - uv[:, b], axis=1),
                atol=1e-10,
            )
        normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        normals /= np.linalg.norm(normals, axis=1, keepdims=True)
        np.testing.assert_allclose(mesh["normals"], np.repeat(normals, 3, axis=0))
        self.assertTrue(np.all(normals[:, 2] > 0))
        self.assertEqual(set(mesh["materials"]), {41})
        self.assertAlmostEqual(
            shapely.from_wkb(heightfield_envelope(mesh, 0).footprint_wkb)
            .symmetric_difference(polygon)
            .area,
            0,
            places=8,
        )

    def test_irregular_and_courtyard_domains_remain_unfilled_without_overlap(self):
        outer = shapely.Polygon(
            [(0, 0), (24, 0), (24, 30), (10, 30), (10, 20), (0, 20)]
        )
        for polygon in (outer, outer.difference(shapely.box(8, 8, 16, 18))):
            mesh = section_surface(polygon, RoofSection(((0, 4), (12, 7), (24, 4))))
            triangles = mesh["vertices"][mesh["faces"]]
            domains = shapely.polygons(triangles[:, :, :2])
            union = shapely.union_all(domains)
            self.assertLess(union.symmetric_difference(polygon).area, 1e-8)
            self.assertLess(float(shapely.area(domains).sum()) - union.area, 1e-8)
            self.assertLess(
                shapely.from_wkb(heightfield_envelope(mesh, 0).footprint_wkb)
                .symmetric_difference(polygon)
                .area,
                1e-8,
            )

    def test_invalid_frames_domains_and_materials_are_rejected(self):
        for knots in (
            [((0, 0), (1, 1))],
            ((0, 0), (0, 1)),
            ((0, 0), (1, float("nan"))),
        ):
            with self.assertRaises(ValueError):
                RoofSection(knots)
        with self.assertRaises(ValueError):
            RoofSection(((0, 0), (1, 1)), (2, 0))
        section = RoofSection(((0, 2), (10, 4)))
        for polygon in (
            shapely.Polygon(),
            shapely.box(0, 0, 11, 5),
            shapely.MultiPolygon([shapely.box(0, 0, 2, 2)]),
        ):
            with self.assertRaises(ValueError):
                section_surface(polygon, section)
        for material in (-1, True, 65536):
            with self.assertRaises(ValueError):
                section_surface(shapely.box(0, 0, 10, 5), section, material=material)

    def test_capacity_rejects_before_clipping_or_triangulation(self):
        with (
            patch(
                "shapely.intersection", side_effect=AssertionError("unneeded clipping")
            ),
            patch(
                "shapely.constrained_delaunay_triangles",
                side_effect=AssertionError("unneeded triangles"),
            ),
            self.assertRaisesRegex(ValueError, "capacity"),
        ):
            section_surface(
                shapely.box(0, 0, 10, 5),
                RoofSection(((0, 3), (5, 5), (10, 3))),
                max_triangles=1,
            )


if __name__ == "__main__":
    unittest.main()
