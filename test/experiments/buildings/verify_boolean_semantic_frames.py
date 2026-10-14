"""Common remote placement and independently yawed prism frames with slab references."""

from dataclasses import replace
import json
from pathlib import Path
import time

import numpy as np
from shapely import affinity

from boolean_surface_probe import boolean
from boolean_semantic_overlay import overlay
from boolean_semantic_attributes import attributes,cut_edges
from evidence import revision
from outshine_ref.buildings.modelling import Frame
from verify_boolean_attributes import authored
from verify_boolean_semantic_overlay import support_coverage,check
from verify_boolean_surfaces import Prism,controls,reference,measure,check_uv


def place(shape,frame):
    return replace(shape,scope=replace(shape.scope,object_frame=frame))


def verify(result,expected):
    np.testing.assert_allclose(measure(result.product),expected,atol=1e-7,rtol=1e-9)
    check_uv(result.product);support_coverage(result);check(result,attributes(result));cut_edges(result)


def run():
    results=[];distant=Frame((1e8,200.,-300.),(17.,31.,23.));yaw=Frame((2.,0.,-3.),(0.,37.,0.))
    names={'coplanar-overlap','thin-passage','courtyard','nested','circular-tower'}
    for name,a,b in controls():
        if name not in names:continue
        for operation in ('union','subtract','intersect'):
            shapes=(authored(a.shape(),0),authored(b.shape(),1))
            baseline=None
            for mode in ('local','remote-common','independent-yaw'):
                record=dict(id=name,operation=operation,mode=mode)
                try:
                    first,last=shapes
                    expected=reference(a,b,operation)
                    if mode=='remote-common':first,last=(place(s,distant) for s in shapes)
                    if mode=='independent-yaw':
                        last=place(last,yaw)
                        polygon=affinity.translate(affinity.rotate(b.polygon,37.,origin=(0.,0.)),xoff=2.,yoff=3.)
                        expected=reference(a,Prism(polygon,b.low,b.high),operation)
                    start=time.perf_counter();result=overlay(boolean(first,last,operation),method='exact-output')
                    elapsed=(time.perf_counter()-start)*1000;verify(result,expected)
                    if mode=='local':baseline=result
                    if mode=='remote-common':
                        assert result.product.object_frame==distant and baseline is not None
                        for key,value in baseline.product.mesh.items():np.testing.assert_array_equal(result.product.mesh[key],value)
                        np.testing.assert_array_equal(result.product.roots,baseline.product.roots)
                        assert result.supports==baseline.supports
                    record.update(status='pass',pipeline_ms=elapsed,triangles=len(result.product.mesh['faces']))
                except Exception as error:record.update(status='error',reason=str(error))
                results.append(record)
    return results


if __name__=='__main__':
    rows=run();report=revision()|dict(cases=len(rows),passed=sum(r['status']=='pass' for r in rows),results=rows,
        scope='Five closed prism controls with common arbitrary remote frame and independently yawed B; not general independently tilted/open Shape coverage')
    path=Path('build/experiments/buildings/semantic-output-frames.json');path.write_text(json.dumps(report,indent=2)+'\n')
    print(report['passed'],'of',report['cases'],flush=True)
    for row in rows:
        if row['status']!='pass':print(row,flush=True)
    raise SystemExit(0 if report['passed']==report['cases'] else 1)
