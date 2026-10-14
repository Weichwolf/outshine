"""Measure coplanar dual support; mixed triangles require an actual semantic overlay."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from boolean_surface_probe import boolean
from evidence import revision
from verify_boolean_surfaces import controls


def audit(product,max_candidates=1048576):
    if type(max_candidates)is not int or max_candidates<=0:raise ValueError('Invalid coplanar audit capacity')
    sources=[source.mesh['vertices'][source.mesh['faces']] for source in product.sources]
    total=sum(np.count_nonzero(product.operands==owner)*len(sources[1-owner]) for owner in (0,1))
    if total>max_candidates:raise ValueError('Coplanar audit candidate capacity exceeded')
    records=[]
    for index,triangle in enumerate(product.mesh['vertices'][product.mesh['faces']]):
        owner=int(product.operands[index]);other=sources[1-owner]
        normal=np.cross(triangle[1]-triangle[0],triangle[2]-triangle[0]);normal/=np.linalg.norm(normal)
        precision=max(1e-9,64*np.finfo(float).eps*max(1.,float(abs(triangle).max())))
        selected=np.all(abs((other-triangle[0])@normal)<=precision,axis=1)
        if not selected.any():continue
        axis=int(np.argmax(abs(normal)));axes=[i for i in range(3) if i!=axis]
        domain=shapely.Polygon(triangle[:,axes])
        projection=shapely.polygons(other[selected][:,:,axes])
        covered=domain.intersection(shapely.union_all(projection));area=covered.area
        uncertainty=4*precision*domain.length
        if area<=uncertainty:continue
        kind='full' if domain.area-area<=uncertainty else 'partial'
        records.append(dict(triangle=index,native_owner=owner,other_owner=1-owner,kind=kind,
            dual_area_m2=area/abs(normal[axis]),triangle_area_m2=domain.area/abs(normal[axis]),precision_m=precision))
    return dict(candidate_pairs=int(total),records=records,full=sum(r['kind']=='full' for r in records),
                partial=sum(r['kind']=='partial' for r in records),semantic_assignment_complete=False)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/boolean-coplanar-audit.json'))
    args=parser.parse_args();rows=[]
    for name,a,b in controls():
        if name not in ('identical','coplanar-overlap','face-contact'):continue
        for operation in ['union','subtract','intersect']:
            rows.append(dict(id=name,operation=operation,**audit(boolean(a.shape(),b.shape(),operation))))
    mixed=next(r for r in rows if r['id']=='coplanar-overlap' and r['operation']=='union')
    if not mixed['partial'] or not np.isclose(sum(r['dual_area_m2'] for r in mixed['records']),60.,atol=1e-8):
        raise ValueError('Coplanar control must cover two 30m² overlap regions and contain mixed triangles')
    args.output.write_text(json.dumps(revision()|dict(results=rows,scope='Diagnostic support regions only; no centroid tag/material assignment or complete Boolean semantic claim'),indent=2)+'\n')
    print('Mixed coplanar union:',mixed['partial'],'partial and',mixed['full'],'full triangles',flush=True)
