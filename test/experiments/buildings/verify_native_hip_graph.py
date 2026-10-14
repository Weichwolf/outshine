"""Unchanged real fixture: selective native exports, complete eaves and metric fields."""

import json
from pathlib import Path
import time

import numpy as np
import shapely

from evidence import revision
from native_hip_graph_probe import graph_data
from outshine_ref.buildings.modelling.skeleton_plan import (
    SkeletonLimits,finish,input_data,native_data,face_boundaries,
)


FIXTURE=Path(__file__).parent/'reference/hip-plans.json'


def prepare_selective(polygon,limits=SkeletonLimits()):
    began=time.perf_counter();origin,local,rings,boundaries=input_data(polygon,limits)
    if local.interiors:
        data,metadata=graph_data(local,rings,limits.nodes)
        owners=face_boundaries(data[0],data[2],boundaries)
        export='measured-graph'
    else:
        data,owners=native_data(local,rings,boundaries,limits);metadata={};export='facets'
    return finish(origin,local,boundaries,data,'native',limits,began,owners=owners),metadata|dict(native_export=export)


def compare(row,comparison):
    source=shapely.Polygon(row['outer'],row['holes']);before=source.wkb;started=time.perf_counter()
    plan,metadata=prepare_selective(source);elapsed=(time.perf_counter()-started)*1000
    expected=row['expected'];points=np.asarray(expected['positions_local_xy_m'])+row['frame']['origin_xy_m']
    old={tuple(f['source_edge']):shapely.Polygon(points[f['nodes']]) for f in expected['facets']}
    assert set(old)=={f.boundary for f in plan.facets}
    error=max(shapely.hausdorff_distance(shapely.Polygon(np.asarray(f.polygon.exterior.coords)+plan.origin),old[f.boundary]) for f in plan.facets)
    assert error<=comparison['position_tolerance_m'],error
    peak_error=abs(plan.distances_m.max()-max(expected['wavefront_distances_m']))
    assert peak_error<=comparison['height_tolerance_m'],peak_error
    np.testing.assert_array_equal(source.wkb,before)
    return dict(id=row['id'],status='pass',generation_ms=elapsed,facets=len(plan.facets),
        maximum_facet_hausdorff_m=float(error),maximum_peak_wavefront_error_m=float(peak_error),
        stored_fixture_method=expected['method'],**metadata)


if __name__=='__main__':
    data=json.loads(FIXTURE.read_text());results=[]
    for row in data['records']:
        try:result=compare(row,data['comparison'])
        except Exception as error:result=dict(id=row['id'],status='error',reason=str(error));print(result,flush=True)
        results.append(result)
    record=revision()|dict(results=results,passed=sum(r['status']=='pass' for r in results),cases=len(results),
        scope='Native-only measured graph for holes, facet export otherwise. Unchanged fixture eaves/geometry/height, no Python fallback, hard timeout or cross-host proof.')
    Path('build/experiments/buildings/native-hip-graph-verification.json').write_text(json.dumps(record,indent=2)+'\n')
    print(record['passed'],'of',record['cases'])
    raise SystemExit(0 if record['passed']==record['cases'] else 1)
