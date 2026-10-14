"""Prepared resource conflicts, immutable origins and logical face interoperability."""

from io import BytesIO
from dataclasses import replace
import unittest

import numpy as np
from PIL import Image
import shapely

from boolean_surface_probe import boolean
from boolean_semantic_overlay import overlay
from boolean_shape_probe import assemble
from outshine_ref.buildings.modelling import Branch,Material,Texture
from outshine_ref.buildings.modelling.material_schema import MAPS
from outshine_ref.buildings.modelling.selectors import tagged
from verify_boolean_attributes import authored
from verify_boolean_shape import verify
from verify_boolean_surfaces import Prism,reference,measure


def texture(path,colour):
    stream=BytesIO();Image.new('RGBA',(4,4),colour).save(stream,format='PNG')
    return Texture(path,stream.getvalue())


def source(prism,owner,resource):
    shape=authored(prism.shape(),owner).texture(resource)
    shape=shape.set_material({name:resource.path for name in MAPS})
    return shape.setup_projection(0,'scope.xz',8.,6.)


class ShapeControls(unittest.TestCase):
    def setUp(self):
        self.a=Prism(shapely.box(0,0,8,6),0.,4.)
        self.b=Prism(shapely.box(4,2,12,8),0.,4.)

    def test_conflicting_names_are_rewritten_per_material_with_shared_bytes(self):
        a=texture('same.png',(30,80,120,255));b=texture('same.png',(180,110,30,255))
        first,last=source(self.a,0,a),source(self.b,1,b)
        result=overlay(boolean(first,last,'union'),method='exact-output');product=assemble(result)
        resources={resource.path:resource for resource in product.shape.textures}
        self.assertEqual(len(resources),2)
        self.assertNotIn('same.png',resources)
        for owner,original in [(0,a),(1,b)]:
            identifiers={int(product.attributes.materials[i]) for i in range(len(result.supports)) if result.product.operands[i]==owner}
            self.assertTrue(identifiers)
            for identifier in identifiers:
                values=dict(dict(product.shape.geometry.material_defs)[identifier].attributes)
                for name in MAPS:
                    resource=resources[values[name]]
                    self.assertEqual(resource.sha256,original.sha256)
                    self.assertIs(resource.encoded,original.encoded)
        for child in product.shape.comp('f',alignment='noAlign'):
            self.assertEqual(child.textures,product.shape.textures)
            self.assertEqual(child.projections,first.projections)
        self.assertIs(first.textures[0],a);self.assertIs(last.textures[0],b)
        self.assertEqual(dict(first.material.attributes)['colormap'],'same.png')
        repeated=assemble(result)
        self.assertEqual(repeated.texture_origins,product.texture_origins)
        self.assertEqual(repeated.shape.geometry.material_defs,product.shape.geometry.material_defs)

    def test_nonconflicting_names_retain_prepared_objects(self):
        a=texture('a.png',(30,80,120,255));b=texture('b.png',(180,110,30,255))
        first,last=source(self.a,0,a),source(self.b,1,b)
        result=assemble(overlay(boolean(first,last,'union'),method='exact-output'))
        self.assertEqual(result.shape.textures,(a,b))
        self.assertIs(result.shape.textures[0],a);self.assertIs(result.shape.textures[1],b)

    def test_missing_prepared_map_is_explicitly_rejected(self):
        first=authored(self.a.shape(),0).set_material({'colormap':'unprepared.png'})
        result=overlay(boolean(first,authored(self.b.shape(),1),'union'),method='exact-output')
        with self.assertRaisesRegex(ValueError,'lacks a prepared resource'):assemble(result)

    def test_same_material_name_and_group_ids_keep_distinct_geometry_values(self):
        result=overlay(boolean(authored(self.a.shape(),0),authored(self.b.shape(),1),'union'),method='exact-output')
        product=assemble(result);verify(product,reference(self.a,self.b,'union'))
        self.assertEqual(len(product.shape.geometry.material_defs),2)
        self.assertEqual(len(product.shape.geometry.group_names),2)
        self.assertEqual(product.shape.material,Material())
        self.assertEqual(product.shape.scope,result.product.sources[0].shape.scope)

    def test_logical_roof_selection_feeds_existing_hip_operator_with_all_uvs(self):
        prism=Prism(shapely.box(0,0,12,8),0.,5.)
        result=assemble(overlay(boolean(authored(prism.shape(),0),authored(prism.shape(),1),'union'),method='exact-output'))
        self.assertEqual(len(result.shape.geometry.polygons),6)
        top=result.shape.comp('f',Branch(tagged('extrude.top')))[0]
        roof=top.roof_hip(1.5,'byHeight');mesh=roof.emit()
        _,roots=np.unique(mesh['vertices'],axis=0,return_inverse=True)
        expected_volume=1.5*(12*8-8*(12+8)/2+8*8/3)
        expected_surface=12*8*(1+np.sqrt(1+(1.5/4)**2))
        actual=measure(replace(result.overlay.product,mesh=mesh,roots=roots))
        np.testing.assert_allclose(actual,[expected_volume,expected_surface],atol=1e-7,rtol=1e-9)
        for channel in range(10):self.assertIn('uv'+str(channel) if channel else 'uv',mesh)
        self.assertEqual(roof.geometry.material_defs,top.geometry.material_defs)
        for face in roof.geometry.polygons:self.assertTrue({'bool.A','bool.B'}<=face.tags)

    def test_missing_triangle_and_overlaid_duplicate_shell_are_rejected(self):
        result=overlay(boolean(authored(self.a.shape(),0),authored(self.b.shape(),1),'union'),method='exact-output')
        product=result.product
        with self.assertRaises(ValueError):
            mesh=dict(product.mesh,faces=product.mesh['faces'][1:])
            broken=replace(product,mesh=mesh,operands=product.operands[1:],source_triangles=product.source_triangles[1:])
            assemble(replace(result,product=broken,supports=result.supports[1:]))
        with self.assertRaisesRegex(ValueError,'nonmanifold|duplicate directed'):
            mesh=dict(product.mesh,faces=np.tile(product.mesh['faces'],(2,1)))
            broken=replace(product,mesh=mesh,operands=np.tile(product.operands,2),source_triangles=np.tile(product.source_triangles,2))
            assemble(replace(result,product=broken,supports=result.supports*2))


if __name__=='__main__':unittest.main()
