"""Clipping retains affine heights, holes and distinct traffic floors."""

import unittest

import numpy as np
import shapely
from ground_surface_showcase import body, combined
from outshine_ref.contracts import Mesh
from retained_surface import surface_domain
from whole_tile_showcase import crop_surfaces, tile_region


def surface(domain, offset):
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(domain))
    points = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    xy, labels = np.unique(points.reshape(-1, 2), axis=0, return_inverse=True)
    return Mesh(
        np.c_[xy, 0.1 * xy[:, 0] + 0.02 * xy[:, 1] + offset], labels.reshape(-1, 3)
    )


class WholeTileChecks(unittest.TestCase):
    def test_clipped_hole_and_affine_height_remain_exact(self):
        domain = shapely.Polygon(
            [(0, 0), (4, 0), (4, 4), (0, 4)], [[(1, 1), (2, 1), (2, 2), (1, 2)]]
        )
        original = surface(domain, 5.0)
        model, colours = body(original, np.zeros(len(original.triangles), int), 0.25)
        region = shapely.box(0.5, 0.5, 3.5, 3.5)
        parts, checks = crop_surfaces(
            model, colours, region, [{"slots": [0], "depth_m": 0.25}]
        )
        model = parts[0][0]
        triangles = model.vertices[model.triangles]
        a, b = (
            triangles[:, 1, :2] - triangles[:, 0, :2],
            triangles[:, 2, :2] - triangles[:, 0, :2],
        )
        positive = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] > 0
        top = Mesh(model.vertices, model.triangles[positive])
        self.assertLess(
            surface_domain(top).symmetric_difference(domain.intersection(region)).area,
            1e-10,
        )
        vertices = model.vertices[np.unique(model.triangles[positive])]
        np.testing.assert_allclose(
            vertices[:, 2],
            0.1 * vertices[:, 0] + 0.02 * vertices[:, 1] + 5.0,
            atol=1e-12,
        )
        self.assertLess(checks[0]["coverage_error_m2"], 1e-10)

    def test_same_xy_different_floors_are_clipped_independently(self):
        domain = shapely.box(0, 0, 4, 4)
        top, ground = surface(domain, 13.0), surface(domain, 5.0)
        original, colours = combined(
            (body(top, np.full(2, 3), 0.35), body(ground, np.zeros(2, int), 0.25))
        )
        parts, checks = crop_surfaces(
            original,
            colours,
            shapely.box(1, 1, 3, 3),
            [{"slots": [3], "depth_m": 0.35}, {"slots": [0], "depth_m": 0.25}],
        )
        self.assertEqual(len(parts), 2)
        self.assertGreater(
            parts[0][0].vertices[:, 2].min(), parts[1][0].vertices[:, 2].max() + 7.0
        )
        self.assertTrue(all(check["coverage_error_m2"] < 1e-10 for check in checks))

    def test_surface_groups_cannot_drop_or_duplicate_source_faces(self):
        original = surface(shapely.box(0, 0, 4, 4), 5.0)
        mesh, colours = body(original, np.zeros(2, int), 0.25)
        group = {"slots": [0], "depth_m": 0.25}
        for groups in ([{"slots": [5], "depth_m": 5.0}], [group, group]):
            with self.assertRaises(ValueError):
                crop_surfaces(mesh, colours, shapely.box(1, 1, 3, 3), groups)

    def test_neighbouring_real_tile_boundaries_share_exact_edges(self):
        west, _ = tile_region("Vienna-Donau-West")
        east, _ = tile_region("Vienna-Donau")
        south, _ = tile_region("Vienna-Donau-South")
        self.assertEqual(west.bounds[2], east.bounds[0])
        self.assertEqual(east.bounds[1], south.bounds[3])
        self.assertGreater(west.intersection(east).length, 1600.0)
        self.assertGreater(east.intersection(south).length, 1600.0)
        union = shapely.union_all((west, east, south))
        self.assertEqual(union.geom_type, "Polygon")
        self.assertEqual(len(union.interiors), 0)


if __name__ == "__main__":
    unittest.main()
