"""Closed Cartesian cuts: source UVs/appearance, winding, holes, scopes and coplanar contacts."""

import argparse
from dataclasses import replace
import json
from pathlib import Path

import numpy as np
import shapely

from evidence import revision
from modelling_extrude_probe import fixtures
from modelling_scope import Frame,Relative
from modelling_shape import Shape
from modelling_faces import Face
from modelling_split_sizes import Part,Floating
from verify_modelling_extrude import closed,volume
from verify_modelling_groups import cube


def near(a,b,tolerance=1e-7):
    if not np.allclose(a,b,rtol=1e-10,atol=tolerance):raise AssertionError((a,b))


def barycentric(point,triangle):
    basis=(triangle[1:]-triangle[0]).T;weights=np.linalg.lstsq(basis,point-triangle[0],rcond=None)[0]
    bary=np.r_[1-weights.sum(),weights]
    return bary,np.linalg.norm(basis@weights-(point-triangle[0]))


def source_attributes(parent,child):
    before,after=parent.emit(),child.emit();native_parts=before['parts'];points=before['vertices'][before['faces']]
    checked=0
    for row,triangle in enumerate(after['faces']):
        index=child.split_info.source_faces[after['parts'][row]]
        face=child.geometry.polygons[after['parts'][row]]
        if index is None:
            assert face.tags==frozenset({'split.cut'});continue
        original=parent.geometry.polygons[index]
        assert (face.material,face.group,face.tags)==(original.material,original.group,original.tags)
        candidates=np.flatnonzero(native_parts==index)
        for vertex in triangle:
            for source_row in candidates:
                bary,error=barycentric(after['vertices'][vertex],points[source_row])
                if error<1e-7 and bary.min()>-1e-7:
                    near(after['uv'][vertex],bary@before['uv'][before['faces'][source_row]])
                    if 'normals' in before:
                        normal=bary@before['normals'][before['faces'][source_row]];normal/=np.linalg.norm(normal)
                        near(after['normals'][vertex],normal)
                    checked+=1;break
            else:raise AssertionError('Cut surface vertex is not on its declared source face')
    assert child.geometry.group_names is parent.geometry.group_names and child.geometry.material_defs is parent.geometry.material_defs
    for array in [child.geometry.unit_vertices,*child.geometry.fields.values()]:assert not array.flags.writeable
    return checked


def prism_controls():
    rows=[];pattern=[Part(Floating(1.),str(i)) for i in range(4)]
    for label,polygon,elevation,height,source_sha in fixtures():
        source=Shape.from_polygon(polygon,elevation,material=7,tags=(label,)).extrude(height,'world.up')
        original=source.emit()
        for axis in ('x','y','z'):
            for method in ('geos',):
                children=source.split(axis,*pattern,adjust=False);amounts=[];attributes=0
                for child in children:
                    mesh=child.emit();closed(mesh);amounts.append(volume(mesh));attributes+=source_attributes(source,child)
                    assert child.split_info.total==4 and child.split_info.label==str(child.split_info.index)
                    coordinate=(mesh['vertices']-source.scope.origin)@source.scope.axes
                    dimension=('x','y','z').index(axis);a,b=child.split_info.interval
                    assert coordinate[:,dimension].min()>=a-1e-7 and coordinate[:,dimension].max()<=b+1e-7
                near(sum(amounts),polygon.area*height)
                if axis=='y':
                    near(amounts,[polygon.area*height/4]*4)
                    assert all(any(face.holes for face in child.geometry.polygons)==bool(polygon.interiors) for child in children)
                for name in original:near(source.emit()[name],original[name])
                rows.append(dict(label=label,axis=axis,source_sha256=source_sha,volumes_m3=amounts,
                    source_corners_checked=attributes,coplanar_source_omissions=sum(len(child.split_info.omitted) for child in children),
                    omitted_contacts=sum(len(child.split_info.omitted) for child in children)))
    return rows


def authored_controls():
    source=cube();faces=[]
    for face in source.geometry.polygons:
        rings=[]
        for ring in face.uvs:
            rings.append(tuple((u+.17*i*i,v-.11*i*i) for i,(u,v) in enumerate(ring)))
        faces.append(replace(face,uvs=tuple(rings)))
    vertices=source.emit()['vertices'];normals=np.tile([.2,.9,.1],(len(vertices),1))
    source=Shape.from_faces(vertices,faces,source.scope,vertex_normals=normals).with_metadata(source.geometry.group_names,source.geometry.material_defs)
    cases=[source,source.r(19.,-31.,23.,'scopeCenter').t(3.,2.,-1.),source.s(Relative(-1.),Relative(1.),Relative(1.))]
    cases.append(replace(cases[1],scope=replace(cases[1].scope,object_frame=Frame((100.,30.,-20.),(9.,17.,-8.)))))
    checked=0
    for parent in cases:
        for axis in ('x','y','z'):
            for method in ('geos',):
                children=parent.split(axis,Part(Floating(1)),Part(Floating(2)),cut_material=7,cut_group=8)
                near(sum(volume(child.emit()) for child in children),abs(volume(parent.emit())))
                for child in children:
                    closed(child.emit());checked+=source_attributes(parent,child)
                    assert child.t(1.,0.,0.).split_info is child.split_info
                    for component in child.comp('f'):assert component.split_info is child.split_info
    return dict(frames=len(cases),axis_method_cases=12,nonaffine_uv_and_normal_corners=checked)


def contact_controls():
    polygon=shapely.Polygon([(0,0),(12,0),(12,10),(0,10)],[[(3,2),(9,2),(9,8),(3,8)]])
    source=Shape.from_polygon(polygon,2.,material=7).extrude(4.)
    children=source.split('x',*[Part(Floating(1)) for _ in range(4)])
    assert 6 in children[0].split_info.source_faces and 8 in children[3].split_info.source_faces
    assert any(entry[3]=='coplanar source surface outside retained volume' for child in children for entry in child.split_info.omitted)
    partial=source.split('y',Part(1.),Part(1.),repeat=False)
    near(sum(volume(child.emit()) for child in partial),polygon.area*2.)
    repeat=source.split('y',Part(1.5),repeat=True)
    near([volume(child.emit()) for child in repeat],[126.,126.,84.])
    return dict(coplanar_hof_wall_material_uv_preserved=True,zero_volume_contacts_reported=True,partial_and_repeat_volume=True)


def invalid_controls():
    source=cube()
    whole=source.split('x',Part(Floating(1)))[0]
    near(volume(whole.emit()),volume(source.emit()));source_attributes(source,whole)
    vertices=source.emit()['vertices'];count=len(vertices)
    mixed=Shape.from_faces(np.r_[vertices,[[20.,2.,0.],[22.,2.,0.],[22.,2.,2.],[20.,2.,2.]]],
        (*source.geometry.polygons,Face(tuple(range(count,count+4)))))
    flat=Shape.from_faces([[0.,0.,0.],[2.,0.,0.],[0.,0.,2.]],[Face((0,1,2)),Face((2,1,0))])
    calls=[lambda:source.split('u',Part(1.)),lambda:source.split('x',Part(Floating(1)),Part(Floating(1))),
        lambda:source.split('x',Part(1.),cut_material=-1),lambda:source.split('x',Part(1.),cut_group=True),
        lambda:source.split('x',Part(.0001),repeat=True,max_parts=10),
        lambda:source.from_mesh(source.emit()).split('x',Part(Floating(1))),
        lambda:source.split('x',Part(1.),adjust='noAdjust'),lambda:mixed.split('x',Part(1.)),lambda:flat.split('x',Part(1.))]
    for call in calls:
        try:call()
        except (ValueError,NotImplementedError):pass
        else:raise AssertionError('Invalid, unsupported or unbounded cut accepted')
    return len(calls)


def open_controls():
    polygon=shapely.Polygon([(0,0),(12,0),(12,10),(0,10)],[[(3,2),(9,2),(9,8),(3,8)]])
    base=Shape.from_polygon(polygon,2.,material=7,tags=('open.source',))
    frames=[base,base.r(23.,17.,-31.).t(3.,4.,5.),base.s(Relative(-1.),Relative(1.),Relative(1.))]
    rows=[]
    for parent in frames:
        original=parent.emit();points=original['vertices'][original['faces']]
        expected=np.linalg.norm(np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0]),axis=1).sum()*.5
        for axis in ('x','z'):
            children=parent.split(axis,*[Part(Floating(1.),str(i)) for i in range(4)])
            areas=[];corners=0
            for child in children:
                mesh=child.emit();triangles=mesh['vertices'][mesh['faces']]
                areas.append(float(np.linalg.norm(np.cross(triangles[:,1]-triangles[:,0],triangles[:,2]-triangles[:,0]),axis=1).sum()*.5))
                corners+=source_attributes(parent,child)
                assert all(index==0 for index in child.split_info.source_faces)
                assert all('split.cut' not in face.tags for face in child.geometry.polygons)
            near(sum(areas),expected)
            rows.append(dict(axis=axis,parts=len(children),areas_m2=areas,source_corners_checked=corners))
    facade=cube().comp('f')[2]
    children=facade.split('x',Part(2.,'bay'),repeat=True)
    for child in children:source_attributes(facade,child)
    assert len(children)>1 and not any('split.cut' in face.tags for child in children for face in child.geometry.polygons)
    return dict(plan_axis_cases=rows,facade_bays=len(children),no_closure_surfaces=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/split-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',prisms=prism_controls(),authored=authored_controls(),contacts=contact_controls(),open_surfaces=open_controls(),invalid=invalid_controls(),
        limits='Open surfaces and closed triangle solids with logical faces; mixed primitives, UV-space, nested pattern syntax and isolated PRT cut rules remain pending')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',prism_axis_method_cases=len(record['prisms']),authored=record['authored'])))


if __name__=='__main__':main()
