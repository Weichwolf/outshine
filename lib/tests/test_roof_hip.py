from dataclasses import replace
import unittest

import numpy as np
import shapely

from outshine_ref.buildings.modelling import Branch, Face, Frame, Shape, SkeletonBudgetExceeded, SkeletonLimits
from outshine_ref.buildings.modelling.metadata import Material
from outshine_ref.buildings.modelling.selectors import tagged


def volume(shape):
    mesh=shape.emit(); corners=mesh['vertices'][mesh['faces']]
    corners=corners-corners.mean(axis=(0,1))
    return np.einsum('ij,ij->i',corners[:,0],np.cross(corners[:,1],corners[:,2])).sum()/6


class HipModelling(unittest.TestCase):
    def test_rectangle_volume_angles_and_closed_oriented_edges(self):
        original=Shape.from_polygon(shapely.box(0,0,12,5))
        for value,mode in ((3.,'byHeight'),(25.,'byAngle')):
            roof=original.roof_hip(value,mode)
            rise=value if mode=='byHeight' else np.tan(np.deg2rad(value))*2.5
            self.assertAlmostEqual(volume(roof),rise*(30-25/6))
            mesh=roof.emit(); _,ids=np.unique(mesh['vertices'],axis=0,return_inverse=True)
            faces=ids[mesh['faces']];edges=np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]])
            _,inverse,counts=np.unique(np.sort(edges,axis=1),axis=0,return_inverse=True,return_counts=True)
            self.assertTrue(np.all(counts==2))
            self.assertTrue(np.all(np.bincount(inverse,weights=np.where(edges[:,0]<edges[:,1],1,-1))==0))
            self.assertEqual(len(roof.comp('f',Branch(tagged('roof.top')))),4)
            self.assertEqual(len(roof.comp('f',Branch(tagged('roof.bottom')))),1)

    def test_multiple_planes_keep_appearance_and_source_face_ownership(self):
        vertices=np.array([[0,0,0],[0,0,-5],[12,0,-5],[12,0,0],
                           [20,0,0],[20,5,0],[32,5,0],[32,0,0]],dtype=float)
        original=Shape.from_faces(vertices,[Face((0,1,2,3),material=7,tags=frozenset({'first'}),group=2),
                                            Face((4,5,6,7),material=9,tags=frozenset({'second'}),group=3)])
        original=original.with_metadata(group_names={2:'one',3:'two'},material_defs={7:Material('slate'),9:Material('copper')}).color(.2,.3,.4)
        roof=original.roof_hip(3.,'byHeight')
        self.assertAlmostEqual(volume(roof),155.)
        self.assertEqual(set(roof.roof_info.source_faces),{0,1})
        self.assertEqual(roof.geometry.material_defs,original.geometry.material_defs)
        self.assertEqual(roof.geometry.group_names,original.geometry.group_names)
        self.assertEqual(roof.material,original.material)
        for index,face in enumerate(roof.geometry.polygons):
            parent=original.geometry.polygons[roof.roof_info.source_faces[index]]
            self.assertEqual((face.material,face.group),(parent.material,parent.group))
            self.assertTrue(parent.tags<=face.tags)

    def test_nested_object_frame_does_not_enter_the_roof_solver(self):
        source=Shape.from_polygon(shapely.box(0,0,12,5)).r(17,31,23)
        distant=replace(source,scope=replace(source.scope,object_frame=Frame((1e8,200,-300),(13,17,23))))
        near=source.roof_hip(3.,'byHeight');far=distant.roof_hip(3.,'byHeight')
        local=replace(far,scope=replace(far.scope,object_frame=Frame()))
        np.testing.assert_allclose(near.emit()['vertices'],local.emit()['vertices'],atol=1e-12)
        self.assertEqual(far.scope.object_frame,distant.scope.object_frame)

    def test_source_normal_stream_is_rebuilt_and_absent_uv_stays_absent(self):
        source=Shape.from_polygon(shapely.box(0,0,12,5));mesh=source.emit()
        mesh['normals']=np.tile([0.,1.,0.],(len(mesh['vertices']),1))
        authored=Shape.from_mesh(mesh,source.scope,source.geometry.polygons).delete_uv(0)
        roof=authored.roof_hip(3.,'byHeight')
        self.assertTrue(roof.roof_info.authored_normals_rebuilt)
        self.assertNotIn('normals',roof.emit());self.assertNotIn('uv',roof.emit())

    def test_discontinuous_native_uv_seams_survive_component_selection(self):
        source=Shape.from_polygon(shapely.box(0,0,12,5));mesh=source.emit()
        mesh['uv']=mesh['uv'].copy();mesh['uv'][mesh['faces'][0]]=[7.,3.]
        source=Shape.from_mesh(mesh,source.scope,source.geometry.polygons)
        roof=source.roof_hip(3.,'byHeight')
        selected=roof.comp('f',Branch(tagged('roof.top'),'='),alignment='noAlign')[0].emit()
        active=selected['uv'][np.unique(selected['faces'])]
        self.assertTrue(np.any(np.all(np.isclose(active,[7.,3.]),axis=1)))
        self.assertTrue(np.any(np.all(np.isclose(active,[0.,0.]),axis=1)))
        np.testing.assert_array_equal(source.emit()['uv'],mesh['uv'])

    def test_budget_and_pending_options_are_explicit(self):
        source=Shape.from_polygon(shapely.box(0,0,12,5))
        for options in ({'max_cells':1},{'limits':SkeletonLimits(seconds=1e-12)},{'limits':SkeletonLimits(vertices=3)}):
            with self.assertRaises(SkeletonBudgetExceeded):source.roof_hip(3.,'byHeight',**options)
        for options in ({'overhang':.5},{'even':True}):
            with self.assertRaises(NotImplementedError):source.roof_hip(3.,'byHeight',**options)
        for value,mode in ((0.,'byHeight'),(-1.,'byHeight'),(90.,'byAngle'),(float('nan'),'byAngle'),(True,'byAngle')):
            with self.assertRaises(ValueError):source.roof_hip(value,mode)
        for options in ({'even':1},{'max_cells':False},{'max_cells':0}):
            with self.assertRaises(ValueError):source.roof_hip(3.,'byHeight',**options)


if __name__=='__main__':unittest.main()
