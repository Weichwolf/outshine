from dataclasses import replace
import unittest

import numpy as np
import shapely

from outshine_ref.buildings.modelling import Face,Frame,Shape
from outshine_ref.buildings.modelling.faces import plane
from test_roof_hip import volume


def closed(shape):
    mesh=shape.emit();_,ids=np.unique(mesh['vertices'],axis=0,return_inverse=True)
    faces=ids[mesh['faces']]
    edges=np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]])
    _,inverse,counts=np.unique(np.sort(edges,axis=1),axis=0,return_inverse=True,return_counts=True)
    np.testing.assert_array_equal(counts,2)
    np.testing.assert_array_equal(np.bincount(inverse,weights=np.where(edges[:,0]<edges[:,1],1,-1)),0)


class ExteriorHip(unittest.TestCase):
    def test_shell_depth_changes_volume_by_exact_domain_area(self):
        source=Shape.from_polygon(shapely.box(0,0,12,5))
        thin=source.roof_hip(3.,'byHeight',overhang=.5,shell_depth_m=.1)
        thick=source.roof_hip(3.,'byHeight',overhang=.5,shell_depth_m=.4)
        d=.5/np.hypot(1.,1.2)
        self.assertAlmostEqual(thin.roof_info.exteriors[0].domain.area,(12+2*d)*(5+2*d),places=7)
        self.assertAlmostEqual(volume(thick)-volume(thin),.3*(12+2*d)*(5+2*d),places=7)
        closed(thin);closed(thick)

    def test_rotated_courtyard_retains_closed_topology_and_metadata(self):
        polygon=shapely.Polygon([(0,0),(12,0),(12,8),(0,8)],[[(2,2),(2,6),(7,6),(7,2)]])
        source=Shape.from_polygon(polygon).r(13.,24.,-7.).color(.2,.3,.4)
        before=source.emit()['vertices'].copy()
        for even in (False,True):
            roof=source.roof_hip(3.,'byHeight',overhang=.5,even=even)
            closed(roof);self.assertGreater(volume(roof),0.)
            self.assertTrue(any('roof.side.inner' in f.tags for f in roof.geometry.polygons))
            self.assertEqual(roof.material,source.material)
            normal=plane(source.emit()['vertices'],source.geometry.polygons[0])[0]
            np.testing.assert_allclose(roof.scope.axes[:,1],normal,atol=1e-10)
            self.assertEqual(len(roof.roof_info.exteriors),1)
        np.testing.assert_array_equal(source.emit()['vertices'],before)

    def test_object_frame_keeps_intrinsic_products_identical(self):
        source=Shape.from_polygon(shapely.box(0,0,12,5)).r(17.,31.,23.)
        distant=replace(source,scope=replace(source.scope,object_frame=Frame((1e8,200.,-300.),(13.,17.,23.))))
        near=source.roof_hip(3.,'byHeight',overhang=.5)
        far=distant.roof_hip(3.,'byHeight',overhang=.5)
        local=replace(far,scope=replace(far.scope,object_frame=Frame()))
        np.testing.assert_array_equal(near.emit()['vertices'],local.emit()['vertices'])
        self.assertEqual(far.scope.object_frame,distant.scope.object_frame)

    def test_multiple_source_planes_and_absent_uv(self):
        source=Shape.from_faces(np.array([[0,0,0],[0,0,-5],[12,0,-5],[12,0,0],
            [20,0,0],[20,5,0],[32,5,0],[32,0,0]],dtype=float),
            [Face((0,1,2,3),material=7,group=2),Face((4,5,6,7),material=9,group=3)]).delete_uv(0)
        result=source.roof_hip(3.,'byHeight',overhang=.5)
        closed(result);self.assertNotIn('uv',result.emit())
        self.assertEqual(set(result.roof_info.source_faces),{0,1})
        for face,owner in zip(result.geometry.polygons,result.roof_info.source_faces,strict=True):
            self.assertEqual((face.material,face.group),[(7,2),(9,3)][owner])
