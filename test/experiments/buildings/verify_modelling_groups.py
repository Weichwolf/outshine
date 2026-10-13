"""Check source groups, value-equivalent materials and hole surfaces without erasing provenance."""

import argparse
from dataclasses import replace
import json
from pathlib import Path

import numpy as np
import shapely

from esri_oracle import CASES,generate
from evidence import revision
from modelling_components import Branch,Surface
from modelling_faces import Face
from modelling_metadata import Material
from outshine_ref.buildings.modelling.scope import Frame
from modelling_selectors import select,tagged
from modelling_shape import Shape
from modelling_wire import Edge,Point
from provider_contract import attributes
from samples import read_site
from verify_modelling_extrude import near,volume


def reject(operation):
    try:operation()
    except (ValueError,NotImplementedError):return
    raise AssertionError('Invalid component metadata was accepted')


def cube():
    source=Shape.from_polygon(shapely.box(0,0,8,5)).extrude(4.)
    faces=[replace(face,group=8 if i<3 else 32,material=7 if i==0 else 31 if i==1 else 19,
                   tags=face.tags|({'source.marker'} if i==0 else set())) for i,face in enumerate(source.geometry.polygons)]
    stone=Material('stone',{'color':(1.,.5,.2),'opacity':1.});glass=Material('glass',{'opacity':.4})
    return Shape.from_faces(source.emit()['vertices'],faces,source.scope).with_metadata(
        {8:'frame',32:'shell',99:'unused'},{7:stone,19:Material('stone',dict(stone.attributes)),31:glass})


def signatures(mesh):
    groups=mesh.get('groups',np.zeros(len(mesh['faces']),dtype=np.uint32));rows=[]
    for triangle,material,group in zip(mesh['faces'],mesh['materials'],groups,strict=True):
        corners=tuple(sorted(tuple(np.round(mesh['vertices'][i],8))+tuple(np.round(mesh['uv'][i],8)) for i in triangle))
        rows.append((corners,int(material),int(group)))
    return sorted(rows)


def group_controls():
    source=cube().r(19.,-31.,23.,'scopeCenter').t(3.,2.,-1.)
    source=replace(source,scope=replace(source.scope,object_frame=Frame((10.,20.,30.),(9.,17.,-8.))))
    if source.group_names()!=('frame','shell') or source.material_names()!=('stone','glass'):
        raise AssertionError('Name arrays include unused entries or differ from component index order')
    rows=[]
    for kind,count in [('g',2),('m',2)]:
        for alignment in ['zUp','noAlign']:
            children=source.comp(kind,alignment=alignment)
            if len(children)!=count:raise AssertionError('Authored groups/material values were confused with palette indices')
            actual=sorted(row for child in children for row in signatures(child.emit()))
            if actual!=signatures(source.emit()):raise AssertionError('Group extraction changed native positions, corner UVs, material or group indices')
            for index,child in enumerate(children):
                info=child.comp_info
                if (info.index,info.total,info.source_indices,info.kind)!=(index,count,(index,),kind):raise AssertionError('Group predecessor attributes are incorrect')
                if child.geometry.group_names!=source.geometry.group_names or child.geometry.material_defs!=source.geometry.material_defs:
                    raise AssertionError('A child lost the authored name/material tables')
                if alignment=='noAlign':
                    if child.scope.pivot!=source.scope.pivot or child.scope.orientation!=source.scope.orientation:raise AssertionError('noAlign changed parent frame')
                for value in child.geometry.fields.values():
                    if value.flags.writeable:raise AssertionError('Grouped native data is mutable')
            rows.append(dict(kind=kind,alignment=alignment,components=count))
    pure=cube();both=select('top')&select('bottom')
    if len(pure.comp('g',Branch(both)))!=1:raise AssertionError('Group keyword conjunction did not inspect separate constituent faces')
    if len(pure.comp('g',Branch(~tagged('source.marker'))))!=1:raise AssertionError('Tag negation operated on individual members instead of the group')
    selected=pure.comp('g',Branch('top'),Branch('all'))
    if [child.comp_info.sel for child in selected]!=['top','all']:raise AssertionError('Already-selected groups reappeared in a later branch')
    if len(pure.comp('g',Branch('all','=:')))!=1 or len(pure.comp('g',Branch('all','=')))!=1:raise AssertionError('Group edge connectivity or combination failed')
    if pure.comp('m',Branch(1.))[0].material_names()!=('glass',):raise AssertionError('Numeric material index used the palette ID')
    if len(pure.comp('m',Branch(lambda query:query.area>100.)))!=1:raise AssertionError('Python aggregate predicates did not receive group area')
    unresolved=replace(pure,geometry=replace(pure.geometry,material_defs=()))
    if len(unresolved.comp('m'))!=3:raise AssertionError('Unresolved palette identifiers were silently assumed equal')
    return rows


def hole_controls():
    vertices=[[0.,3.,0.],[12.,3.,0.],[12.,3.,10.],[0.,3.,10.],[2.,3.,2.],[6.,3.,2.],[6.,3.,6.],[2.,3.,6.],
              [8.,3.,2.],[10.,3.,2.],[10.,3.,5.],[8.,3.,5.]]
    uv=(((0.,0.),(1.,0.),(1.,1.),(0.,1.)),((.1,.2),(.7,.9),(.5,.6),(.2,.8)),((.3,.1),(.6,.3),(.8,.4),(.9,.7)))
    source=Shape.from_faces(vertices,[Face((3,2,1,0),((4,5,6,7),(8,9,10,11)),7,('courtyard',),uv,18)])
    source=source.with_metadata({18:'roof'},{7:Material('stone')});surface=Surface(source);holes=source.comp('h')
    if len(holes)!=2:raise AssertionError('Hole components were merged or omitted')
    areas=[]
    for number,child in enumerate(holes):
        parent=surface.faces[0];face=child.geometry.polygons[0]
        if face.uvs!=(parent.uvs[number+1][::-1],) or face.material!=7 or face.group!=18 or face.tags!=parent.tags:
            raise AssertionError('Filled hole lost source corner UVs/material/group/tags')
        near(Surface(child).normals[0],surface.normals[0]);areas.append(Surface(child).areas[0])
        if child.group_names()!=('roof',):raise AssertionError('Hole extraction lost its group name')
        near(volume(child.extrude(2.,'world.up').emit()),areas[-1]*2.)
    near(areas,[16.,6.]);near(sum(areas)+surface.areas[0],120.)
    if len(source.comp('h',Branch('all','=:')))!=2 or len(source.comp('h',Branch('all','=')))!=1:raise AssertionError('Independent hole boundaries became connected')
    if Shape.from_polygon(shapely.box(0,0,3,4)).comp('h'):raise AssertionError('A hole was invented in a simple footprint')
    return dict(hole_areas_m2=areas,filled_normal_matches_parent=True,corner_uvs=True,source_group_and_material=True)


def wire_and_metadata_controls():
    vertices=[[0.,0.,0.],[4.,0.,0.],[4.,0.,4.]]
    source=Shape.from_edges(vertices,[Edge((0,1),7,group=8),Edge((1,2),31,group=32)],[[0.,1.,0.],[1.,0.,0.]])
    source=source.with_metadata({8:'beam',32:'brace'},{7:Material('metal'),31:Material('stone')})
    for kind in ['g','m']:
        children=source.comp(kind)
        if len(children)!=2 or len(source.comp(kind,Branch('all','=:')))!=1:raise AssertionError('Wire group extraction lost endpoint connectivity')
        for child in children:
            if len(child.geometry.edges)!=1 or len(child.emit()['faces']):raise AssertionError('Wire group extraction changed primitive dimension')
            product=child.extrude(1.);face=product.geometry.polygons[0]
            if face.group!=child.geometry.edges[0].group or product.geometry.material_defs!=source.geometry.material_defs:
                raise AssertionError('Edge extrusion lost group/material provenance')
    points=Shape.from_points(vertices,[Point((0,),7,group=8),Point((1,),31,group=32)],[[0,1,0],[0,1,0]])
    if len(points.comp('g'))!=2 or len(points.comp('m'))!=2 or points.comp('h'):raise AssertionError('Point groups changed dimension or invented holes')
    color=[.2,.3,.4];material=Material('owned',{'color':color});color[0]=999.
    if dict(material.attributes)['color']!=(.2,.3,.4) or replace(material,name='changed').attributes!=material.attributes:raise AssertionError('Material ownership/replacement is not immutable')
    reject(lambda:Face((0,1,2),group=-1));reject(lambda:Edge((0,1),group=True));reject(lambda:Material('invalid',{'color':float('nan')}))
    reject(lambda:source.with_metadata([(8,'a'),(8,'b')]))
    wrong=source.emit()|{'edges_groups':np.array([999,32],dtype=np.uint32)}
    reject(lambda:Shape.from_mesh(wrong,source.scope,edges=source.geometry.edges))
    surface=cube();wrong=surface.emit()|{'groups':np.full(len(surface.emit()['faces']),999,dtype=np.uint32)}
    reject(lambda:Shape.from_mesh(wrong,surface.scope,surface.geometry.polygons))
    return dict(wire_groups=True,point_groups=True,material_ownership=True,edge_extrusion_inherits_metadata=True,invalid_cases=6)


def reference_controls():
    rows=[]
    for label,site,identifier in CASES:
        sample=next(row for row in read_site(site)[0] if row['id']==identifier);data=attributes(sample['properties'])
        source=Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,));body=source.extrude(data['bodyHeight'],'world.up')
        _,_,reference=generate(sample,'extrusion_rule.rpk');holes=source.comp('h')
        filled=sum(abs(volume(hole.extrude(data['bodyHeight'],'world.up').emit())) for hole in holes)
        expected=shapely.Polygon(sample['polygon'].exterior).area*data['bodyHeight']
        near(reference['surface_integral_m3']+filled,expected,1e-4)
        for kind in ['g','m']:
            children=body.comp(kind);combined=np.concatenate([child.emit()['vertices'] for child in children])
            near([combined.min(axis=0),combined.max(axis=0)],reference['bounds_prt_m'])
            if sorted(row for child in children for row in signatures(child.emit()))!=signatures(body.emit()):raise AssertionError('A real body changed during group/material roundtrip')
        rows.append(dict(label=label,source_id=identifier,source_sha256=sample['source_sha256'],prt_version=reference['prt_version'],hole_components=len(holes),filled_hole_volume_m3=filled))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-groups-validation.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',groups=group_controls(),holes=hole_controls(),wire=wire_and_metadata_controls(),prt_reference=reference_controls(),
        prt_scope='Real standard-prism bounds/volume plus analytic filled voids; PyEncoder exposes no material/group metadata or isolated g/m/h rule',
        pending=['isolated PRT group/material/hole scope rule','full shader defaults and texture/object-pointer material equivalence','mixed primitive group assembly'])
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',group_frames=len(record['groups']),hole_controls=len(record['holes']['hole_areas_m2']),real_prt_cases=len(record['prt_reference']))))


if __name__=='__main__':main()
