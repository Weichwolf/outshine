"""Analytic shed-roof volumes, closure, source coverage, frames and corner-attribute controls."""

import argparse
from dataclasses import replace
import json
import math
from pathlib import Path
import time

import numpy as np
import shapely

from evidence import revision
from outshine_ref.buildings.modelling.components import Branch
from outshine_ref.buildings.modelling.faces import ROOF_TAGS,face_shape,plane
from outshine_ref.buildings.modelling.metadata import Material
from outshine_ref.buildings.modelling.scope import Frame
from outshine_ref.buildings.modelling.selectors import tagged
from outshine_ref.buildings.modelling.shape import Shape
from outshine_ref.buildings.modelling.split_sizes import Floating,Part


def source(polygon):
    result=Shape.from_polygon(polygon,material=9,tags=('source.roof','roof.top'))
    face=result.geometry.polygons[0]
    values=tuple(tuple((float(i*i+number),float((i+1)%3)) for i in range(len(ring))) for number,ring in enumerate((face.outer,*face.holes)))
    result=Shape.from_faces(result.emit()['vertices'],[replace(face,group=3,uvs=values)]).with_metadata(
        group_names={3:'roof'},material_defs={9:Material('slate',{'roughness':.8})}).color(.2,.3,.4)
    for channel in range(1,10):result=result.copy_uv(0,channel).scale_uv(channel,channel+1,channel+2)
    return result


def measure(product):
    points=product['vertices'];triangles=points[product['faces']];centre=triangles.reshape(-1,3).mean(axis=0)
    shifted=triangles-centre;vectors=np.cross(shifted[:,1]-shifted[:,0],shifted[:,2]-shifted[:,0])
    assert np.all(np.linalg.norm(vectors,axis=1)>1e-10)
    volume=float(np.einsum('ij,ij->i',shifted[:,0],np.cross(shifted[:,1],shifted[:,2])).sum()/6)
    _,ids=np.unique(np.round(points,9),axis=0,return_inverse=True)
    faces=ids[product['faces']];edges=np.concatenate([faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]])
    keys=np.sort(edges,axis=1);_,inverse,counts=np.unique(keys,axis=0,return_inverse=True,return_counts=True)
    assert np.all(counts==2),'Roof has an open or nonmanifold triangle edge'
    signs=np.where(edges[:,0]<edges[:,1],1,-1);sums=np.bincount(inverse,weights=signs)
    assert np.all(sums==0),'Roof triangle winding is inconsistent'
    return volume


def verify_case(original,polygon,value,index,mode):
    baseline=original.emit();result=original.roof_shed(value,index,mode);mesh=result.emit();normal,_,_,face=plane(baseline['vertices'],original.geometry.polygons[0])
    np.testing.assert_array_equal(original.emit()['vertices'],baseline['vertices'])
    chosen=int(round(index))%len(face.outer);a,b=face.outer[chosen],face.outer[(chosen+1)%len(face.outer)]
    tangent=baseline['vertices'][b]-baseline['vertices'][a];direction=np.cross(normal,tangent/np.linalg.norm(tangent))
    if value<0:direction=-direction
    original_points=baseline['vertices'][list(face.outer)];extents=(original_points-original_points[0])@direction
    run=float(np.ptp(extents));rise=value if mode=='byHeight' else math.tan(math.radians(abs(value)))*run
    triangle=baseline['vertices'][baseline['faces']];vectors=np.cross(triangle[:,1]-triangle[:,0],triangle[:,2]-triangle[:,0]);weights=np.linalg.norm(vectors,axis=1)/2
    area=weights.sum();centroid=np.average(triangle.mean(axis=1),axis=0,weights=weights)
    fraction=(np.dot(centroid-original_points[0],direction)-extents.min())/run
    volume=measure(mesh);np.testing.assert_allclose(volume,area*rise*fraction,atol=1e-7,rtol=1e-9)
    np.testing.assert_allclose(result.scope.axes[:,1],normal,atol=1e-10)
    active=mesh['vertices'][np.unique(mesh['faces'])];local=(active-result.scope.origin)@result.scope.axes
    np.testing.assert_allclose(local.min(axis=0),0.,atol=1e-8)
    np.testing.assert_allclose(local.max(axis=0),result.scope.size,atol=1e-8)
    assert result.geometry.material_defs==original.geometry.material_defs and result.geometry.group_names==original.geometry.group_names
    assert result.material==original.material and result.textures==original.textures and result.projections==original.projections
    assert all(f.material==9 and f.group==3 and 'source.roof' in f.tags and len(f.tags&ROOF_TAGS.keys())==1 for f in result.geometry.polygons)
    top=result.comp('f',Branch(tagged('roof.top')),alignment='noAlign');bottom=result.comp('f',Branch(tagged('roof.bottom')),alignment='noAlign')
    assert len(top)==len(bottom)==1
    for channel in range(10):
        get=lambda f:f.uvs if channel==0 else dict(f.uv_sets)[channel]
        assert get(top[0].geometry.polygons[0])==get(face)
        assert get(bottom[0].geometry.polygons[0])==tuple(row[::-1] for row in get(face))
    roof=top[0].emit();tri=roof['vertices'][roof['faces']];top_vectors=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]);top_normals=top_vectors/np.linalg.norm(top_vectors,axis=1)[:,None]
    assert np.all(top_normals@normal>0.)
    np.testing.assert_allclose(top_normals@normal,1/math.sqrt(1+(rise/run)**2),atol=1e-9)
    projected=(roof['vertices']-original.scope.origin)@original.scope.axes
    cover=shapely.union_all([shapely.Polygon(t[:,[0,2]]) for t in projected[roof['faces']]])
    source_projected=(triangle-original.scope.origin)@original.scope.axes
    expected=shapely.union_all([shapely.Polygon(t[:,[0,2]]) for t in source_projected])
    assert cover.symmetric_difference(expected).area<1e-7 and len(cover.interiors)==len(polygon.interiors)
    tags=np.array([sum(bit for tag,bit in ROOF_TAGS.items() if tag in f.tags) for f in result.geometry.polygons],dtype=np.uint8)
    np.testing.assert_array_equal(mesh['face_tag_mask'],tags[mesh['parts']])
    return dict(value=value,index=index,mode=mode,triangles=len(mesh['faces']),volume_m3=volume,rise_m=rise,run_m=run,source_holes=len(polygon.interiors))


def controls():
    outlines=[shapely.box(0,0,8,6),shapely.Polygon([(0,0),(8,0),(8,3),(4,3),(4,7),(0,7)]),
        shapely.Polygon([(0,0),(10,0),(10,8),(0,8)],holes=[[(2,2),(2,6),(7,6),(7,2)]])]
    rows=[]
    for polygon in outlines:
        base=source(polygon)
        for value,index,mode in [(20.,0,'byAngle'),(-20.,0,'byAngle'),(3.,1,'byHeight'),(3.,-1,'byHeight'),
                (30.,len(base.geometry.polygons[0].outer)+2,'byAngle'),(3.,1.5,'byHeight'),(3.,-2.5,'byHeight')]:
            rows.append(verify_case(base,polygon,value,index,mode))
        framed=replace(base,scope=replace(base.scope,pivot=Frame((10.,-3.,8.),(7.,11.,19.)),object_frame=Frame((100.,200.,-300.),(13.,17.,23.))))
        for frame in [base.r(31.,17.,23.).t(10.,-4.,3.),base.s(-12.,0.,9.).r(17.,31.,0.),framed]:
            rows.append(verify_case(frame,polygon,3.,2,'byHeight'))
    bare=Shape.from_polygon(outlines[0]).delete_uv(0).roof_shed(3.,mode='byHeight')
    assert not any(k.startswith('uv') for k in bare.emit())
    authored=Shape.from_faces(source(outlines[0]).emit()['vertices'],source(outlines[0]).geometry.polygons,vertex_normals=np.tile([0.,1.,0.],(len(source(outlines[0]).emit()['vertices']),1)))
    assert authored.roof_shed(20.).roof_info.authored_normals_rebuilt and 'normals' not in authored.roof_shed(20.).emit()
    metadata_controls(outlines[0])
    invalid=[(True,0,'byAngle'),(float('nan'),0,'byAngle'),(0.,0,'byAngle'),(90.,0,'byAngle'),(-90.,0,'byAngle'),
        (-3.,0,'byHeight'),(0.,0,'byHeight'),(20.,True,'byAngle'),(20.,float('inf'),'byAngle'),(20.,2**31,'byAngle'),(20.,0,'unknown')]
    for args in invalid:
        try:base.roof_shed(*args)
        except ValueError:pass
        else:raise AssertionError('Invalid roof parameter accepted')
    return rows


def metadata_controls(polygon):
    base=source(polygon);mesh=base.emit();face=base.geometry.polygons[0]
    uvs=mesh['uv'].copy();uvs[list(face.outer)]=[[0.,0.],[3.,1.],[5.,4.],[1.,2.]]
    stream=Shape.from_mesh(mesh|dict(uv=uvs),polygons=(replace(face,uvs=(),uv_sets=()),))
    roof=stream.roof_shed(2.,mode='byHeight');top=next(f for f in roof.geometry.polygons if 'roof.top' in f.tags)
    assert top.uvs==(tuple(map(tuple,uvs[list(face.outer)])),),'Native corner UV fallback was replaced by generated coordinates'
    a=Shape.from_polygon(shapely.box(0,0,4,3));b=Shape.from_polygon(shapely.box(0,0,4,3)).r(-90.,0.,0.).translate('rel','world',10.,0.,0.)
    points=np.r_[a.emit()['vertices'],b.emit()['vertices']];offset=len(a.emit()['vertices']);other=b.geometry.polygons[0]
    combined=Shape.from_faces(points,(a.geometry.polygons[0],replace(other,outer=tuple(i+offset for i in other.outer))))
    result=combined.roof_shed(2.,mode='byHeight');product=result.emit()
    for owner in (0,1):
        rows=np.asarray(result.roof_info.source_faces)[product['parts']]==owner
        np.testing.assert_allclose(measure(product|dict(faces=product['faces'][rows])),12.,atol=1e-8)
    warped=face_shape([[0,0,0],[4,0,0],[4,0,3],[0,.2,3]],[a.geometry.polygons[0]],allow_warped=True)
    colours=Shape.from_mesh(mesh|dict(vertex_colors=np.ones((len(mesh['vertices']),4))),polygons=base.geometry.polygons)
    for unsupported in [Shape.from_mesh(mesh),colours,warped]:
        try:unsupported.roof_shed(20.)
        except (NotImplementedError,ValueError):pass
        else:raise AssertionError('Unsupported roof input was silently changed')
    roof=Shape.from_polygon(polygon).roof_shed(3.,1,'byHeight')
    children=roof.split('x',Part(Floating(1.),'left'),Part(Floating(1.),'right'))
    np.testing.assert_allclose(sum(measure(child.emit()) for child in children),measure(roof.emit()),atol=1e-8)
    for child in children:
        mesh=child.emit();tags=np.array([sum(bit for tag,bit in ROOF_TAGS.items() if tag in f.tags) for f in child.geometry.polygons],dtype=np.uint8)
        np.testing.assert_array_equal(mesh['face_tag_mask'],tags[mesh['parts']])


def benchmark():
    rows=[]
    for name,polygon in [('rectangle',shapely.box(0,0,8,6)),('courtyard',shapely.Polygon([(0,0),(10,0),(10,8),(0,8)],holes=[[(2,2),(2,6),(7,6),(7,2)]]))]:
        shape=source(polygon);shape.roof_shed(20.);times=[]
        for _ in range(21):
            start=time.perf_counter_ns();shape.roof_shed(20.);times.append((time.perf_counter_ns()-start)/1e6)
        rows.append(dict(case=name,p50_ms=float(np.percentile(times,50)),p95_ms=float(np.percentile(times,95)),scope='warm Python Shape operation including geometry compilation; excludes source creation, final render and GPU'))
    return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/modelling-roof-shed-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',cases=controls(),timings=benchmark(),limits='Analytic controls; isolated CGA rule, trim planes, exact SDK UV/normal semantics and native LOD remain unproved')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',cases=len(record['cases']),timings=record['timings'])))
