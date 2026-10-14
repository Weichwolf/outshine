"""Full-region semantic support, A priority, source UVs and independent solid measures."""

import argparse
from dataclasses import replace
import json
from pathlib import Path
import resource
import time

import numpy as np
import shapely

from boolean_coplanar_audit import audit
from boolean_semantic_overlay import overlay
from boolean_semantic_attributes import attributes,cut_edges
from boolean_surface_probe import boolean
from compare_roof_oracle import cases
from evidence import revision
from verify_boolean_attributes import authored,expected_material
from verify_boolean_surfaces import Prism,controls,reference,measure,check_uv


def support_coverage(result):
    product=result.product;before=[s.mesh['vertices'][s.mesh['faces']] for s in product.sources]
    total=0.
    for number,triangle in enumerate(product.mesh['vertices'][product.mesh['faces']]):
        supports=result.supports[number]
        preferred=before[supports[0][0]][supports[0][1]]
        normal=np.cross(preferred[1]-preferred[0],preferred[2]-preferred[0]);normal/=np.linalg.norm(normal)
        if np.any(abs((triangle-preferred[0])@normal)>result.precision_m*8):
            raise ValueError('Output vertices left their authoritative source plane')
        axis=int(np.argmax(abs(normal)));axes=[i for i in range(3) if i!=axis]
        domain=shapely.set_precision(shapely.Polygon(triangle[:,axes]),result.precision_m)
        tolerance=8*result.precision_m*domain.length
        for owner,index in supports:
            corners=before[owner][index]
            if np.any(abs((corners-preferred[0])@normal)>result.precision_m*8):raise ValueError('Declared support left the source plane')
            candidate=shapely.set_precision(shapely.Polygon(corners[:,axes]),result.precision_m)
            if domain.difference(candidate).area>tolerance:raise ValueError('Declared source triangle does not support entire semantic triangle')
        owner=int(product.operands[number]);other=before[1-owner]
        selected=np.all(abs((other-preferred[0])@normal)<=result.precision_m*8,axis=1)
        projected=shapely.set_precision(shapely.polygons(other[selected][:,:,axes]),result.precision_m)
        covered=domain.intersection(shapely.union_all(projected)) if selected.any() else shapely.Polygon()
        if len(supports)==2:
            if domain.area-covered.area>tolerance:raise ValueError('Dual tag extends beyond double support')
            if owner!=0 or supports[0][0]!=0:raise ValueError('Double support did not select operand A')
            total+=domain.area/abs(normal[axis])
        elif covered.area>tolerance:raise ValueError('Single tag triangle contains unmarked dual area')
    return total


def check(result,bound):
    product=result.product
    for i,supports in enumerate(result.supports):
        owner=supports[0][0]
        assert dict(bound.material_bindings[int(bound.materials[i])].material.attributes)==expected_material(owner)
        for origin,index in supports:
            source=product.sources[origin];part=int(source.mesh['parts'][index])
            assert source.shape.geometry.polygons[part].tags<=bound.tags[i]
            assert ('bool.A' if origin==0 else 'bool.B') in bound.tags[i]
        assert (('bool.A' in bound.tags[i]),('bool.B' in bound.tags[i]))==(any(o==0 for o,_ in supports),any(o==1 for o,_ in supports))


def run(real,method):
    rows=list(controls())
    if real:
        for row in cases():
            if row['control']:continue
            polygon=row['polygon'];x,y,a,b=polygon.bounds
            rows.append((row['id'],Prism(polygon,0.,5.),Prism(shapely.box(x+(a-x)*.3,y+(b-y)*.3,x+(a-x)*.7,y+(b-y)*.7),-1.,7.)))
    results=[]
    for name,a,b in rows:
        for operation in ['union','subtract','intersect']:
            record=dict(id=name,operation=operation)
            try:
                native=boolean(authored(a.shape(),0),authored(b.shape(),1),operation)
                original={k:v.copy() for k,v in native.mesh.items()}
                started=time.perf_counter();result=overlay(native,method=method);elapsed=(time.perf_counter()-started)*1000
                np.testing.assert_allclose(measure(result.product),reference(a,b,operation),atol=1e-7,rtol=1e-9)
                check_uv(result.product);dual=support_coverage(result);bound=attributes(result);check(result,bound)
                edges=cut_edges(result)
                if name=='coplanar-overlap' and operation=='union':
                    np.testing.assert_allclose(dual,60.,atol=1e-7,rtol=1e-9)
                    assert len(edges)>0
                if name=='identical' and operation in ('union','intersect'):
                    np.testing.assert_allclose(dual,392.,atol=1e-7,rtol=1e-9)
                    assert len(edges)==0
                for key,value in original.items():np.testing.assert_array_equal(native.mesh[key],value)
                record.update(status='pass',overlay_ms=elapsed,triangles=len(result.product.mesh['faces']),
                    input_triangles=len(native.mesh['faces']),added_roots=result.added_roots,dual_area_m2=dual,cut_edges=len(edges))
            except Exception as error:
                record.update(status='error',reason=str(error));print(record,flush=True)
            results.append(record)
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--real',action='store_true')
    parser.add_argument('--method',choices=['triangle','plane'],default='triangle')
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/boolean-semantic-verification.json'))
    args=parser.parse_args();results=run(args.real,args.method)
    report=revision()|dict(results=results,cases=len(results),passed=sum(r['status']=='pass' for r in results),method=args.method,
        process_peak_rss_mib=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss/1024,
        scope='Experimental closed prism semantic overlay: complete source-support regions/A priority/both operand tags/derived intersection edges; no SDK exact connectivity/UV equivalence or general Shape/open/rotated/lineage/resource namespace approval.')
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(report['passed'],'of',report['cases'],flush=True)
    raise SystemExit(0 if report['passed']==report['cases'] else 1)
