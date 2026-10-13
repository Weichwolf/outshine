"""Check metric scope-XY projection capture, split continuity and channel ownership."""

import argparse
from dataclasses import replace
import json
from pathlib import Path

import numpy as np
import shapely

from evidence import revision
from outshine_ref.buildings.modelling.insert import Asset
from outshine_ref.buildings.modelling.scope import Relative,Frame
from outshine_ref.buildings.modelling.shape import Shape
from outshine_ref.buildings.modelling.split_sizes import Part,Floating
from outshine_ref.buildings.modelling.projection import dimension,resolved_offset
from modelling_uv_cases import adjacent


def assert_uv(shape,width=2.,height=1.5,offset=(0.,0.)):
    emitted=shape.project_uv(0).emit()
    np.testing.assert_allclose(emitted['uv'],emitted['vertices'][:,:2]/[width,height]-offset,atol=1e-12)


def controls():
    body=Shape.from_polygon(shapely.box(0.,0.,6.,4.)).extrude(3.)
    setup=body.setup_projection(0,'scope.xy',2.,1.5)
    assert setup.geometry is body.geometry
    assert_uv(setup)
    for child in setup.split('x',Part(2.),Part(4.)):
        assert child.projections==setup.projections;assert_uv(child)
        for face in child.comp('f'):assert_uv(face)
    assert_uv(setup.t(1.,2.,0.))
    projected=setup.project_uv(0)
    np.testing.assert_array_equal(projected.t(1.,2.,0.).emit()['uv'],projected.emit()['uv'])
    negative=body.setup_projection(0,'scope.xy',-2.,1.5,.5,.75);assert_uv(negative,-2.,1.5,(-.25,.5))
    relative=body.setup_projection(0,'scope.xy',Relative(.5),Relative(.5));assert_uv(relative,3.,1.5)
    assert_uv(body.setup_projection(0,'scope.xy',Floating(2.4),Floating(2.)),3.,1.5)
    for extent,requested,expected in ((6.,2.4,3.),(7.,2.,1.75),(6.,20.,6.),(6.,0.,6.),
                                      (0.,2.,1e-8),(-6.,2.,2.),(-6.,0.,-6.),(1e308,1e-24,1e-8)):
        assert dimension(Floating(requested),extent)==expected
    rotated=body.r(0.,0.,90.).setup_projection(0,'scope.xy',2.,1.5).project_uv(0).emit()
    np.testing.assert_allclose(rotated['uv'],np.column_stack((rotated['vertices'][:,1]/2.,-rotated['vertices'][:,0]/1.5)),atol=1e-12)
    depth=body.setup_projection(0,'scope.xy',2.,1.5,uw_factor=.5).project_uv(0).emit()
    np.testing.assert_allclose(body.scope.origin,[0.,0.,-4.],atol=1e-12)
    np.testing.assert_allclose(depth['uv'][:,0],(depth['vertices'][:,0]+.5*(depth['vertices'][:,2]+4.))/2.,atol=1e-12)
    wrapped=body.setup_projection(0,'scope.xy',2.,1.5,2000.5,1500.75);assert_uv(wrapped,offset=(.25,.5))
    other=setup.setup_projection(9,'scope.xy',3.,1.).project_uv(9)
    assert len(other.projections)==2 and 'uv9' in other.geometry.fields
    inserted=setup.insert(Asset.from_shape(body));assert_uv(inserted)
    for face in setup.comp('f'):
        assert_uv(face.extrude(.25))
    sheet=adjacent().setup_projection(0,'scope.xy',2.,1.5)
    for piece in sheet.split_area('x',Part(Relative(.5)),Part(Relative(.5))):assert_uv(piece)
    shrunk=setup.insert(Asset.from_shape(body)).s(0.,0.,0.)
    assert_uv(shrunk)
    before=shrunk.emit()['vertices']
    for altered in (shrunk.project_uv(0),shrunk.scale_uv(0,2.,3.),shrunk.copy_uv(0,9),shrunk.delete_uv(0)):
        np.testing.assert_array_equal(altered.emit()['vertices'],before)
    for face in projected.geometry.polygons:
        for ids,pairs in zip((face.outer,*face.holes),face.uvs,strict=True):
            np.testing.assert_allclose(pairs,projected.emit()['uv'][list(ids)],atol=1e-12)
    for call in (lambda:body.project_uv(0),lambda:body.setup_projection(10,'scope.xy',1.,1.),
                 lambda:body.setup_projection(0,'world.xx',1.,1.),lambda:body.setup_projection(0,None,1.,1.)):
        try:call()
        except (ValueError,NotImplementedError):pass
        else:raise AssertionError('Missing or unsupported projection accepted')
    world_controls(body)
    offset_controls(body)
    selector_controls(body)
    return dict(world_frames_and_shared_phase=True,relative_floating_offsets=True,signed_floating_dimensions=True,six_scope_selectors=True,metric_uv=True,split_component_continuity=True,captured_frame=True,baked_uv_moves_with_geometry=True,
        signed_relative_sizes=True,floating_rounding_and_limits=True,rotated_frame_and_depth=True,periodic_offsets=True,
        channel_independence=True,insert_inheritance=True,extrude_and_area_split=True,instance_minimum_size=True,logical_corners=True)


def world_controls(body):
    transformed=replace(body,scope=replace(body.scope,position=(3.,5.,-2.),orientation=(17.,29.,-41.),
        object_frame=Frame((123.,-11.,57.),(9.,-25.,37.))))
    for name,indices,sign in (('xy',(0,1,2),1),('xz',(0,2,1),1),('yx',(1,0,2),-1),
                              ('yz',(1,2,0),1),('zx',(2,0,1),1),('zy',(2,1,0),-1)):
        for source in (body,transformed):
            setup=source.setup_projection(0,'world.'+name,2.,3.,.4,.9,.25)
            for child in (setup,setup.t(7.,-3.,2.),*setup.comp('f')):
                result=child.project_uv(0).emit();coords=result['vertices'][:,indices]
                expected=np.column_stack(((coords[:,0]+sign*.25*coords[:,2]-.4)/2.,(coords[:,1]-.9)/3.))
                np.testing.assert_allclose(result['uv'],expected,atol=2e-12)
            assert setup.projections==body.setup_projection(0,'world.'+name,2.,3.,.4,.9,.25).projections
            relative=source.setup_projection(0,'world.'+name,Relative(.5),Relative(.25)).project_uv(0).emit()
            sizes=np.asarray(source.scope.size)[list(indices[:2])]*[.5,.25]
            np.testing.assert_allclose(relative['uv'],relative['vertices'][:,indices[:2]]/sizes,atol=2e-12)


def offset_controls(body):
    assert dimension(Floating(-2.4),6.)==3.
    assert dimension(Floating(-2.4),-6.)==3.
    assert resolved_offset(Relative(-.25),2.,6.)==-1.5
    assert resolved_offset(Floating(.6),Floating(2.4),6.)==.75
    assert resolved_offset(Floating(.6),Floating(-2.4),6.)==-.75
    assert resolved_offset(Floating(-.6),Floating(-2.4),6.)==.75
    assert resolved_offset(Floating(.6),2.4,6.)==.6
    assert resolved_offset(Floating(.6),Floating(0.),6.)==.6
    assert resolved_offset(Floating(.6),Floating(2.4),0.)==0.
    assert_uv(body.setup_projection(0,'scope.xy',Floating(-2.4),1.5,Floating(.6),Relative(.25)),3.,1.5,(-.25,.5))
    assert_uv(body.setup_projection(0,'scope.xy',2.,1.5,Floating(.6),Floating(.3)),2.,1.5,(.3,.2))


def selector_controls(body):
    expected={'xy':((1,0,0),(0,1,0),(0,0,1)),
              'xz':((1,0,0),(0,0,1),(0,1,0)),
              'yx':((0,1,0),(1,0,0),(0,0,-1)),
              'yz':((0,1,0),(0,0,1),(1,0,0)),
              'zx':((0,0,1),(1,0,0),(0,1,0)),
              'zy':((0,0,1),(0,1,0),(-1,0,0))}
    for name,rows in expected.items():
        frame=np.array(rows,dtype=float)
        for source in (body,body.r(17.,-29.,41.).t(3.,-2.,7.)):
            configured=source.setup_projection(0,'scope.'+name,2.,3.,.4,.9,.25)
            for child in (configured,*configured.comp('f')):
                result=child.project_uv(0).emit()
                local=(result['vertices']-source.scope.origin)@source.scope.axes
                oriented=local@frame.T
                uv=np.column_stack(((oriented[:,0]+.25*oriented[:,2]-.4)/2.,(oriented[:,1]-.9)/3.))
                np.testing.assert_allclose(result['uv'],uv,atol=2e-12)
            relative=source.setup_projection(0,'scope.'+name,Relative(.5),Relative(.25)).project_uv(0).emit()
            local=(relative['vertices']-source.scope.origin)@source.scope.axes
            sizes=np.abs(frame[:2])@source.scope.size
            np.testing.assert_allclose(relative['uv'],(local@frame[:2].T)/(sizes*[.5,.25]),atol=2e-12)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/projection-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',controls=controls(),
        limits='Twelve scope/world selectors with analytical controls and static SDK matrix evidence; large-coordinate SDK origin rebasing, default setup and isolated PRT equivalence remain open')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(record['controls']))
