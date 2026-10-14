import unittest

import manifold3d as manifold
import numpy as np
import shapely

from outshine_ref.buildings.modelling import PrismVolume,union_prisms


def closed(product):
    triangles=product.vertex_roots[product.mesh['faces']]
    edges=np.concatenate([triangles[:,[0,1]],triangles[:,[1,2]],triangles[:,[2,0]]])
    _,inverse,counts=np.unique(np.sort(edges,axis=1),axis=0,return_inverse=True,return_counts=True)
    np.testing.assert_array_equal(counts,2)
    np.testing.assert_array_equal(np.bincount(inverse,weights=np.where(edges[:,0]<edges[:,1],1,-1)),0)
    points=product.mesh['vertices'][product.mesh['faces']]
    if not len(points):return
    shifted=points.astype(np.longdouble)-points.mean(axis=(0,1)).astype(np.longdouble)
    volume=float((shifted[:,0]*np.cross(shifted[:,1],shifted[:,2])).sum()/6)
    area=float(np.linalg.norm(np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0]),axis=1).sum()/2)
    np.testing.assert_allclose([volume,area],[product.volume_m3,product.area_m2],atol=1e-10,rtol=1e-10)


class PrismUnions(unittest.TestCase):
    def test_two_joined_slabs_remove_the_complete_internal_cap(self):
        shape=shapely.box(0,0,3,4)
        product=union_prisms([PrismVolume('lower',shape,0,2,7),PrismVolume('upper',shape,2,5,9)])
        self.assertAlmostEqual(product.volume_m3,60.);self.assertAlmostEqual(product.area_m2,94.)
        closed(product);self.assertEqual(product.source_ids,('lower','upper'))
        np.testing.assert_array_equal(product.mesh['materials'],np.array([7,9])[product.source_indices])

    def test_courtyard_volume_and_surface_keep_the_hole(self):
        shape=shapely.Polygon([(0,0),(12,0),(12,8),(0,8)],[[(3,2),(3,6),(9,6),(9,2)]])
        product=union_prisms([PrismVolume('courtyard',shape,-2,5)])
        self.assertAlmostEqual(product.volume_m3,shape.area*7)
        self.assertAlmostEqual(product.area_m2,2*shape.area+shape.length*7);closed(product)

    def test_disjoint_components_and_edge_contact_keep_native_topology(self):
        for shape in (shapely.box(3,4,6,8),shapely.box(6,5,9,9)):
            product=union_prisms([PrismVolume('a',shapely.box(0,0,3,4),0,5),PrismVolume('b',shape,0,5)])
            self.assertAlmostEqual(product.volume_m3,120.)
            self.assertAlmostEqual(product.area_m2,188.);closed(product)

    def test_multipolygon_and_empty_source_keep_complete_declared_id_table(self):
        shape=shapely.MultiPolygon([shapely.box(0,0,2,3),shapely.box(6,0,8,3)])
        product=union_prisms([PrismVolume('empty',shapely.Polygon(),0,1),PrismVolume('parts',shape,0,2)])
        self.assertAlmostEqual(product.volume_m3,24.);self.assertAlmostEqual(product.area_m2,64.)
        self.assertEqual(product.omissions,(('empty','empty_polygon'),));self.assertEqual(product.source_ids,('empty','parts'))
        np.testing.assert_array_equal(product.source_indices,1);closed(product)

    def test_output_arrays_are_immutable_and_library_ids_do_not_affect_replay(self):
        sources=[PrismVolume('floor',shapely.box(0,0,3,4),0,.2),PrismVolume('wall',shapely.box(0,0,.2,4),.2,3)]
        a=union_prisms(sources);manifold.Manifold.reserve_ids(137);b=union_prisms(sources)
        for name in a.mesh:np.testing.assert_array_equal(a.mesh[name],b.mesh[name])
        for name in ('vertex_roots','source_indices','source_triangles'):np.testing.assert_array_equal(getattr(a,name),getattr(b,name))
        for values in [*a.mesh.values(),a.vertex_roots,a.source_indices,a.source_triangles]:
            self.assertFalse(values.flags.writeable)
            with self.assertRaises(ValueError):values.setflags(write=True)

    def test_invalid_shapes_heights_and_materials_fail_before_generation(self):
        for shape in (None,shapely.LineString([(0,0),(1,1)]),shapely.Polygon([(0,0,1),(1,0,1),(0,1,1)])):
            with self.assertRaises(ValueError):PrismVolume('invalid',shape,0,1)
        for low,high in ((0,0),(1,0),(False,1),(0,np.inf)):
            with self.assertRaises(ValueError):PrismVolume('invalid',shapely.box(0,0,1,1),low,high)
        for material in (-1,65536,True,2.5):
            with self.assertRaises(ValueError):PrismVolume('invalid',shapely.box(0,0,1,1),0,1,material)

    def test_duplicate_ids_and_triangle_capacity_are_explicit_failures(self):
        source=PrismVolume('same',shapely.box(0,0,1,1),0,1)
        with self.assertRaisesRegex(ValueError,'Duplicate'):union_prisms([source,source])
        with self.assertRaisesRegex(ValueError,'input triangle capacity'):union_prisms([source],max_triangles=11)
        for capacity in (True,0,-1):
            with self.assertRaises(ValueError):union_prisms([source],max_triangles=capacity)

    def test_overlapping_sources_generate_new_faces_and_obey_output_capacity(self):
        sources=[PrismVolume('a',shapely.box(0,0,3,3),0,3),PrismVolume('b',shapely.box(1,1,4,4),1,4)]
        product=union_prisms(sources)
        self.assertAlmostEqual(product.volume_m3,46.);self.assertAlmostEqual(product.area_m2,84.);closed(product)
        with self.assertRaisesRegex(ValueError,'output triangle capacity'):union_prisms(sources,max_triangles=24)

    def test_empty_union_is_a_valid_empty_boundary(self):
        product=union_prisms([])
        self.assertEqual(product.mesh['vertices'].shape,(0,3));self.assertEqual(product.mesh['faces'].shape,(0,3))
        self.assertEqual((product.volume_m3,product.area_m2),(0,0));closed(product)


if __name__=='__main__':unittest.main()
