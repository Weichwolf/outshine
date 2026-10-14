"""Actual geometry rejects sealed access/slabs and absent constructed stairs."""

import unittest

import manifold3d as manifold
import shapely

from boolean_building_bands import construct
from boolean_building_plan import sources
from building_vertical import build,level
from outshine_ref.buildings.modelling import PrismVolume,union_prisms
from structural_shell_geometry import assembly,native,mesh_section
from verify_structural_shell import geometry,access


class StructuralControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plan=build(dict(id='control',polygon=shapely.box(0,0,14,12),
            properties={'render_height':6.4,'render_min_height':0,'building:levels':2}))
        cls.shell=construct(cls.plan);_,cls.collision=assembly(cls.plan,cls.shell,frames=True)

    def test_complete_framed_control_retains_every_access_and_stair_connection(self):
        geometry(self.plan,sources(self.plan),self.shell);result=access(self.plan,self.collision)
        self.assertGreater(result['sampled_poses'],200);self.assertGreaterEqual(result['sampled_min_headroom_m'],1.90)

    def test_filled_stair_opening_is_rejected_by_actual_ray_clearance(self):
        low=self.plan['ceiling_z_m'];high=level(self.plan,1)['floor_z_m']
        cap=union_prisms([PrismVolume('bad-cap',self.plan['stair']['opening'],low,high)])
        sealed=manifold.Manifold.batch_boolean([self.collision,native(cap.mesh,cap.vertex_roots)],manifold.OpType.Add)
        with self.assertRaisesRegex(ValueError,'torso'):access(self.plan,sealed)

    def test_missing_stair_flights_are_rejected_by_actual_foot_support(self):
        _,missing=assembly(self.plan,self.shell,frames=True,omit_materials=(5,))
        with self.assertRaisesRegex(ValueError,'foot disk'):access(self.plan,missing)

    def test_sealed_entrance_is_rejected_by_actual_ray_clearance(self):
        door=self.plan['entrance'];cap=union_prisms([PrismVolume('bad-door',door['polygon'],self.plan['floor_z_m'],self.plan['floor_z_m']+door['height_m'])])
        sealed=manifold.Manifold.batch_boolean([self.collision,native(cap.mesh,cap.vertex_roots)],manifold.OpType.Add)
        with self.assertRaisesRegex(ValueError,'torso'):access(self.plan,sealed)

    def test_double_mesh_sections_preserve_an_island_inside_a_courtyard(self):
        shape=shapely.Polygon([(0,0),(10,0),(10,10),(0,10)],[[(2,2),(2,8),(8,8),(8,2)]])
        island=shapely.box(4,4,6,6)
        product=union_prisms([PrismVolume('ring',shape,0,3),PrismVolume('island',island,0,3)])
        section=mesh_section(product.mesh,product.vertex_roots,1.5)
        self.assertLess(section.symmetric_difference(shape.union(island)).area,1e-10)


if __name__=='__main__':unittest.main()
