"""Check component geometry, frames, exclusion, connectivity, predicates and reference round trips."""

import argparse
from dataclasses import replace
import hashlib
import json
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely

from esri_oracle import CASES,generate,geometry_digest
from evidence import revision
from modelling_components import Branch,Surface
from modelling_faces import Face
from outshine_ref.buildings.modelling.scope import Frame,Scope
from modelling_selectors import HORIZONTAL,NUTANT_HIGH,VERTICAL,Selector,select,tagged
from modelling_shape import Shape
from provider_contract import attributes
from samples import read_site
from verify_modelling_extrude import closed,near,volume


def reject(operation,kind=ValueError):
    try:operation()
    except kind:return
    raise AssertionError('Unsupported or invalid component input was silently accepted')


def combined(shapes):
    vertices=[];faces=[];offset=0
    for shape in shapes:
        mesh=shape.emit();vertices.append(mesh['vertices']);faces.append(mesh['faces']+offset);offset+=len(mesh['vertices'])
    return dict(vertices=np.concatenate(vertices),faces=np.concatenate(faces))


def uv_signature(mesh):
    rows=[]
    for face in mesh['faces']:
        positions=np.rint(mesh['vertices'][face]*1e8).astype(np.int64);uv=np.rint(mesh['uv'][face]*1e8).astype(np.int64)
        first=min(range(3),key=lambda i:tuple(positions[i]));indices=(np.arange(3)+first)%3
        rows.append(tuple(np.c_[positions,uv][indices].ravel()))
    return hashlib.sha256(np.asarray(sorted(rows),dtype=np.int64).tobytes()).hexdigest()


def reconstructed_uv(shapes):
    positions=[];faces=[];uv=[];offset=0
    for shape in shapes:
        mesh=shape.emit();positions.append(mesh['vertices']);faces.append(mesh['faces']+offset);uv.append(mesh['uv']);offset+=len(mesh['vertices'])
    return dict(vertices=np.concatenate(positions),faces=np.concatenate(faces),uv=np.concatenate(uv))


def frame_controls():
    source=Shape.from_polygon(shapely.box(0,0,8,5),tags=('origin',)).extrude(4)
    selected=source.comp('f',Branch('front'),Branch('side'),Branch('all','='))
    expected=[('front',0,1),('side',0,3),('side',1,3),('side',2,3),('all',0,1)]
    if [(p.comp_info.sel,p.comp_info.index,p.comp_info.total) for p in selected]!=expected:
        raise AssertionError('Selector exclusion or per-selector comp.index/total differs from the documented cube example')
    if set(i for p in selected for i in p.comp_info.source_indices)!=set(range(6)):
        raise AssertionError('Component selection omitted or duplicated a source face')
    base=source.emit();roundtrip=combined(selected)
    if geometry_digest(base['vertices'],base['faces'])!=geometry_digest(roundtrip['vertices'],roundtrip['faces']):
        raise AssertionError('Component frames moved or retriangulated the parent surface')
    if uv_signature(base)!=uv_signature(reconstructed_uv(selected)):raise AssertionError('Component frames changed triangle-corner UV interpolation')
    rotated=source.r(17,29,-31,'scopeCenter').t(2,-3,5)
    frame=Frame((31,-17,9),(24,37,-12));rotated=replace(rotated,scope=replace(rotated.scope,object_frame=frame,pivot=Frame((2,3,-4),(11,-7,13))))
    checks=0
    for alignment in ('zUp','noAlign'):
        children=rotated.comp('f',Branch('all'),alignment=alignment)
        for child in children:
            index=child.comp_info.source_indices[0];parent_face=rotated.geometry.polygons[index]
            parent_points=rotated.emit()['vertices'];ring=parent_points[list(parent_face.outer)]
            if alignment=='zUp':
                near(child.scope.pivot_origin,ring[0]);near(child.scope.axes[:,0],(ring[1]-ring[0])/np.linalg.norm(ring[1]-ring[0]))
                normal=np.cross(ring[1]-ring[0],ring[2]-ring[0]);normal/=np.linalg.norm(normal)
                near(child.scope.axes[:,2],normal);near(child.scope.size[2],0.)
            else:
                if child.scope.pivot!=rotated.scope.pivot or child.scope.orientation!=rotated.scope.orientation:
                    raise AssertionError('noAlign changed the source pivot or orientation')
            mesh=child.emit();local=(mesh['vertices']-child.scope.origin)@child.scope.axes
            near(local.min(axis=0),np.minimum(0,child.scope.size));near(local.max(axis=0),np.maximum(0,child.scope.size))
            for field in (child.geometry.unit_vertices,*child.geometry.fields.values()):
                if field.flags.writeable:raise AssertionError('Component geometry is mutable')
            checks+=1
        near(volume(combined(children)),volume(rotated.emit()))
    with patch('shapely.constrained_delaunay_triangles',side_effect=AssertionError('Face selection retriangulated immutable source geometry')):
        source.comp('f')
    negative=Shape.from_polygon(shapely.box(0,0,8,5)).extrude(-4)
    if set(negative.comp('f',Branch('top'))[0].emit()['face_tag_mask'])!={8}:
        raise AssertionError('Negative scope size did not reverse the local top direction')
    if set(negative.comp('f',Branch('world.up'))[0].emit()['face_tag_mask'])!={1}:
        raise AssertionError('World up incorrectly followed a negative local scope axis')
    return dict(rotated_component_frames=checks,documented_cube_groups=len(selected),no_retriangulation=True,negative_local_world_directions=True)


def grid():
    vertices=np.array([[x,0.,-y] for y in range(4) for x in range(4)])
    faces=[]
    for y in range(3):
        for x in range(3):
            first=y*4+x;index=len(faces);tags={f'grid.{index}'}
            if index==4:tags.add('core')
            if index==0:tags|={'Facade.Front','Facade.TypeA.Side.Left'}
            faces.append(Face((first,first+1,first+5,first+4),tags=tags))
    return Shape.from_faces(vertices,faces)


def topology_controls():
    source=grid();border=source.comp('f',Branch('border'));inside=source.comp('f',Branch('inside'))
    if len(border)!=8 or len(inside)!=1 or inside[0].comp_info.source_indices!=(4,):
        raise AssertionError('Open grid border/inside classification is incorrect')
    counts=[len(source.comp('f',Branch(tagged('core',sub)))) for sub in (None,'e','v')]
    if counts!=[1,5,9]:raise AssertionError('Face tags did not propagate through the requested edge/vertex adjacency')
    for query in ('Facade','?.Front','Facade.*.Left'):
        if len(source.comp('f',Branch(tagged(query))))!=1:raise AssertionError('Hierarchical subtag wildcard differs from the SDK examples')
    if source.comp('f',Branch(tagged('?.TypeA.Left'))):raise AssertionError('Question-mark wildcard consumed multiple subtags')
    courtyard=shapely.Polygon([(0,0),(12,0),(12,10),(0,10)],[[(3,2),(9,2),(9,8),(3,8)]])
    body=Shape.from_polygon(courtyard).extrude(6)
    walls=body.comp('f',Branch(tagged('extrude.side'),'=:'))
    if len(walls)!=2 or [len(p.geometry.polygons) for p in walls]!=[4,4]:raise AssertionError('Connected operator joined the outer walls to the courtyard walls')
    if body.comp('f',Branch('border')):raise AssertionError('Closed courtyard prism has a false border component')
    roof=body.comp('f',Branch(tagged('extrude.top')))[0]
    if len(roof.geometry.polygons[0].holes)!=1:raise AssertionError('Component extraction filled the courtyard')
    vertices=np.array([[0,0,0],[2,0,0],[2,0,-2],[0,0,-2],[2,0,0],[4,0,0],[4,0,-2],[2,0,-2]],dtype=float)
    disjoint=Shape.from_faces(vertices,[Face((0,1,2,3)),Face((4,5,6,7))])
    if len(disjoint.comp('f',Branch('all','=:')))!=2:raise AssertionError('Component grouping implicitly welded disconnected source vertex IDs')
    touching=Shape.from_faces(np.r_[vertices[:4],[[4,0,-2],[4,0,-4],[2,0,-4]]],[Face((0,1,2,3)),Face((2,4,5,6))])
    if len(touching.comp('f',Branch('all','=:')))!=2:raise AssertionError('Faces sharing only a vertex were combined as edge-connected')
    return dict(grid_border_faces=8,grid_inside_faces=1,tag_adjacency_counts=counts,courtyard_wall_groups=2,disconnected_faces_preserved=True)


def selector_controls():
    query=Surface(grid()).query(4)
    def normal(y):return replace(query,normal=(np.sqrt(1-y*y),y,0.),scope_normal=(np.sqrt(1-y*y),y,0.),object_normal=(np.sqrt(1-y*y),y,0.),scope_size=(1.,1.,1.))
    expected=[(VERTICAL,{'vertical','side'}),(np.nextafter(VERTICAL,1.),{'aslant','side'}),
              (HORIZONTAL,{'aslant','side'}),(np.nextafter(HORIZONTAL,1.),{'horizontal'}),
              (-HORIZONTAL,{'side'}),(np.nextafter(-HORIZONTAL,0.),{'nutant','side'}),
              (NUTANT_HIGH,{'vertical','nutant','side'})]
    names=('vertical','horizontal','aslant','nutant','side')
    for value,wanted in expected:
        actual={name for name in names if select(name)(normal(value))}
        if actual!=wanted:raise AssertionError(f'Inclination boundary {value}: {actual} != {wanted}')
    diagonal=replace(query,scope_normal=(1/np.sqrt(2),0.,1/np.sqrt(2)),scope_size=(1.,1.,1.))
    if not (select('front')(diagonal) and select('right')(diagonal)):raise AssertionError('Direction ties are not inclusive')
    cube=Shape.from_polygon(shapely.box(0,0,8,5)).extrude(4)
    alternatives=cube.comp('f',Branch(select('top')|select('front'),'='),Branch(~select('inside')))
    if len(alternatives)!=1 or len(alternatives[0].geometry.polygons)!=2:raise AssertionError('Logical selection or exclusion is incorrect')
    if len(cube.comp('f',Branch(lambda face:face.area>30)))!=4:raise AssertionError('Python area predicate selected the wrong faces')
    if len(cube.comp('f',Branch(True)))!=6 or cube.comp('f',Branch(False)):raise AssertionError('Boolean component selection is incorrect')
    if cube.comp('f',Branch(3.))[0].comp_info.source_indices!=(3,):raise AssertionError('Numeric component indexing is incorrect')
    for value in ('front','back','left','right','top','bottom'):
        if len(cube.comp('f',Branch(value)))!=1:raise AssertionError('Axis-aligned direction count is incorrect')
    for value in ('world.south','world.north','world.west','world.east','world.up','world.down'):
        if len(cube.comp('f',Branch(value)))!=1:raise AssertionError('World-axis direction count is incorrect')
    for value in ('object.front','object.back','object.left','object.right','object.top','object.bottom','street.front','street.back','street.left','street.right'):
        if len(cube.comp('f',Branch(value)))!=1:raise AssertionError('Object/street-fallback direction count is incorrect')
    reject(lambda:cube.comp('f',Branch('unknown')));reject(lambda:cube.comp('f',alignment='unknown'))
    reject(lambda:cube.comp('invalid'));reject(lambda:Branch('all','unknown'));reject(lambda:select(.5))
    reject(lambda:cube.comp('f',Branch(lambda _:1)));reject(lambda:tagged('Facade.F*'));reject(lambda:bool(select('front')),TypeError)
    return dict(inclination_boundary_controls=len(expected),documented_direction_forms=22,logical_python_predicates=True,invalid_inputs_rejected=8)


def inheritance_controls():
    points=np.array([[0.,0.,0.],[4.,0.,0.],[4.,0.,-3.],[0.,0.,-3.]])
    uv=(((0.,0.),(1.,.2),(.7,1.),(.1,.7)),)
    face=Face((0,1,2,3),material=7,tags=('keep-custom','extrude.side.outer'),uvs=uv)
    source=Shape.from_faces(points,[face],vertex_normals=np.tile([0.,1.,0.],(4,1))).r(13,22,17)
    child=source.comp('f')[0]
    if uv_signature(source.emit())!=uv_signature(child.emit()):raise AssertionError('Nonaffine corner UV interpolation changed on extraction')
    near(np.sort(child.emit()['normals'],axis=0),np.sort(source.emit()['normals'],axis=0))
    if child.geometry.polygons[0].material!=7 or child.geometry.polygons[0].tags!=face.tags:
        raise AssertionError('Component extraction lost the material or inherited tags')
    info=child.comp_info;changed=child.t(1,2,3).r(4,5,6).s(5,6,0)
    if changed.comp_info is not info or changed.geometry is not child.geometry:raise AssertionError('Scope operations lost last-comp metadata or copied geometry')
    solid=child.extrude(.3);closed(solid.emit());near(volume(solid.emit()),3.6)
    if solid.comp_info is not info:raise AssertionError('Extrusion lost predecessor comp metadata')
    if set(solid.emit()['face_tag_mask'])!={1,2,8}:raise AssertionError('A repeated operation retained obsolete automatic extrusion tags')
    if not all('keep-custom' in row.tags for row in solid.geometry.polygons):raise AssertionError('Automatic-tag replacement removed a user tag')
    return dict(nonaffine_uvs_preserved=True,authored_normals_preserved=True,predecessor_comp_attributes_preserved=True,repeated_auto_tags_replaced=True)


def warped_controls():
    vertices=np.array([[x,.5*max(0,x-4)+.25*max(0,n-4),-n] for n in (0.,4.,8.) for x in (0.,4.,8.)])
    faces=[Face((i,i+1,i+4,i+3)) for i in (0,1,3,4)]
    source=Shape.from_faces(vertices,faces).extrude(1.,'vertex.normal');children=source.comp('f');mesh=source.emit();roundtrip=combined(children)
    if geometry_digest(mesh['vertices'],mesh['faces'])!=geometry_digest(roundtrip['vertices'],roundtrip['faces']):
        raise AssertionError('Nonplanar component fitting flattened or retriangulated the source surface')
    if uv_signature(mesh)!=uv_signature(reconstructed_uv(children)):raise AssertionError('Warped caps lost source UV interpolation')
    top=source.comp('f',Branch(tagged('extrude.top')))
    if len(top)!=4 or not all(child.scope.size[2]>1e-5 for child in top):raise AssertionError('Warped cap components lost their actual thickness')
    return dict(components=len(children),top_scope_thickness_m=[child.scope.size[2] for child in top],
                scope='own geometry/UV preservation and tight bounds; exact SDK nonplanar component alignment remains unverified')


def reference_controls():
    rows=[];sites={}
    for label,site,identifier in CASES:
        if site not in sites:sites[site]=read_site(site)[0]
        sample=next(row for row in sites[site] if row['id']==identifier);data=attributes(sample['properties'])
        body=Shape.from_polygon(sample['polygon'],data['minHeight'],tags=(identifier,)).extrude(data['bodyHeight'],'world.up')
        children=body.comp('f');product=combined(children);closed(product);_,_,reference=generate(sample,'extrusion_rule.rpk')
        near(volume(product),reference['surface_integral_m3'],max(1e-6,reference['surface_integral_m3']*1e-8))
        near([product['vertices'].min(axis=0),product['vertices'].max(axis=0)],reference['bounds_prt_m'])
        if uv_signature(body.emit())!=uv_signature(reconstructed_uv(children)):raise AssertionError('Real footprint component extraction changed source UV interpolation')
        rows.append(dict(label=label,source_id=identifier,source_sha256=sample['source_sha256'],components=len(children),
                         source_holes=len(sample['polygon'].interiors),volume_m3=volume(product),prt_version=reference['prt_version']))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-component-validation.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',frames=frame_controls(),topology=topology_controls(),
        selectors=selector_controls(),inheritance=inheritance_controls(),real_prt_roundtrips=reference_controls(),
        warped=warped_controls(),
        prt_scope='Geometry roundtrip against a standard extrusion RPK; no isolated CGA comp selector/pivot oracle',
        pending=['edge/face-edge/vertex/face-vertex/material/group/hole components','streetWidth-map selectors','trim-plane propagation',
                 'fractional numeric selector semantics','isolated PRT comp rules','exact SDK nonplanar component alignment'])
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',control_groups=5,real_prt_roundtrips=len(record['real_prt_roundtrips']))))


if __name__=='__main__':main()
