import unittest

import numpy as np
import shapely

from outshine_ref.buildings.modelling import heightfield_envelope


def rectangle():
    return dict(vertices=np.array([[0.,0.,3.],[8.,0.,3.],[8.,6.,3.],[0.,6.,3.]]),
        faces=np.array([[0,1,2],[0,2,3]],dtype=np.int32))


def measures(envelope):
    mesh=envelope.mesh;vertices=mesh['vertices'];triangles=vertices[mesh['faces']]
    local=triangles-(vertices.min(axis=0)+vertices.max(axis=0))/2
    volume=np.einsum('ij,ij->i',local[:,0],np.cross(local[:,1],local[:,2])).sum()/6
    area=np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1).sum()/2
    edges={}
    for triangle in envelope.vertex_roots[mesh['faces']]:
        for a,b in zip(triangle,np.roll(triangle,-1)):
            key=tuple(sorted((int(a),int(b))));edges.setdefault(key,[]).append((int(a),int(b)))
    if any(len(pair)!=2 or pair[0]!=pair[1][::-1] for pair in edges.values()):
        raise AssertionError('Envelope is not a directed closed two-manifold')
    return float(volume),float(area)


class HeightfieldTests(unittest.TestCase):
    def test_flat_rectangle_has_exact_closed_volume_surface_and_domain(self):
        result=heightfield_envelope(rectangle(),0.,wall_material=12)
        self.assertEqual(measures(result),(144.,180.))
        self.assertEqual(result.volume_m3,144.)
        self.assertLess(shapely.from_wkb(result.footprint_wkb).symmetric_difference(shapely.box(0,0,8,6)).area,1e-12)
        self.assertEqual(len(result.boundary_edges),4)
        self.assertTrue(np.all(result.mesh['materials'][result.surface_roles==2]==12))

    def test_sloping_plane_integrates_height_without_a_source_interface_cap(self):
        mesh=rectangle();mesh['vertices'][:,2]+=.4*mesh['vertices'][:,1]
        result=heightfield_envelope(mesh,-1.)
        volume,area=measures(result)
        self.assertAlmostEqual(volume,249.6,places=10)
        self.assertAlmostEqual(result.volume_m3,volume,places=10)
        self.assertAlmostEqual(area,48*np.sqrt(1.16)+48+145.6,places=10)
        self.assertEqual(len(result.mesh['faces']),12)

    def test_ridge_sides_use_the_existing_boundary_split_at_the_peak(self):
        vertices=np.array([[0.,0.,3.],[8.,0.,3.],[8.,3.,5.],[0.,3.,5.],[8.,6.,3.],[0.,6.,3.]])
        result=heightfield_envelope(dict(vertices=vertices,faces=np.array([[0,1,2],[0,2,3],[3,2,4],[3,4,5]])),0.)
        volume,area=measures(result)
        self.assertAlmostEqual(volume,192.,places=10)
        self.assertAlmostEqual(area,48*np.sqrt(1+4/9)+48+96,places=10)
        self.assertEqual(len(result.boundary_edges),6)
        self.assertEqual(int(np.count_nonzero(result.surface_roles==0)),4)

    def test_courtyard_remains_open_in_a_plane_with_holes(self):
        polygon=shapely.Polygon([(0,0),(10,0),(10,10),(0,10)],[[(3,3),(3,7),(7,7),(7,3)]])
        triangles=[]
        for triangle in shapely.get_parts(shapely.constrained_delaunay_triangles(polygon)):
            xy=np.asarray(triangle.exterior.coords)[:3]
            if np.cross(np.r_[xy[1]-xy[0],0.],np.r_[xy[2]-xy[0],0.])[2]<0:xy=xy[::-1]
            triangles.append(np.c_[xy,2.+xy[:,0]*.2])
        points=np.asarray(triangles).reshape(-1,3)
        result=heightfield_envelope(dict(vertices=points,faces=np.arange(len(points)).reshape(-1,3)),0.)
        self.assertAlmostEqual(measures(result)[0],84*3.,places=10)
        self.assertLess(shapely.from_wkb(result.footprint_wkb).symmetric_difference(polygon).area,1e-12)
        self.assertFalse(shapely.from_wkb(result.footprint_wkb).covers(shapely.Point(5,5)))

    def test_upper_uv0_to_uv9_normals_and_face_metadata_are_retained(self):
        mesh=rectangle();mesh['materials']=np.array([5,9],dtype=np.uint16);mesh['groups']=np.array([2,4],dtype=np.uint32)
        mesh['parts']=np.array([7,8],dtype=np.int32);mesh['normals']=np.tile([0.,0.,2.],(4,1))
        for index in range(10):mesh['uv' if not index else 'uv'+str(index)]=np.arange(8,dtype=float).reshape(4,2)+index*10
        result=heightfield_envelope(mesh,0.);selected=result.surface_roles==0;corners=result.mesh['faces'][selected].reshape(-1)
        for key in ('normals','uv',*('uv'+str(i) for i in range(1,10))):
            np.testing.assert_array_equal(result.mesh[key][corners],mesh[key][mesh['faces']].reshape(-1,mesh[key].shape[1]))
        for key in ('parts','groups','materials'):np.testing.assert_array_equal(result.mesh[key][selected],mesh[key])
        np.testing.assert_array_equal(result.source_triangles[selected],np.arange(2))

    def test_point_contact_components_never_weld_vertices(self):
        a=rectangle();a['vertices'][:,:2]*=.5
        b=a['vertices']+np.array([4.,3.,0.]);mesh=dict(vertices=np.r_[a['vertices'],b],faces=np.r_[a['faces'],a['faces']+4])
        result=heightfield_envelope(mesh,0.);self.assertEqual(measures(result)[0],72.)
        point=np.all(result.mesh['vertices']==[4.,3.,3.],axis=1)
        self.assertEqual(len(np.unique(result.vertex_roots[point])),2)

    def test_large_translations_keep_shape_volume_and_closed_edges(self):
        for shift in ((512.,-256.,2.),(1e6,-2e6,100.)):
            mesh=rectangle();mesh['vertices']+=shift;result=heightfield_envelope(mesh,shift[2])
            self.assertEqual(measures(result),(144.,180.))
            self.assertEqual(result.projection_area_m2,48.)

    def test_reversed_vertical_overlapping_and_invalid_base_surfaces_are_rejected(self):
        mesh=rectangle()
        for bad in (mesh|dict(faces=mesh['faces'][:,::-1]),mesh|dict(vertices=mesh['vertices'][:,[0,2,1]]),
                    dict(vertices=np.r_[mesh['vertices'],mesh['vertices']+[0,0,1]],faces=np.r_[mesh['faces'],mesh['faces']+4])):
            with self.assertRaises(ValueError):heightfield_envelope(bad,-1.)
        for base in (3.,4.,float('nan')):
            with self.assertRaises(ValueError):heightfield_envelope(mesh,base)

    def test_unmatched_interior_t_junction_is_not_closed_with_an_internal_wall(self):
        points=np.array([[0.,0.,2.],[2.,0.,2.],[2.,4.,2.],[0.,4.,2.],[4.,0.,2.],[4.,2.,2.],[2.,2.,2.],[4.,4.,2.]])
        faces=np.array([[0,1,2],[0,2,3],[1,4,5],[1,5,6],[6,5,7],[6,7,2]])
        with self.assertRaisesRegex(ValueError,'interior edge|T junction'):
            heightfield_envelope(dict(vertices=points,faces=faces),0.)

    def test_owned_snapshot_is_immutable_and_invalid_channels_are_explicit(self):
        mesh=rectangle();result=heightfield_envelope(mesh,0.);mesh['vertices'][:]=999.
        self.assertEqual(result.volume_m3,144.)
        for array in result.mesh.values():
            with self.assertRaises(ValueError):array.setflags(write=True)
        for bad in (True,-1,1.5,65536):
            with self.assertRaises(ValueError):heightfield_envelope(rectangle(),0.,wall_material=bad)
        with self.assertRaises(NotImplementedError):heightfield_envelope(rectangle()|dict(colour=np.zeros((4,4))),0.)


if __name__=='__main__':unittest.main()
