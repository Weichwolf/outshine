import unittest
from dataclasses import replace
from io import BytesIO

import numpy as np
import shapely
from PIL import Image

from outshine_ref.buildings.modelling import Asset, Branch, Face, Frame, Material, Part, Shape, Texture
from outshine_ref.buildings.modelling.selectors import tagged


def volume(mesh):
    triangles = mesh['vertices'][mesh['faces']]
    return np.einsum('ij,ij->', triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])) / 6


class InstalledShape(unittest.TestCase):
    def test_courtyard_composition_keeps_holes_materials_and_volume(self):
        polygon = shapely.Polygon([(0, 0), (15, 0), (15, 12), (0, 12)],
                                  [[(3, 3), (3, 8), (9, 8), (9, 3)]])
        source = Shape.from_polygon(polygon, 2).with_metadata({0: 'courtyard'}, {1: Material('wall')})
        body = source.extrude(7)
        self.assertAlmostEqual(volume(body.emit()), polygon.area * 7)
        children = body.split('x', Part(7.5), Part(7.5))
        self.assertAlmostEqual(sum(volume(child.emit()) for child in children), volume(body.emit()))
        for child in children:
            self.assertEqual(child.geometry.group_names, source.geometry.group_names)
            self.assertEqual(child.geometry.material_defs, source.geometry.material_defs)
        tops = body.comp('f', Branch(tagged('extrude.top')), alignment='noAlign')
        self.assertEqual(len(tops), 1)
        self.assertEqual(len(tops[0].geometry.polygons[0].holes), 1)

    def test_primitive_ownership_and_arbitrary_native_uvs(self):
        vertices = np.array([[0., 0., 0.], [4., 0., 0.], [0., 0., 3.]])
        mesh = dict(vertices=vertices, faces=np.array([[0, 2, 1]]),
                    uv=np.array([[1., 2.], [3., 4.], [5., 6.]]), uv9=np.array([[7., 8.], [9., 10.], [11., 12.]]))
        shape = Shape.from_mesh(mesh)
        vertices[:] = 99
        np.testing.assert_array_equal(shape.emit()['vertices'], [[0, 0, 0], [4, 0, 0], [0, 0, 3]])
        rotated = shape.rotate_scope(17, -28, 39)
        self.assertIs(rotated.geometry.fields['uv9'], shape.geometry.fields['uv9'])
        np.testing.assert_allclose(rotated.emit()['vertices'], shape.emit()['vertices'], atol=1e-12)


class ScopeGeometry(unittest.TestCase):
    def test_geometry_normals_and_payloads_stay_fixed_under_nested_signed_frames(self):
        source = Shape.from_faces([(0., 0., 0.), (6., 0., 0.), (6., 0., 4.), (0., 0., 4.)],
                                  [Face((0, 3, 2, 1), tags={'source'})]).extrude(3)
        mesh = source.emit()
        normals = np.tile([.2, .7, .4], (len(mesh['vertices']), 1))
        source = Shape.from_mesh(mesh | dict(normals=normals), source.scope, source.geometry.polygons)
        scope = replace(source.scope, position=(1, -4, 7), orientation=(20, 35, -17), size=(-6, 3, 4),
                        pivot=Frame((3, 2, -1), (35, -20, 12)), object_frame=Frame((1e8, 10, -80), (-5, 70, 9)))
        source = replace(source, scope=scope)
        before = source.emit()
        changed = source.rotate_scope(31, -22, 63)
        after = changed.emit()
        np.testing.assert_allclose(after['vertices'], before['vertices'], atol=2e-8, rtol=0)
        np.testing.assert_allclose(after['normals'], before['normals'], atol=1e-12, rtol=0)
        self.assertEqual(changed.scope.orientation, (51, 13, 46))
        self.assertIs(changed.scope.pivot, source.scope.pivot)
        self.assertIs(changed.geometry.polygons, source.geometry.polygons)
        for name in ('faces', 'uv', 'parts', 'materials', 'face_tag_mask'):
            self.assertIs(changed.geometry.fields[name], source.geometry.fields[name])
        self.assertTrue(np.all(np.asarray(changed.scope.size) >= 0))
        np.testing.assert_allclose(changed.geometry.unit_vertices.min(axis=0), [0, 0, 0], atol=1e-12)
        np.testing.assert_allclose(changed.geometry.unit_vertices.max(axis=0), [1, 1, 1], atol=1e-12)

    def test_scope_only_rotation_changes_next_roof_direction(self):
        shape = Shape.from_polygon(shapely.box(0, 0, 8, 6), 3)
        rotated = shape.rotate_scope(0, 31, 0)
        np.testing.assert_allclose(rotated.emit()['vertices'], shape.emit()['vertices'], atol=1e-12)
        roof = rotated.roof_ridge(2, 'byHeight')
        np.testing.assert_allclose(roof.roof_info.ridge_directions[0], [np.cos(np.deg2rad(31)), 0, -np.sin(np.deg2rad(31))], atol=1e-12)

    def test_empty_geometry_changes_only_angles(self):
        shape = Shape.from_mesh(dict(vertices=np.empty((0, 3)), faces=np.empty((0, 3), dtype=int)))
        rotated = shape.rotate_scope(12, 34, 56)
        self.assertIs(rotated.geometry, shape.geometry)
        self.assertEqual(rotated.scope.position, shape.scope.position)
        self.assertEqual(rotated.scope.size, shape.scope.size)
        with self.assertRaises(ValueError):
            shape.rotate_scope(float('nan'), 0, 0)

    def test_inserted_assets_keep_world_geometry_and_resources(self):
        body = Shape.from_polygon(shapely.box(0, 0, 8, 6)).extrude(4)
        asset = Asset.from_shape(body, 'cached-control')
        placed = body.insert(asset).s(10, 3, 5).r(0, 27, 0)
        rotated = placed.rotate_scope(0, -42, 0)
        np.testing.assert_allclose(rotated.emit()['vertices'], placed.emit()['vertices'], atol=1e-12)
        self.assertIs(rotated.asset, asset)
        self.assertFalse(rotated.is_instanced())

    def test_prepared_texture_and_projection_keep_the_same_uv_field(self):
        encoded = BytesIO()
        Image.new('RGBA', (8, 8), (30, 60, 90, 180)).save(encoded, format='PNG')
        texture = Texture('procedural-control.png', encoded.getvalue())
        source = Shape.from_polygon(shapely.box(0, 0, 8, 6)).texture(texture)
        source = source.setup_projection(0, 'scope.xz', 8, 6)
        rotated = source.rotate_scope(17, -28, 39)
        np.testing.assert_allclose(rotated.project_uv(0).emit()['uv'], source.project_uv(0).emit()['uv'], atol=1e-12)
        self.assertIs(rotated.textures[0], texture)
        self.assertEqual(rotated.projections, source.projections)
        self.assertEqual(rotated.material, source.material)


if __name__ == '__main__':
    unittest.main()
