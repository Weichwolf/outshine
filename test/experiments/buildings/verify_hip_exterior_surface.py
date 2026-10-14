"""Measure the own positive exterior field before full Shape/UV integration."""

import json
from pathlib import Path

import numpy as np
import shapely

from compare_roof_oracle import cases
from evidence import revision
from hip_exterior_surface_probe import prepare
from outshine_ref.buildings.modelling import prepare_hip


def verify(row,value,mode,even,overhang):
    plan=prepare_hip(row['polygon']);positions=plan.positions_m.copy();distances=plan.distances_m.copy()
    surface=prepare(plan,value,mode,overhang,even)
    polygons=[p.polygon for p in surface.patches]
    union=shapely.union_all(polygons,grid_size=surface.precision_m)
    tolerance=max(1e-7,plan.footprint.length*np.sqrt(2)*surface.precision_m)
    overlap=sum(p.area for p in polygons)-union.area
    original_overlap=union.intersection(shapely.set_precision(plan.footprint,surface.precision_m)).area
    assert overlap<=tolerance and original_overlap<=tolerance,(overlap,original_overlap,tolerance)
    lowest=float('inf');highest=-float('inf')
    for patch in surface.patches:
        xy=np.asarray(patch.polygon.exterior.coords);z=xy@patch.gradient+patch.intercept_m
        lowest=min(lowest,float(z.min()));highest=max(highest,float(z.max()))
    assert lowest>=surface.base_level_m-1e-7 and highest<=1e-7,(lowest,highest,surface.base_level_m)
    np.testing.assert_array_equal(plan.positions_m,positions);np.testing.assert_array_equal(plan.distances_m,distances)
    if row['id']=='rectangle' and not even:
        horizontal=overhang/np.hypot(1.,plan.factor(value,mode))
        expected=plan.footprint.buffer(horizontal,join_style='mitre')
        assert expected.symmetric_difference(surface.domain).area<=tolerance
    return dict(id=row['id'],value=value,mode=mode,even=even,overhang_m=overhang,
                patches=len(polygons),candidate_pairs=surface.candidate_pairs,precision_m=surface.precision_m,
                overlap_area_m2=overlap,interior_overlap_area_m2=original_overlap,base_level_m=surface.base_level_m,
                minimum_exterior_height_m=lowest,maximum_exterior_height_m=highest,collapsed_triangles=surface.collapsed_triangles)


if __name__=='__main__':
    results=[]
    for row in cases():
        for value,mode in ((3.,'byHeight'),(25.,'byAngle')):
            for even in (False,True):
                for overhang in ((.5,3.) if row['control'] else (.5,)):
                    try:result=verify(row,value,mode,even,overhang)|dict(status='pass')
                    except Exception as error:result=dict(id=row['id'],mode=mode,even=even,overhang_m=overhang,status='error',reason=str(error));print(result,flush=True)
                    results.append(result)
    record=revision()|dict(results=results,passed=sum(r['status']=='pass' for r in results),cases=len(results),
        scope='Own exterior strip upper-envelope validity and original-field preservation. No full shared-edge Shape/UV/bottom closure or SDK exterior/tessellation equivalence.')
    Path('build/experiments/buildings/hip-exterior-surface-verification.json').write_text(json.dumps(record,indent=2)+'\n')
    print(record['passed'],'of',record['cases'],flush=True)
