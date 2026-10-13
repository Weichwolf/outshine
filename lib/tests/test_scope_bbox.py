import unittest
from dataclasses import replace

import numpy as np
import shapely
from shapely import affinity

from outshine_ref.buildings.modelling import Asset, Frame, Shape
from outshine_ref.buildings.modelling.scope_bbox import SELECTORS
from outshine_ref.buildings.modelling.wire import Edge, Point


class ScopeBBox(unittest.TestCase):
    def test_all_six_selectors_preserve_geometry_and_fix_the_third_axis(self):
        polygon = affinity.rotate(shapely.box(0, 0, 12, 5), 31, origin=(0, 0))
        body = Shape.from_polygon(polygon, 2).extrude(7)
        body = replace(body, scope=replace(body.scope, orientation=(17, -28, 39),
                                           pivot=Frame((7, -3, 8), (7, 11, 19)),
                                           object_frame=Frame((1e8, 200, -300), (13, 17, 23))))
        before = body.emit()
        for selector in SELECTORS:
            a, b = ('xyz'.index(axis) for axis in selector)
            c = 3-a-b
            fitted = body.align_scope_to_geometry_bbox(selector)
            np.testing.assert_allclose(fitted.emit()['vertices'], before['vertices'], atol=3e-8, rtol=0)
            np.testing.assert_allclose(fitted.scope.axes[:, c], body.scope.axes[:, c], atol=1e-12, rtol=0)
            self.assertGreaterEqual(fitted.scope.size[a]+1e-10, fitted.scope.size[b])
            np.testing.assert_allclose(fitted.scope.origin, fitted.scope.pivot_origin, atol=1e-12)
            self.assertEqual(fitted.scope.position, (0, 0, 0))
            self.assertEqual(fitted.scope.orientation, (0, 0, 0))
            self.assertIs(fitted.geometry.polygons, body.geometry.polygons)
            for key in body.geometry.fields:
                self.assertIs(fitted.geometry.fields[key], body.geometry.fields[key])

    def test_rotated_rectangle_gets_minimum_area_and_expected_roof_axis(self):
        polygon = affinity.rotate(shapely.box(0, 0, 12, 5), 31, origin=(0, 0))
        source = Shape.from_polygon(polygon, 3)
        for selector, a, b in (('xz', 0, 2), ('zx', 2, 0)):
            fitted = source.align_scope_to_geometry_bbox(selector)
            self.assertAlmostEqual(fitted.scope.size[a], 12)
            self.assertAlmostEqual(fitted.scope.size[b], 5)
            self.assertAlmostEqual(fitted.scope.size[a]*fitted.scope.size[b], polygon.area)
        roof = source.align_scope_to_geometry_bbox('xz').roof_ridge(2, 'byHeight')
        np.testing.assert_allclose(roof.roof_info.ridge_directions[0], [np.cos(np.deg2rad(31)), 0, -np.sin(np.deg2rad(31))], atol=1e-12)

    def test_native_normals_extra_uvs_and_instance_payload_survive(self):
        shape = Shape.from_polygon(shapely.box(0, 0, 8, 6)).extrude(4)
        mesh = shape.emit()
        mesh['normals'] = np.tile([.2, .7, .4], (len(mesh['vertices']), 1))
        mesh['uv9'] = np.arange(len(mesh['vertices'])*2, dtype=float).reshape(-1, 2)
        shape = Shape.from_mesh(mesh, shape.scope, shape.geometry.polygons)
        asset = Asset.from_shape(shape)
        placed = shape.insert(asset).s(-14, 2, 9).r(17, 27, -39)
        fitted = placed.align_scope_to_geometry_bbox('zx')
        for name in ('vertices', 'normals', 'uv9'):
            np.testing.assert_allclose(fitted.emit()[name], placed.emit()[name], atol=1e-12, rtol=0)
        self.assertIs(fitted.asset, asset)
        self.assertIs(fitted.geometry.fields['uv9'], placed.geometry.fields['uv9'])

    def test_line_and_point_are_valid_degenerate_projections(self):
        wire = Shape.from_edges([(0., 0., 0.), (3., 4., 0.)], [Edge((0, 1))], [[0., 0., 1.]])
        fitted = wire.align_scope_to_geometry_bbox('xy')
        self.assertAlmostEqual(fitted.scope.size[0], 5)
        self.assertAlmostEqual(fitted.scope.size[1], 0)
        np.testing.assert_allclose(fitted.emit()['edges_normals'], wire.emit()['edges_normals'], atol=1e-12)
        point = Shape.from_points([(7., 8., 9.)], [Point((0,))], [[0., 0., 1.]])
        fitted = point.align_scope_to_geometry_bbox('yz')
        np.testing.assert_allclose(fitted.emit()['vertices'], [[7, 8, 9]], atol=1e-12)
        self.assertEqual(fitted.scope.size, (0, 0, 0))

    def test_unused_vertices_do_not_inflate_the_indexed_box(self):
        mesh = dict(vertices=np.array([[0., 0., 0.], [4, 0, 0], [0, 3, 2], [90, 80, 70]]),
                    faces=np.array([[0, 1, 2]]))
        source = Shape.from_mesh(mesh)
        fitted = source.align_scope_to_geometry_bbox('xy')
        self.assertLess(np.prod(fitted.scope.size), 100)
        np.testing.assert_allclose(fitted.emit()['vertices'], mesh['vertices'], atol=1e-12)
        self.assertIs(fitted.geometry.fields['faces'], source.geometry.fields['faces'])
        mesh['vertices'][2, 2] = 0
        with self.assertRaises(NotImplementedError):
            Shape.from_mesh(mesh).align_scope_to_geometry_bbox('xy')

    def test_invalid_selectors_and_unreferenced_shapes_are_rejected(self):
        shape = Shape.from_polygon(shapely.box(0, 0, 8, 6))
        for selector in ('xx', 'xyz', None, [], True):
            with self.subTest(selector=selector), self.assertRaises(ValueError):
                shape.align_scope_to_geometry_bbox(selector)
        empty = Shape.from_mesh(dict(vertices=np.empty((0, 3)), faces=np.empty((0, 3), dtype=int)))
        with self.assertRaises(ValueError):
            empty.align_scope_to_geometry_bbox('xz')

    def test_square_ties_and_repeated_fitting_keep_a_stable_axis(self):
        polygon = affinity.rotate(shapely.box(0, 0, 5, 5), 31, origin=(0, 0))
        source = Shape.from_polygon(polygon, 3)
        for selector in ('xz', 'zx'):
            fitted = source.align_scope_to_geometry_bbox(selector)
            repeated = source.align_scope_to_geometry_bbox(selector)
            twice = fitted.align_scope_to_geometry_bbox(selector)
            np.testing.assert_array_equal(repeated.scope.axes, fitted.scope.axes)
            np.testing.assert_allclose(twice.scope.axes, fitted.scope.axes, atol=1e-12)
            np.testing.assert_allclose(twice.emit()['vertices'], source.emit()['vertices'], atol=1e-12)
            self.assertAlmostEqual(fitted.scope.size[0]*fitted.scope.size[2], 25.)


if __name__ == '__main__':
    unittest.main()
