"""Exact original prism triangles and retained separate point-contact fans."""

import unittest

import numpy as np
import shapely
from outshine_ref.buildings.modelling import PrismVolume
from outshine_ref.buildings.modelling.prism_source_topology import (
    fan_topology,
    topology,
)
from outshine_ref.buildings.modelling.prism_union import primitive_mesh
from shapely.affinity import rotate, translate


class PrismSourceTopology(unittest.TestCase):
    def test_four_unique_vertices_preserve_every_original_triangle_in_order(self):
        controls = (
            shapely.box(0, 0, 5, 3),
            shapely.Polygon([(0, 0), (5, 0), (1, 1), (0, 3)]),
            shapely.Polygon([(0, 0), (2, 0), (4, 0), (0, 3)]),
        )
        for polygon in controls:
            for candidate in (
                polygon,
                shapely.reverse(polygon),
                rotate(polygon, 37),
                translate(polygon, 1e8, -1e8),
            ):
                source = PrismVolume("cut", candidate, 2, 5, 17)
                vertices, faces = primitive_mesh(source)
                mesh, roots = topology(candidate, vertices, faces)
                self.assertEqual(len(mesh["vertices"]), 8)
                self.assertEqual(mesh["faces"].dtype, np.dtype(np.uint64))
                self.assertEqual(
                    vertices[faces].tobytes(), mesh["vertices"][mesh["faces"]].tobytes()
                )
                np.testing.assert_array_equal(roots, np.arange(8))
                old, old_roots = fan_topology(candidate, vertices, faces)
                self.assertEqual(
                    old["vertices"][old["faces"]].tobytes(),
                    mesh["vertices"][mesh["faces"]].tobytes(),
                )
                self.assertEqual(len(np.unique(old_roots)), 8)

    def test_holes_many_corners_and_repeated_corner_keep_exact_original_fan_path(self):
        polygons = (
            shapely.Polygon(
                [(0, 0), (5, 0), (5, 4), (0, 4)], [[(1, 1), (1, 2), (2, 2), (2, 1)]]
            ),
            shapely.Polygon([(0, 0), (4, 0), (4, 2), (2, 1), (0, 2)]),
            shapely.Polygon([(0, 0), (4, 0), (4, 0), (0, 3)]),
        )
        for polygon in polygons:
            vertices, faces = primitive_mesh(PrismVolume("cut", polygon, 0, 2))
            old, old_roots = fan_topology(polygon, vertices, faces)
            mesh, roots = topology(polygon, vertices, faces)
            for key in old:
                self.assertEqual(old[key].tobytes(), mesh[key].tobytes())
            self.assertEqual(old_roots.tobytes(), roots.tobytes())

    def test_point_touching_components_keep_two_distinct_fans_at_each_shared_point(
        self,
    ):
        polygon = shapely.MultiPolygon(
            [shapely.box(0, 0, 1, 1), shapely.box(1, 1, 2, 2)]
        )
        vertices, faces = primitive_mesh(PrismVolume("cut", polygon, 0, 2))
        old, old_roots = fan_topology(polygon, vertices, faces)
        mesh, roots = topology(polygon, vertices, faces)
        self.assertEqual(old_roots.tobytes(), roots.tobytes())
        for key in old:
            self.assertEqual(old[key].tobytes(), mesh[key].tobytes())
        for height in (0, 2):
            selected = np.all(mesh["vertices"] == [1, 1, height], axis=1)
            self.assertEqual(len(np.unique(roots[selected])), 2)


if __name__ == "__main__":
    unittest.main()
