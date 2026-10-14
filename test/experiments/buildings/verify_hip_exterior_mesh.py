"""Actual exterior meshes: exact oriented closure, UVs and height-integral volumes."""

import argparse
import json
import time
from pathlib import Path

import numpy as np

from compare_roof_oracle import cases
from evidence import revision
from hip_exterior_mesh_probe import emit
from verify_modelling_roof_hip import measure
from verify_modelling_roof_shed import source


def emitted_uv(original,result):
    """Check output corners and centroids against independent affine native charts."""
    before=original.emit();after=result.emit()
    samples=np.array([[1.,0.,0.],[0.,1.,0.],[0.,0.,1.],[.2,.3,.5]])
    anchor=before['vertices'][before['faces'][0,0]][[0,2]]
    native=(before['vertices'][:,[0,2]]-anchor)[before['faces']]
    points=np.einsum('si,tij->tsj',samples,(after['vertices'][:,[0,2]]-anchor)[after['faces']])
    original_uv=np.concatenate([before['uv'+str(c) if c else 'uv'][before['faces']] for c in range(10)],axis=2)
    actual=np.concatenate([np.einsum('si,tij->tsj',samples,after['uv'+str(c) if c else 'uv'][after['faces']]) for c in range(10)],axis=2)
    passed=np.zeros(len(points),dtype=bool)
    for corners,attributes in zip(native,original_uv,strict=True):
        selected=np.flatnonzero(~passed)
        if not len(selected):break
        first,last=corners[1:]-corners[0];det=first[0]*last[1]-first[1]*last[0]
        if det==0:raise ValueError('Native UV reference triangle is degenerate')
        delta=points[selected]-corners[0]
        u=(delta[...,0]*last[1]-delta[...,1]*last[0])/det
        v=(first[0]*delta[...,1]-first[1]*delta[...,0])/det
        expected=np.stack([1-u-v,u,v],axis=2)@attributes
        passed[selected]=np.all(np.isclose(expected,actual[selected],atol=1e-7,rtol=1e-9),axis=(1,2))
    if not passed.all():raise ValueError('Emitted UV triangle does not follow one affine native chart')
    return int(actual.size)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--method',choices=['envelope','collar'],default='envelope')
    parser.add_argument('--output',type=Path,default=None)
    args=parser.parse_args()
    results=[]
    for row in cases():
        for value,mode in ((3.,'byHeight'),(25.,'byAngle')):
            for even in (False,True):
                for overhang in ((.5,3.) if row['control'] else (.5,)):
                    try:
                        started=time.perf_counter()
                        original=source(row['polygon'])
                        proof=emit(original,row['polygon'],value,mode,overhang,even,method=args.method)
                        generation_ms=(time.perf_counter()-started)*1000
                        volume,quality=measure(proof.shape.emit())
                        np.testing.assert_allclose(volume,proof.expected_volume_m3,atol=1e-7,rtol=1e-9)
                        result=dict(id=row['id'],mode=mode,even=even,overhang_m=overhang,status='pass',volume_m3=volume,
                                    cells=proof.cells,triangles=len(proof.shape.emit()['faces']),uv_values_checked=proof.uv_values_checked,
                                    maximum_shared_height_adjustment_m=proof.maximum_shared_height_adjustment_m,**quality)
                        result['generation_ms']=generation_ms
                        result['emitted_uv_values_checked']=emitted_uv(original,proof.shape)
                    except Exception as error:
                        result=dict(id=row['id'],mode=mode,even=even,overhang_m=overhang,status='error',reason=str(error));print(result,flush=True)
                    results.append(result)
    report=revision()|dict(results=results,passed=sum(r['status']=='pass' for r in results),cases=len(results),
        method=args.method,shell_depth_m=.1,
        scope='Experimental horizontal Source Shapes, own positive exterior/closed bottom 10 cm below the lowest eave. UV0-9 affine fits before and after output (corners/centroids), exact closure and independent polygon field-height volume integral; no arbitrary frames or SDK equivalence or proof of exclusive native-chart ownership.')
    output=args.output or Path('build/experiments/buildings/hip-exterior-'+args.method+'-mesh-verification.json')
    output.write_text(json.dumps(report,indent=2)+'\n')
    print(report['passed'],'of',report['cases'])
    raise SystemExit(0 if report['passed']==report['cases'] else 1)
