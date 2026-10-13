"""Compare signed offset plans with area identities, topology controls and the pinned Esri helper."""

import argparse
import json
from pathlib import Path
import time

import numpy as np
import shapely

from esri_offset_oracle import reference
from evidence import revision
from outshine_ref.buildings.modelling.offset_plan import offset


def cases():
    return [('rectangle',shapely.box(0,0,10,8),1.),
        ('wing',shapely.Polygon([(0,0),(10,0),(10,4),(4,4),(4,10),(0,10)]),.5),
        ('acute',shapely.Polygon([(0,0),(10,0),(0,1)]),.1),
        ('split',shapely.Polygon([(0,0),(5,0),(5,2),(7,2),(7,0),(13,0),(13,6),(7,6),(7,3),(5,3),(5,5),(0,5)]),.6),
        ('collapsed',shapely.box(0,0,10,8),5.)]


def controls():
    rows=[]
    for name,polygon,distance in cases():
        for value in (-distance,0.,distance):
            plan=offset(polygon,value)
            signed=sum(row.polygon.area*row.normal_sign for row in plan.regions)
            assert abs(signed-polygon.area)<1e-7
            assert plan.occluder is None
            for kind in ('inside','border','all'):
                selected=offset(polygon,value,kind)
                assert selected.occluder.equals(polygon)
                assert all(kind=='all' or row.kind==kind for row in selected.regions)
            for row in plan.regions:
                assert row.polygon.is_valid and shapely.is_ccw(row.polygon.exterior)==(row.normal_sign>0)
        inside=offset(polygon,-distance,'inside');sdk=reference(polygon,distance)
        xyz=np.asarray(sdk['vertices'],dtype=float).reshape(-1,3)
        actual=shapely.Polygon(xyz[:,[0,2]]) if len(xyz) else shapely.Polygon()
        expected=max((row.polygon for row in inside.regions),key=lambda part:part.area,default=shapely.Polygon())
        error=actual.symmetric_difference(expected).area
        assert error<2e-5,(name,error)
        if name=='split':assert len(inside.regions)==2 and len(xyz)>0
        if name=='collapsed':assert not inside.regions and len(xyz)==0 and inside.bounds is None
        elapsed=[]
        for iteration in range(22):
            started=time.perf_counter();offset(polygon,-distance,'inside')
            if iteration:elapsed.append((time.perf_counter()-started)*1000)
        rows.append(dict(case=name,components=len(inside.regions),sdk_difference_m2=error,oracle=sdk,
            warm_plan_p50_ms=float(np.median(elapsed)),warm_plan_p95_ms=float(np.quantile(elapsed,.95)),repetitions=21))
    courtyard=shapely.Polygon([(0,0),(20,0),(20,20),(0,20)],holes=[[(8,8),(8,12),(12,12),(12,8)]])
    result=offset(courtyard,-1.,'inside');assert len(result.regions)==1
    assert abs(result.regions[0].polygon.area-(18*18-6*6))<1e-9
    expanded=offset(shapely.box(0,0,10,8),1.)
    assert sum(row.polygon.area for row in expanded.regions if row.kind=='inside')==120
    assert sum(row.polygon.area*row.normal_sign for row in expanded.regions if row.kind=='border')==-40
    for distance,keep in ((float('nan'),None),(float('inf'),None),(True,None),(1.,'unknown')):
        try:offset(courtyard,distance,keep)
        except ValueError:pass
        else:raise AssertionError('Invalid offset request accepted')
    return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/offset-plan-verification.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',cases=controls(),
        limits='Planar geometry only; SDK helper compares largest inward simple-ring component, not full CGA. Shape/UV/normal/tag propagation, border subdivision and runtime occlusion queries pending')
    args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',sdk_cases=len(record['cases']))))
