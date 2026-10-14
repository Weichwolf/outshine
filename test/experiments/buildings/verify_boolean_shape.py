"""Whole logical face coverage, native payload preservation and public components."""

import argparse
from dataclasses import replace
import json
from pathlib import Path
import time

import numpy as np
import shapely

from boolean_surface_probe import boolean
from boolean_semantic_overlay import overlay
from boolean_shape_probe import assemble
from compare_roof_oracle import cases
from evidence import revision
from outshine_ref.buildings.modelling import Frame
from outshine_ref.buildings.modelling.components import Surface
from outshine_ref.buildings.modelling.faces import plane
from outshine_ref.buildings.modelling.material import resolved_materials
from verify_boolean_attributes import authored
from verify_boolean_semantic_overlay import support_coverage,check
from verify_boolean_surfaces import Prism,controls,measure,reference,check_uv


def face_coverage(result):
    shape=replace(result.shape,scope=replace(result.shape.scope,object_frame=Frame()))
    mesh=shape.emit();surface=Surface(shape)
    if any(len(rows)!=2 for rows in surface.edge_owners.values()):
        raise ValueError('Logical faces have an open/nonmanifold edge')
    if any(surface.query(i).border for i in range(len(surface.faces))):
        raise ValueError('Closed logical face was classified as border')
    for index,face in enumerate(surface.faces):
        normal,domain,_,_=plane(mesh['vertices'],face)
        points=mesh['vertices'][list(face.outer)];axis=np.eye(3)[np.argmin(abs(normal))]
        x=np.cross(axis,normal);x/=np.linalg.norm(x);basis=np.array([x,np.cross(normal,x)])
        origin=points[0];rings=[(mesh['vertices'][list(ring)]-origin)@basis.T for ring in (face.outer,*face.holes)]
        domain=shapely.Polygon(rings[0],rings[1:]);rows=result.regions.rows[index]
        triangles=(mesh['vertices'][mesh['faces'][list(rows)]]-origin)@basis.T
        polygons=shapely.polygons(triangles);union=shapely.union_all(polygons)
        tolerance=max(1e-10,8*result.overlay.precision_m*domain.length)
        if union.symmetric_difference(domain).area>tolerance or abs(sum(p.area for p in polygons)-domain.area)>tolerance:
            raise ValueError('Native triangles do not cover whole logical face exactly once')
        for row in rows:
            bound=result.attributes
            assert (face.material,face.group,face.tags)==(int(bound.materials[row]),int(bound.groups[row]),bound.tags[row])
    return mesh


def verify(result,expected):
    mesh=face_coverage(result);native=result.overlay.product
    for key,values in native.mesh.items():
        if key=='faces':np.testing.assert_array_equal(mesh[key],values)
        else:np.testing.assert_allclose(mesh[key],values,atol=1e-10,rtol=1e-12)
    product=replace(native,mesh=mesh)
    np.testing.assert_allclose(measure(product),expected,atol=1e-7,rtol=1e-9)
    check_uv(product);support_coverage(result.overlay);check(result.overlay,result.attributes)
    for alignment in ('noAlign','zUp'):
        children=result.shape.comp('f',alignment=alignment)
        if len(children)!=len(result.regions.faces):raise ValueError('Component count differs from logical regions')
        if sum(len(child.emit()['faces']) for child in children)!=len(mesh['faces']):raise ValueError('Face selection lost native triangles')
        for index,child in enumerate(children):
            assert child.textures==result.shape.textures
            actual=child.emit();rows=list(result.regions.rows[index])
            for key in ['vertices',*['uv'+str(c) if c else 'uv' for c in range(10)]]:
                np.testing.assert_allclose(actual[key][actual['faces']],mesh[key][mesh['faces'][rows]],atol=1e-10,rtol=1e-12)
            for identifier,material in resolved_materials(child).items():
                assert material==dict(result.shape.geometry.material_defs)[identifier]
    for kind in ('g','m'):
        selected=result.shape.comp(kind,alignment='noAlign')
        if sum(len(child.emit()['faces']) for child in selected)!=len(mesh['faces']):raise ValueError('Group/material selection lost triangles')


def run(real):
    inputs=list(controls())
    if real:
        for row in cases():
            if row['control']:continue
            polygon=row['polygon'];x,y,a,b=polygon.bounds
            inputs.append((row['id'],Prism(polygon,0.,5.),Prism(shapely.box(x+(a-x)*.3,y+(b-y)*.3,x+(a-x)*.7,y+(b-y)*.7),-1.,7.)))
    rows=[]
    for name,a,b in inputs:
        for operation in ('union','subtract','intersect'):
            record=dict(id=name,operation=operation)
            try:
                result=overlay(boolean(authored(a.shape(),0),authored(b.shape(),1),operation),method='exact-output')
                start=time.perf_counter();product=assemble(result);elapsed=(time.perf_counter()-start)*1000
                verify(product,reference(a,b,operation))
                record.update(status='pass',assembly_ms=elapsed,triangles=len(result.product.mesh['faces']),
                    logical_faces=len(product.regions.faces),holes=sum(len(face.holes) for face in product.regions.faces))
            except Exception as error:record.update(status='error',reason=str(error));print(record,flush=True)
            rows.append(record)
    return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--real',action='store_true')
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/boolean-shape-verification.json'))
    args=parser.parse_args();rows=run(args.real)
    report=revision()|dict(results=rows,cases=len(rows),passed=sum(row['status']=='pass' for row in rows),
        scope='Experimental closed prism logical face/Shape reconstruction; general resource namespace, recursive ancestry, authored normals, cut-edge components and independently tilted/open Shapes pending')
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(report['passed'],'of',report['cases'],flush=True)
    raise SystemExit(0 if report['passed']==report['cases'] else 1)
