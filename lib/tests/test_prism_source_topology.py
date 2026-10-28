"""Exact original prism triangles and retained separate point-contact fans."""

import unittest
from unittest.mock import patch

import manifold3d
import numpy as np
import shapely
from outshine_ref.buildings.modelling import PrismVolume, prism_cut
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
                bounds = np.asarray(candidate.bounds)
                origin = np.r_[(bounds[:2] + bounds[2:]) / 2, 0]
                identifier = manifold3d.Manifold.reserve_ids(1)
                channels = tuple(["uv"] + ["uv" + str(i) for i in range(1, 10)])
                with patch.object(prism_cut, "topology", fan_topology):
                    before, count = prism_cut.prism_source(
                        source, origin, identifier, channels
                    )
                after, new_count = prism_cut.prism_source(
                    source, origin, identifier, channels
                )
                self.assertEqual(count, new_count)
                np.testing.assert_allclose(
                    [before.volume(), after.volume()],
                    candidate.area * 3,
                    rtol=0,
                    atol=1e-9,
                )
                np.testing.assert_allclose(
                    [before.surface_area(), after.surface_area()],
                    2 * candidate.area + candidate.length * 3,
                    rtol=0,
                    atol=1e-9,
                )
                encoded = after.to_mesh64()
                self.assertEqual(encoded.vert_properties.shape[1], 23)
                self.assertTrue(np.all(encoded.vert_properties[:, 3:] == 0))
                self.assertTrue(np.all(np.asarray(encoded.face_id) >= 0))
                self.assertTrue(np.all(np.asarray(encoded.face_id) < count))

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
