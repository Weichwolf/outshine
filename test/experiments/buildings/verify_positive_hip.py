"""Public positive-overhang products: intrinsic frames, UV0–9, closure and volume."""

import argparse
from dataclasses import replace
import json
from pathlib import Path
import resource
import time

import numpy as np
import shapely

from compare_roof_oracle import cases
from evidence import revision
from modelling_hip_surface_probe import local
from outshine_ref.buildings.modelling import Frame
from outshine_ref.buildings.modelling.faces import ROOF_TAGS,plane
from verify_modelling_roof_hip import measure
from verify_modelling_roof_shed import source


def frame(original):
    mesh=local(original);face=original.geometry.polygons[0]
    normal=plane(mesh['vertices'],face)[0];origin=mesh['vertices'][face.outer[0]]
    vertices=mesh['vertices'][list(face.outer)]
    edges=np.roll(vertices,-1,axis=0)-vertices;tangent=edges[np.argmax(np.linalg.norm(edges,axis=1))]
    tangent-=normal*(tangent@normal);tangent/=np.linalg.norm(tangent)
    return origin,np.array([tangent,np.cross(normal,tangent)]),normal


def check_uv(original,result):
    before=local(original);after=local(result);origin,basis,_=frame(original)
    samples=np.array([[1.,0.,0.],[0.,1.,0.],[0.,0.,1.],[.2,.3,.5]])
    native=((before['vertices']-origin)@basis.T)[before['faces']]
    points=np.einsum('si,tij->tsj',samples,((after['vertices']-origin)@basis.T)[after['faces']])
    attributes=np.concatenate([before['uv'+str(c) if c else 'uv'][before['faces']] for c in range(10)],axis=2)
    actual=np.concatenate([np.einsum('si,tij->tsj',samples,after['uv'+str(c) if c else 'uv'][after['faces']]) for c in range(10)],axis=2)
    passed=np.zeros(len(points),dtype=bool)
    for corners,values in zip(native,attributes,strict=True):
        selected=np.flatnonzero(~passed)
        if not len(selected):break
        delta=points[selected]-corners[0]
        pair=np.linalg.solve(np.stack((corners[1]-corners[0],corners[2]-corners[0]),axis=1),delta.reshape(-1,2).T).T
        weights=np.c_[1.-pair.sum(axis=1),pair].reshape(-1,4,3)
        expected=weights@values
        passed[selected]=np.all(np.isclose(expected,actual[selected],atol=1e-7,rtol=1e-9),axis=(1,2))
    if not passed.all():raise ValueError('Output triangle fails affine native UV0–9 reference')
    return int(actual.size)


def check(original,result,value,mode):
    result=replace(result,scope=replace(result.scope,object_frame=Frame()))
    mesh=local(result);plan=result.roof_info.plans[0];surface=result.roof_info.exteriors[0]
    field=result.roof_info.surfaces[0] or plan;factor=field.factor(value,mode)
    expected=sum(p.polygon.area*(np.asarray(p.polygon.centroid.coords[0])@p.gradient+p.intercept_m)*factor
                 for p in field.facets)-plan.footprint.area*surface.base_level_m
    expected+=sum(p.polygon.area*(np.asarray(p.polygon.centroid.coords[0])@p.gradient+p.intercept_m-surface.base_level_m)
                  for p in surface.patches)
    actual,quality=measure(mesh)
    np.testing.assert_allclose(actual,expected,atol=1e-7,rtol=1e-9)
    origin,basis,normal=frame(original)
    top=np.asarray(['roof.top' in f.tags for f in result.geometry.polygons])[mesh['parts']]
    bottom=np.asarray(['roof.bottom' in f.tags for f in result.geometry.polygons])[mesh['parts']]
    projected=(mesh['vertices']-origin)@basis.T-plan.origin
    domain=shapely.union_all(shapely.polygons(projected[mesh['faces'][top]]))
    tolerance=surface.precision_m*surface.domain.length*4
    if domain.symmetric_difference(surface.domain).area>tolerance:raise ValueError('Roof top does not cover exterior domain')
    np.testing.assert_allclose((mesh['vertices'][mesh['faces'][bottom]]-origin)@normal,surface.base_level_m,atol=1e-8)
    active=mesh['vertices'][np.unique(mesh['faces'])]
    bounds=(active-result.scope.origin)@result.scope.axes
    np.testing.assert_allclose(bounds.min(axis=0),0.,atol=1e-8)
    np.testing.assert_allclose(bounds.max(axis=0),result.scope.size,atol=1e-8)
    np.testing.assert_allclose(result.scope.axes[:,1],normal,atol=1e-10)
    assert result.material==original.material and result.textures==original.textures and result.projections==original.projections
    assert result.geometry.material_defs==original.geometry.material_defs and result.geometry.group_names==original.geometry.group_names
    assert all(f.material==9 and f.group==3 and 'source.roof' in f.tags and len(f.tags&ROOF_TAGS.keys())==1
               for f in result.geometry.polygons)
    return dict(volume_m3=actual,triangles=len(mesh['faces']),uv_values_checked=check_uv(original,result),**quality)


def run(real):
    results=[]
    for row in cases():
        if not real and not row['control']:continue
        variants=[('identity',source(row['polygon']))]
        if row['control']:
            tilted=source(row['polygon']).r(17.,31.,23.)
            variants.append(('tilted',tilted))
            variants.append(('object-frame',replace(tilted,scope=replace(tilted.scope,object_frame=Frame((1e8,200.,-300.),(13.,17.,23.))))))
        for label,original in variants:
            before=local(original)
            for value,mode in ((3.,'byHeight'),(25.,'byAngle')):
                for even in (False,True):
                    for distance in ((.5,3.) if row['control'] else (.5,)):
                        record=dict(id=row['id'],frame=label,mode=mode,even=even,overhang_m=distance)
                        try:
                            began=time.perf_counter()
                            result=original.roof_hip(value,mode,overhang=distance,even=even)
                            elapsed=(time.perf_counter()-began)*1000
                            record.update(check(original,result,value,mode),generation_ms=elapsed,status='pass')
                            for key,val in before.items():
                                if isinstance(val,np.ndarray):np.testing.assert_array_equal(local(original)[key],val)
                            assert result.scope.object_frame==original.scope.object_frame
                        except Exception as error:
                            record.update(status='error',reason=str(error));print(record,flush=True)
                        results.append(record)
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--real',action='store_true')
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/positive-hip-verification.json'))
    args=parser.parse_args();results=run(args.real)
    report=revision()|dict(results=results,cases=len(results),passed=sum(r['status']=='pass' for r in results),
        process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024,
        scope='Public positive collar; exact oriented closure, independent field-height integral, top domain, bottom level, frame bounds, appearance and affine native UV0–9. Own bounded-miter/10cm-bottom policy; no SDK equivalence or exclusive chart proof from this verifier.')
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(report['passed'],'of',report['cases'],flush=True)
    raise SystemExit(0 if report['passed']==report['cases'] else 1)
