"""Analytic two-plane roof integrals, native UV interpolation, logical joins and composition."""

import argparse
from dataclasses import replace
import json
import math
from pathlib import Path
import time

import numpy as np
import shapely

from evidence import revision
from modelling_components import Branch
from modelling_extrude import aligned_scope
from modelling_faces import ROOF_TAGS,face_shape,plane
from modelling_metadata import inherit
from modelling_scope import Frame
from modelling_selectors import tagged
from modelling_shape import Shape
from modelling_split_sizes import Part,Floating
from verify_modelling_roof_shed import source,measure
from verify_modelling_split import barycentric


def reframe(shape,angle):
    mesh=shape.emit();normal=plane(mesh['vertices'],shape.geometry.polygons[0])[0]
    scope=aligned_scope(mesh['vertices'],normal,replace(shape.scope,orientation=(0.,angle,0.)),False)
    return inherit(Shape.from_mesh(mesh,scope,shape.geometry.polygons),shape)


def analytical(shape,value,mode):
    mesh=shape.emit();face=shape.geometry.polygons[0];normal=plane(mesh['vertices'],face)[0]
    x=shape.scope.axes[:,0]-normal*np.dot(normal,shape.scope.axes[:,0])
    if np.linalg.norm(x)<1e-10:x=shape.scope.axes[:,1]-normal*np.dot(normal,shape.scope.axes[:,1])
    x/=np.linalg.norm(x);across=np.cross(normal,x);origin=mesh['vertices'][face.outer[0]];basis=np.array([x,across])
    rings=[(mesh['vertices'][list(ring)]-origin)@basis.T for ring in (face.outer,*face.holes)]
    polygon=shapely.Polygon(rings[0],rings[1:]);a,low,b,high=polygon.bounds;middle=(low+high)/2.;half=(high-low)/2.
    rise=value if mode=='byHeight' else math.tan(math.radians(value))*half;volume=0.
    for start,end,slope,intercept in [(low,middle,rise/half,-low*rise/half),(middle,high,-rise/half,high*rise/half)]:
        part=polygon.intersection(shapely.box(a-1.,start,b+1.,end))
        volume+=part.area*(slope*part.centroid.y+intercept)
    return polygon,origin,basis,normal,x,rise,half,volume


def uv_surface(original,result,normal,origin):
    before,after=original.emit(),result.emit();source_triangles=before['vertices'][before['faces']];checked=0
    channels=[key for key in before if key=='uv' or key.startswith('uv') and key[2:].isdigit()]
    for row,indices in enumerate(after['faces']):
        face=result.geometry.polygons[after['parts'][row]]
        if not face.tags&{'roof.top','roof.bottom'}:continue
        triangle=after['vertices'][indices]
        for weights in (np.array([1.,0.,0.]),np.array([0.,1.,0.]),np.array([0.,0.,1.]),np.array([.2,.3,.5])):
            point=weights@triangle;point-=normal*np.dot(point-origin,normal);matches=[]
            for number,candidate in enumerate(source_triangles):
                bary,error=barycentric(point,candidate)
                if error<1e-7 and bary.min()>-1e-8:matches.append((number,bary))
            assert matches,'Roof point left its source triangle surface'
            if not any(all(np.allclose(weights@after[key][indices],bary@before[key][before['faces'][number]],atol=1e-7,rtol=1e-9)
                for key in channels) for number,bary in matches):raise AssertionError('Roof cut changed its native source UV interpolation')
            checked+=len(channels)
    return checked


def logical_ridge(result,origin,basis,middle):
    faces=result.geometry.polygons;vertices=result.emit()['vertices'];owners={};shared=0
    for index,face in enumerate(faces):
        for ring in (face.outer,*face.holes):
            for a,b in zip(ring,ring[1:]+ring[:1]):owners.setdefault(tuple(sorted((a,b))),[]).append(index)
    assert all(len(group)==2 for group in owners.values()),'Logical roof faces do not share a closed boundary'
    for (a,b),group in owners.items():
        if not all('roof.top' in faces[index].tags for index in group):continue
        points=vertices[[a,b]];coordinates=(points-origin)@basis.T
        np.testing.assert_allclose(coordinates[:,1],middle,atol=1e-7)
        assert np.linalg.norm(points[1]-points[0])>1e-10;shared+=1
    assert shared>0,'Roof has no shared logical ridge edge'
    return shared


def verify(shape,value,mode):
    baseline=shape.emit();roof=shape.roof_ridge(value,mode);mesh=roof.emit();polygon,origin,basis,normal,x,rise,half,expected=analytical(shape,value,mode)
    np.testing.assert_allclose(measure(mesh),expected,atol=1e-7,rtol=1e-9)
    np.testing.assert_allclose(roof.scope.axes[:,1],normal,atol=1e-10)
    np.testing.assert_allclose(roof.roof_info.ridge_directions[0],x,atol=1e-10)
    np.testing.assert_allclose(roof.roof_info.rises_m[0],rise,atol=1e-8)
    top=roof.comp('f',Branch(tagged('roof.top')),alignment='noAlign');triangles=[]
    for part in top:
        native=part.emit();points=native['vertices'][native['faces']];triangles.extend(points)
        vectors=np.cross(points[:,1]-points[:,0],points[:,2]-points[:,0]);normals=vectors/np.linalg.norm(vectors,axis=1)[:,None]
        np.testing.assert_allclose(normals@normal,1/math.sqrt(1+(rise/half)**2),atol=1e-8)
    projected=[shapely.Polygon((triangle-origin)@basis.T) for triangle in triangles];cover=shapely.union_all(projected)
    assert cover.symmetric_difference(polygon).area<1e-7 and abs(sum(p.area for p in projected)-polygon.area)<1e-7
    first=logical_ridge(roof,origin,basis,sum(polygon.bounds[1::2])/2.)
    checked=uv_surface(shape,roof,normal,origin)
    assert roof.geometry.group_names==shape.geometry.group_names and roof.geometry.material_defs==shape.geometry.material_defs
    assert roof.material==shape.material and roof.textures==shape.textures and roof.projections==shape.projections
    assert all(f.material==9 and f.group==3 and 'source.roof' in f.tags and len(f.tags&ROOF_TAGS.keys())==1 for f in roof.geometry.polygons)
    for key in baseline:np.testing.assert_array_equal(shape.emit()[key],baseline[key])
    return dict(mode=mode,value=value,volume_m3=expected,triangles=len(mesh['faces']),top_components=len(top),shared_ridge_edges=first,uv_values_checked=checked)


def controls():
    polygons=[shapely.box(0,0,8,6),shapely.Polygon([(0,0),(8,0),(8,3),(4,3),(4,7),(0,7)]),
        shapely.Polygon([(0,0),(10,0),(10,8),(0,8)],holes=[[(2,2),(2,6),(7,6),(7,2)]]),
        shapely.Polygon([(0,0),(10,0),(10,8),(7,8),(7,2),(3,2),(3,8),(0,8)])]
    rows=[]
    for polygon in polygons:
        original=source(polygon)
        for angle in (0.,31.,90.):
            for value,mode in [(25.,'byAngle'),(3.,'byHeight')]:rows.append(verify(reframe(original,angle),value,mode))
        framed=replace(original,scope=replace(original.scope,pivot=Frame((10.,-3.,8.),(7.,11.,19.)),object_frame=Frame((100.,200.,-300.),(13.,17.,23.))))
        for shape in [original.r(31,17,23).t(10,-4,3),original.s(-12,0,9).r(17,31,0),framed]:rows.append(verify(shape,3.,'byHeight'))
    composition(polygons[0]);extra_inputs(polygons[0]);return rows


def composition(polygon):
    shape=source(polygon);roof=shape.roof_ridge(3.,'byHeight');normal=plane(shape.emit()['vertices'],shape.geometry.polygons[0])[0];origin=shape.emit()['vertices'][shape.geometry.polygons[0].outer[0]]
    changed=roof.emit();changed['uv']=changed['uv']+.25
    corrupted=Shape.from_mesh(changed,roof.scope,roof.geometry.polygons)
    try:uv_surface(shape,corrupted,normal,origin)
    except AssertionError:pass
    else:raise AssertionError('UV corruption control was not detected')
    plain=Shape.from_polygon(polygon).roof_ridge(3.,'byHeight');children=plain.split('x',Part(Floating(1.),'left'),Part(Floating(1.),'right'))
    np.testing.assert_allclose(sum(measure(child.emit()) for child in children),measure(plain.emit()),atol=1e-8)
    assert 'uv' not in Shape.from_polygon(polygon).delete_uv(0).roof_ridge(3.,'byHeight').emit()
    for value,mode in [(0.,'byAngle'),(-20.,'byAngle'),(90.,'byAngle'),(float('nan'),'byHeight'),(-3.,'byHeight'),(True,'byAngle'),(3.,'unknown')]:
        try:shape.roof_ridge(value,mode)
        except ValueError:pass
        else:raise AssertionError('Invalid ridge value accepted')


def extra_inputs(polygon):
    base=source(polygon);mesh=base.emit();face=base.geometry.polygons[0]
    changed=dict(mesh);changed['uv']=mesh['uv'].copy();changed['uv'][mesh['faces'][0]]+=[7.,3.]
    seam=inherit(Shape.from_mesh(changed,base.scope,base.geometry.polygons),base);verify(seam,3.,'byHeight')
    normal=np.tile([0.,1.,0.],(len(mesh['vertices']),1));authored=Shape.from_mesh(mesh|dict(normals=normal),base.scope,base.geometry.polygons)
    assert authored.roof_ridge(3.,'byHeight').roof_info.authored_normals_rebuilt
    assert 'normals' not in authored.roof_ridge(3.,'byHeight').emit()
    other=base.r(-90.,90.,0.).translate('rel','world',20.,0.,0.);second=other.emit();offset=len(mesh['vertices'])
    joined={key:np.concatenate([mesh[key],second[key]+offset if key=='faces' else second[key]+1 if key=='parts' else second[key]]) for key in mesh}
    moved=other.geometry.polygons[0];logical=(face,replace(moved,outer=tuple(i+offset for i in moved.outer)))
    combined=Shape.from_mesh(joined,polygons=logical);roof=combined.roof_ridge(3.,'byHeight');product=roof.emit()
    for owner in (0,1):
        selected=np.asarray(roof.roof_info.source_faces)[product['parts']]==owner
        np.testing.assert_allclose(measure(product|dict(faces=product['faces'][selected])),72.,atol=1e-7)
    warp=face_shape([[0,0,0],[8,0,0],[8,0,6],[0,.2,6]],[replace(face,uvs=(),uv_sets=())],allow_warped=True)
    for invalid in [Shape.from_mesh(mesh),Shape.from_mesh(mesh|dict(vertex_colors=np.ones((len(mesh['vertices']),4))),polygons=base.geometry.polygons),warp,
        Shape.from_mesh(mesh|dict(faces=mesh['faces'][:,::-1]),polygons=base.geometry.polygons)]:
        try:invalid.roof_ridge(3.,'byHeight')
        except (ValueError,NotImplementedError):pass
        else:raise AssertionError('Unsupported or inconsistent source geometry was silently accepted')


def timings():
    shape=source(shapely.Polygon([(0,0),(10,0),(10,8),(0,8)],holes=[[(2,2),(2,6),(7,6),(7,2)]]));shape.roof_ridge(3.,'byHeight');values=[]
    for _ in range(21):
        start=time.perf_counter_ns();shape.roof_ridge(3.,'byHeight');values.append((time.perf_counter_ns()-start)/1e6)
    return dict(p50_ms=float(np.percentile(values,50)),p95_ms=float(np.percentile(values,95)),scope='warm Python roof generation and native array assembly with UV0-9; excludes source, final render and GPU')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-roof-ridge-verification.json'));args=parser.parse_args()
    record=revision()|dict(status='pass',cases=controls(),timings=timings(),limits='Analytic controls; exact SDK connectivity/normals, trim planes, isolated CGA rule, multi-face adjacency and native LOD remain unproved')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',cases=len(record['cases']),timings=record['timings'])))
