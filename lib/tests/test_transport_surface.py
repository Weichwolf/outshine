"""Counterexamples for closing already conforming XY transport surfaces."""

import unittest

import numpy as np
from shapely.geometry import LineString, box

from outshine_ref import buildings, transport, vegetation
from outshine_ref.contracts import BuildingFootprint, HeightInterval, LocalFrame, Mesh, Reservation, SourceFeature, SourceInputs
from outshine_ref.pipeline import prepare
from outshine_ref.transport.surface import close_surface, _sorted_boundary


def audit_closed(mesh):
    faces = mesh.triangles
    edges = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    _, inverse, counts = np.unique(np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True)
    signs = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    sums = np.bincount(inverse, weights=signs, minlength=len(counts))
    if not np.all(counts == 2) or not np.all(sums == 0):
        raise ValueError("body is not a closed oriented indexed mesh")
    triangles = mesh.vertices[faces] - mesh.vertices[0]
    return float(np.einsum("ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])).sum() / 6)


class SolidCases(unittest.TestCase):
    def square(self):
        return Mesh(np.array([[0., 0., 3.], [4, 0, 3], [4, 4, 3], [0, 4, 3]]), np.array([[0, 1, 2], [0, 2, 3]]))

    def test_flat_slope_and_reversed_input_keep_shared_vertices_and_volume(self):
        source = self.square()
        for heights in (source.vertices[:, 2], source.vertices[:, 0] * .06 + source.vertices[:, 1] * .02):
            top = source.vertices.copy()
            top[:, 2] = heights
            for faces in (source.triangles, source.triangles[:, ::-1]):
                surface = Mesh(top, faces)
                result = close_surface(surface, .25)
                self.assertAlmostEqual(audit_closed(result), 4)
                np.testing.assert_array_equal(result.vertices[:4], top)
                self.assertEqual(len(result.triangles), 12)
        self.assertEqual(source.vertices[0, 2], 3)

    def test_inner_hole_stays_open_and_large_translation_preserves_volume(self):
        outer = np.array([[0., 0.], [8, 0], [8, 8], [0, 8]])
        inner = np.array([[2., 2.], [6, 2], [6, 6], [2, 6]])
        faces = []
        for a in range(4):
            b = (a + 1) % 4
            faces.extend(((a, b, b + 4), (a, b + 4, a + 4)))
        vertices = np.c_[np.vstack((outer, inner)), np.full(8, 6.)]
        for offset in (np.zeros(3), np.array([1e7, -2e7, 170.])):
            result = close_surface(Mesh(vertices + offset, np.array(faces)), .5)
            self.assertAlmostEqual(audit_closed(result), 24)
            self.assertEqual(len(result.triangles), 32)
            self.assertFalse(np.any(np.all((result.vertices[:, :2] - offset[:2] > 2) & (result.vertices[:, :2] - offset[:2] < 6), axis=1)))

    def test_invalid_depth_degenerate_face_and_nonmanifold_boundary_are_rejected(self):
        for depth in (0, -1, float("nan"), float("inf")):
            with self.subTest(depth=depth), self.assertRaises(ValueError):
                close_surface(self.square(), depth)
        invalid_faces = (np.array([[0, 0, 1]]), np.array([[0, 1, 2], [0, 1, 2]]),
                         np.array([[0, 1, 2], [0, 2, 3], [0, 1, 3]]))
        for faces in invalid_faces:
            with self.subTest(faces=faces.tolist()), self.assertRaises(ValueError):
                close_surface(Mesh(self.square().vertices, faces), .25)
        top = np.array([[0., 0., 0.], [1, 0, 0], [0, 1, 0], [-1, 0, 0], [0, -1, 0]])
        with self.assertRaises(ValueError):
            close_surface(Mesh(top, np.array([[0, 1, 2], [0, 3, 4]])), .25)

    def test_disconnected_parts_and_empty_surface(self):
        square = self.square()
        surface = Mesh(np.vstack((square.vertices, square.vertices + [10, 0, 5])),
                       np.vstack((square.triangles, square.triangles + 4)))
        self.assertAlmostEqual(audit_closed(close_surface(surface, .25)), 8)
        with self.assertRaises(ValueError):
            close_surface(Mesh(np.empty((0, 3)), np.empty((0, 3), dtype=int)), .25)

    def test_unrepresentable_depth_and_overflow_are_rejected(self):
        square = self.square()
        vertices = square.vertices.copy()
        vertices[:, 2] = 1e20
        with self.assertRaises(ValueError):
            close_surface(Mesh(vertices, square.triangles), .25)
        vertices[:, 2] = -1e308
        with self.assertRaises(ValueError):
            close_surface(Mesh(vertices, square.triangles), 1e308)

    def test_closed_bridge_reaches_consumers_without_cutting_lower_buildings(self):
        source = self.square()
        bridge_top = source.vertices + [0., 0., 5.]
        bridge = close_surface(Mesh(bridge_top, source.triangles), .25)
        frame = LocalFrame("bridge-enu")
        house = BuildingFootprint("house", box(0, 0, 4, 4), HeightInterval(0, 3), attributes={"building": "house"})
        road = SourceFeature("road", LineString([(0, 2), (4, 2)]), {"brunnel": "bridge"})
        inputs = SourceInputs(frame, (road,), (house,))
        space = Reservation("road", box(0, 0, 4, 4), HeightInterval(7.75, 13), "traffic")
        consumers = []

        def houses(context):
            consumers.append("buildings")
            self.assertEqual(context.footprints, (house,))
            self.assertFalse(context.reservations[0].overlaps(house))
            return buildings.Plan(frame)

        def plants(context):
            consumers.append("vegetation")
            self.assertEqual(context.reservations, (space,))
            return vegetation.Plan(frame)

        world = prepare(inputs, lambda _: transport.Plan(frame, reservations=(space,), meshes=(bridge,)), houses, plants)
        self.assertEqual(consumers, ["buildings", "vegetation"])
        self.assertIs(world.sources, inputs)
        self.assertIs(world.transport.meshes[0], bridge)
        self.assertEqual(world.sources.transport[0].attributes["brunnel"], "bridge")

    def test_edge_methods_reject_the_same_invalid_topology(self):
        for faces in (np.array([[0, 1, 2], [0, 1, 2]]), np.array([[0, 1, 2]] * 3)):
            for method in (_sorted_boundary,):
                with self.subTest(method=method.__name__), self.assertRaises(ValueError):
                    method(faces)


if __name__ == "__main__":
    unittest.main()
