import unittest
from io import BytesIO

import shapely
from PIL import Image

from outshine_ref.buildings.modelling import Material, Shape, Texture
from outshine_ref.buildings.modelling.material import resolved_materials


def prepared_source():
    image = BytesIO()
    Image.new('RGBA', (4, 4), (70, 130, 180, 200)).save(image, format='PNG')
    texture = Texture('procedural-component.png', image.getvalue())
    footprint = shapely.Polygon([(0, 0), (8, 0), (8, 6), (0, 6)],
                                [[(2, 2), (2, 4), (5, 4), (5, 2)]])
    shape = Shape.from_polygon(footprint).extrude(4)
    shape = shape.with_metadata(material_defs={1: Material('base', {'roughness': .9})})
    shape = shape.texture(texture).set_material({'roughness': .23, 'metallic': .61})
    return shape.setup_projection(0, 'scope.xz', 8, 6), texture


class ComponentAppearanceTests(unittest.TestCase):
    def test_surface_and_group_components_keep_prepared_appearance(self):
        source, texture = prepared_source()
        for kind in ('f', 'g', 'm', 'h'):
            children = source.comp(kind)
            self.assertTrue(children, kind)
            for child in children:
                self.assertEqual(child.material, source.material, kind)
                self.assertEqual(child.projections, source.projections, kind)
                self.assertEqual(child.textures, source.textures, kind)
                self.assertIs(child.textures[0], texture)
                self.assertEqual(child.geometry.material_defs, source.geometry.material_defs)
                for material in resolved_materials(child).values():
                    self.assertEqual(dict(material.attributes)['roughness'], .23)

    def test_nested_selection_shares_encoded_texture_and_projection(self):
        source, texture = prepared_source()
        groups = source.comp('g')
        for group in groups:
            for child in group.comp('f'):
                self.assertIs(child.textures[0].encoded, texture.encoded)
                self.assertEqual(child.projections, source.projections)
                self.assertEqual(child.material, source.material)


if __name__ == '__main__':
    unittest.main()
