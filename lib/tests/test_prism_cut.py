from dataclasses import replace
import unittest

import numpy as np
import shapely

from outshine_ref.buildings.modelling import heightfield_envelope,PrismVolume,subtract_prisms


def box():
    vertices=np.array([[0.,0.,5.],[10.,0.,5.],[10.,6.,5.],[0.,6.,5.]])
    return heightfield_envelope(dict(vertices=vertices,faces=np.array([[0,1,2],[0,2,3]]),materials=np.array([5,5],dtype=np.uint16)),0.,wall_material=5,base_material=5)


def measures(product):
    mesh=product.mesh;triangles=mesh['vertices'][mesh['faces']]
    shift=triangles.astype(np.longdouble)-triangles.mean(axis=(0,1)).astype(np.longdouble)
    volume=float((shift[:,0]*np.cross(shift[:,1],shift[:,2])).sum()/6)
    area=float(np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1).sum()/2)
    edges={}
    for triangle in product.vertex_roots[mesh['faces']]:
        for a,b in zip(triangle,np.roll(triangle,-1)):
            edges.setdefault(tuple(sorted((int(a),int(b)))),[]).append((int(a),int(b)))
    if any(len(e)!=2 or e[0]!=e[1][::-1] for e in edges.values()):raise AssertionError('Cut boundary is not directed closed')
    return volume,area


class PrismCuts(unittest.TestCase):
    def test_internal_room_keeps_negative_cavity_volume_and_source_materials(self):
        product=subtract_prisms('outer',box(),[PrismVolume('room',shapely.box(1,1,9,5),.2,4.8,17)])
        volume,area=measures(product)
        self.assertAlmostEqual(volume,152.8,places=9);self.assertAlmostEqual(area,454.4,places=9)
        np.testing.assert_allclose([volume,area],[product.volume_m3,product.area_m2],atol=1e-9)
        self.assertEqual(product.source_ids,('outer','room'))
        np.testing.assert_array_equal(product.mesh['materials'],np.array([5,17])[product.source_indices])

    def test_overlapping_cutters_use_their_union_without_repeated_void_volume(self):
        cut=[PrismVolume('a',shapely.box(1,1,7,5),1,4,8),PrismVolume('b',shapely.box(5,1,9,5),1,4,9)]
        product=subtract_prisms('outer',box(),cut)
        self.assertAlmostEqual(product.volume_m3,204.,places=9);measures(product)
        self.assertEqual(product.source_ids,('outer','a','b'))
        np.testing.assert_array_equal(product.mesh['materials'],np.array([5,8,9])[product.source_indices])

    def test_outer_uv0_to_uv9_are_interpolated_from_original_triangles(self):
        source=box();mesh=dict(source.mesh);points=mesh['vertices']
        for i in range(10):mesh['uv' if not i else 'uv'+str(i)]=np.c_[points[:,0]*.2+points[:,2]*.3+i,points[:,1]*.4-points[:,2]*.1-i]
        source=replace(source,mesh=mesh);product=subtract_prisms('outer',source,[PrismVolume('through',shapely.box(4,2,6,4),-1,6,9)])
        faces=product.mesh['faces'];outer=product.source_indices==0;selected=faces[outer].reshape(-1)
        p=product.mesh['vertices'][selected]
        for i in range(10):
            expected=np.c_[p[:,0]*.2+p[:,2]*.3+i,p[:,1]*.4-p[:,2]*.1-i]
            np.testing.assert_allclose(product.mesh['uv' if not i else 'uv'+str(i)][selected],expected,atol=1e-12)
            np.testing.assert_array_equal(product.mesh['uv' if not i else 'uv'+str(i)][faces[~outer].reshape(-1)],0.)
        measures(product)

    def test_large_translations_preserve_geometry_and_complete_material_origins(self):
        original=box()
        for shift in ((512.,-256.,0.),(1e6,-2e6,0.)):
            moved=replace(original,mesh=dict(original.mesh,vertices=original.mesh['vertices']+shift))
            polygon=shapely.affinity.translate(shapely.box(1,1,9,5),*shift[:2])
            product=subtract_prisms('outer',moved,[PrismVolume('room',polygon,.2,4.8,17)])
            np.testing.assert_allclose(measures(product),[152.8,454.4],atol=1e-7,rtol=0.)
            self.assertEqual(product.source_ids,('outer','room'))

    def test_no_cutters_empty_prisms_and_complete_erasure_are_explicit(self):
        source=box();same=subtract_prisms('outer',source,[])
        self.assertAlmostEqual(same.volume_m3,300.);self.assertEqual(same.source_ids,('outer',));measures(same)
        empty=subtract_prisms('outer',source,[PrismVolume('empty',shapely.Polygon(),0,1)])
        self.assertEqual(empty.omissions,(('empty','empty_polygon'),));self.assertAlmostEqual(empty.volume_m3,300.)
        gone=subtract_prisms('outer',source,[PrismVolume('all',shapely.box(-1,-1,11,7),-1,6)])
        self.assertEqual(gone.mesh['faces'].shape,(0,3));self.assertEqual(gone.volume_m3,0.)

    def test_replay_and_output_ownership_ignore_backend_identifier_allocation(self):
        import manifold3d as manifold
        source=box();cuts=[PrismVolume('door',shapely.box(4,-1,6,1),.2,2.4,1)]
        a=subtract_prisms('outer',source,cuts);manifold.Manifold.reserve_ids(291);b=subtract_prisms('outer',source,cuts)
        for key in a.mesh:np.testing.assert_array_equal(a.mesh[key],b.mesh[key])
        for key in ('vertex_roots','source_indices','source_triangles'):np.testing.assert_array_equal(getattr(a,key),getattr(b,key))
        for value in [*a.mesh.values(),a.vertex_roots,a.source_indices,a.source_triangles]:
            with self.assertRaises(ValueError):value.setflags(write=True)

    def test_invalid_roots_identity_channels_and_capacities_reject(self):
        source=box();cut=PrismVolume('same',shapely.box(1,1,2,2),1,2)
        with self.assertRaises(ValueError):subtract_prisms('same',source,[cut])
        with self.assertRaises(ValueError):subtract_prisms('outer',source,[cut,cut])
        with self.assertRaises(ValueError):subtract_prisms('',source,[])
        with self.assertRaises(ValueError):subtract_prisms('outer',None,[])
        for points in (np.empty((0,3)),np.zeros((4,2)),np.full((4,3),np.nan)):
            with self.assertRaisesRegex(ValueError,'finite ENU vertices'):
                subtract_prisms('outer',replace(source,mesh=dict(source.mesh,vertices=points)),[])
        bad=replace(source,vertex_roots=np.zeros(len(source.mesh['vertices']),dtype=np.int64))
        with self.assertRaisesRegex(ValueError,'joins different positions'):subtract_prisms('outer',bad,[])
        with self.assertRaises(NotImplementedError):subtract_prisms('outer',replace(source,mesh=dict(source.mesh,colour=np.zeros((1,4)))),[])
        for capacity in (True,0,1):
            with self.assertRaises(ValueError):subtract_prisms('outer',source,[cut],max_triangles=capacity)


if __name__=='__main__':unittest.main()
