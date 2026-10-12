"""Independent insertion dimensions, origins, materials, normal transforms and alias ownership."""

import argparse
from dataclasses import replace
import json
from pathlib import Path

import numpy as np

from evidence import revision
from modelling_insert import Asset,MODES
from modelling_insert_cases import block,frame
from modelling_faces import Face
from modelling_scope import Frame,Scope
from modelling_shape import Shape
from verify_modelling_extrude import near


def controls():
    source=block();asset=Asset.from_shape(source,'off-origin');mesh=source.emit();rows=[]
    near(asset.minimum,[-2.,3.,1.]);near(asset.geometry.reference_size,[4.,2.,8.])
    fixtures=[((8,6,12),(8,6,12),(0,0,0)),((8,6,0),(8,6,20),(0,0,0)),
              ((8,0,12),(8,3.5,12),(0,0,0)),((0,6,12),(9,6,12),(0,0,0)),
              ((0,6,0),(12,6,24),(-6,0,3)),((8,0,0),(8,4,16),(0,6,2)),
              ((0,0,12),(6,3,12),(-3,4.5,0)),((0,0,0),(4,2,8),(-2,3,1))]
    bases=[Scope(position=(10,20,30)),Scope(position=(10,20,30),orientation=(21.,-17.,39.)),
           Scope(position=(10,20,30),orientation=(21.,-17.,39.),pivot=Frame((1,2,3),(7,8,9)),object_frame=Frame((9,8,7),(12,13,14)))]
    for reference in bases:
        for size,expected,delta in fixtures:
            parent=replace(source,scope=replace(reference,size=size))
            for mode in MODES:
                child=parent.insert(asset,mode);dims=np.array(expected,dtype=float);shift=np.array(delta,dtype=float)
                if mode=='keepSizeAndPosition':dims=np.array([4.,2.,8.]);shift=np.array([-2.,3.,1.])
                elif mode=='keepSizeAlignPosition':dims=np.array([4.,2.,8.]);shift=(np.array(size)-dims)/2.;shift[1]=0.
                near(child.scope.size,dims);near(child.scope.origin,parent.scope.origin+parent.scope.axes@shift)
                expected_points=(mesh['vertices']-[-2.,3.,1.])/[4.,2.,8.]*dims
                expected_points=expected_points@parent.scope.axes.T+child.scope.origin
                emitted=child.emit();near(emitted['vertices'],expected_points)
                expected_normals=mesh['normals']/ (dims/[4.,2.,8.]);expected_normals=expected_normals@parent.scope.axes.T
                expected_normals/=np.linalg.norm(expected_normals,axis=1)[:,None];near(emitted['normals'],expected_normals)
                assert child.geometry is asset.geometry and child.is_instanced() and child.t(1,2,3).is_instanced()
                assert all(np.array_equal(emitted[key],mesh[key]) for key in ('faces','uv','parts','materials'))
                assert child.geometry.polygons==source.geometry.polygons
                assert child.group_names()==source.group_names() and child.material_names()==source.material_names()
                assert all(not array.flags.writeable for array in (child.geometry.unit_vertices,*child.geometry.fields.values()))
                rows.append(dict(mode=mode,size=size,adjusted_size=dims.tolist(),origin_delta=shift.tolist()))
    assert not parent.is_instanced() and not parent.insert(asset).comp('f')[0].is_instanced()
    return rows


def axis_controls():
    source=block();asset=Asset.from_shape(source,'z-up','zUp');parent=replace(source,scope=Scope(size=(0,0,0)))
    expected=source.emit()['vertices'][:,[0,2,1]]*np.array([1.,1.,-1.])
    near(parent.insert(asset).emit()['vertices'],expected)
    pane=Shape.from_faces([[0,0,7],[2,0,7],[2,3,7],[0,3,7]],[Face((0,1,2,3))])
    asset=Asset.from_shape(pane);child=replace(parent,scope=Scope(size=(8,6,0))).insert(asset)
    near(child.scope.size,[8,6,0]);near(child.emit()['vertices'].min(0),[0,0,0]);near(child.emit()['vertices'].max(0),[8,6,0])
    near(replace(parent,scope=Scope(size=(-8,6,0))).insert(Asset.from_shape(source)).scope.size,[-8,6,4])
    cancelled=replace(parent,scope=Scope(size=(-8,4,0))).insert(Asset.from_shape(source))
    near(cancelled.scope.size,[-8,4,0]);near(np.ptp(cancelled.emit()['vertices'],axis=0),[8,4,1e-5])
    renamed=cancelled.with_metadata({8:'renamed'});assert renamed.is_instanced()
    near(renamed.emit()['vertices'],cancelled.emit()['vertices'])
    return dict(z_up=True,planar_asset=True,signed_sizes=True,logical_zero_render_minimum_m=1e-5)


def invalid_controls():
    source=block();asset=Asset.from_shape(source)
    calls=[lambda:source.insert(source),lambda:source.insert(asset,'unknown'),lambda:source.insert(asset,cleanup='numericalCleanup'),
           lambda:Asset.from_shape(source,up_axis='xUp'),lambda:Asset.from_shape('file.obj'),
           lambda:Asset.from_shape(Shape.from_mesh(dict(vertices=np.empty((0,3)),faces=np.empty((0,3),dtype=int))))]
    for call in calls:
        try:call()
        except (ValueError,NotImplementedError):pass
        else:raise AssertionError('Invalid asset insertion accepted')
    return len(calls)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/insert-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',cases=controls(),axes=axis_controls(),invalid=invalid_controls(),
        limits='Prepared own geometry only; asset file readers, numerical cleanup, trim planes, complete material overrides and isolated PRT insert rules remain pending; no native GPU instance admission')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',cases=len(record['cases']),invalid=record['invalid'])))


if __name__=='__main__':main()
