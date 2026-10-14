"""Compare finite Python execution and isolated native calls on pinned geometry."""

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np
import shapely

from evidence import revision
from outshine_ref.buildings.modelling import prepare_hip


HANGER=[(0.,0.),(-.45231913453437711,.45155481886627058),
    (-.90463826906875378,-.45155484650368294),(-2.2615956726718842,.45155481886627058),
    (3.1662339417406367,9.9341996371328207),(-7.6894252870844051,15.804402235010485),
    (-12.255186343214582,7.1026908842675827),(-.41524366068275098,-.7772666189143429)]


def child(method):
    request=json.loads(sys.stdin.read());polygon=shapely.Polygon(request['outer'],request['holes'])
    print(json.dumps(dict(status='entered',pid=__import__('os').getpid())),flush=True)
    try:
        plan=prepare_hip(polygon,method)
        result=dict(status='pass',method=plan.method,facets=len(plan.facets),peak_m=float(plan.distances_m.max()))
        if 'expected' in request:
            expected=request['expected'];points=np.asarray(expected['positions_local_xy_m'])+request['frame']['origin_xy_m']
            facets={tuple(f['source_edge']):shapely.Polygon(points[f['nodes']]) for f in expected['facets']}
            if set(facets)!={f.boundary for f in plan.facets}:raise ValueError('Fixture eave ownership mismatch')
            error=max(shapely.hausdorff_distance(shapely.Polygon(np.asarray(f.polygon.exterior.coords)+plan.origin),facets[f.boundary]) for f in plan.facets)
            peak=abs(plan.distances_m.max()-max(expected['wavefront_distances_m']))
            result.update(maximum_facet_hausdorff_m=float(error),peak_error_m=float(peak))
            if max(error,peak)>5e-5:result['status']='mismatch'
    except Exception as error:result=dict(status='error',kind=type(error).__name__,reason=str(error))
    print(json.dumps(result),flush=True)


def run(row,method,timeout):
    started=time.monotonic()
    try:
        p=subprocess.run([sys.executable,str(Path(__file__).resolve()),'--child',method],
            input=json.dumps(row),text=True,capture_output=True,timeout=timeout)
        result=dict(status='terminal',exit_code=p.returncode,output=p.stdout[-1000:],stderr=p.stderr[-1000:])
    except subprocess.TimeoutExpired as error:
        result=dict(status='timeout',output=(error.stdout or b'').decode()[-1000:])
    return dict(id=row['id'],requested_method=method,elapsed_ms=(time.monotonic()-started)*1000,**result)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--child',choices=['auto','skeleton'])
    parser.add_argument('--timeout',type=float,default=3.);args=parser.parse_args()
    if args.child:child(args.child)
    else:
        fixture=json.loads((Path(__file__).parent/'reference/hip-plans.json').read_text())
        rows=[dict(id='CentralPark:near-touching-native-hanger',outer=HANGER,holes=[])]
        results=[run(rows[0],method,args.timeout) for method in ['auto','skeleton']]
        for row in fixture['records']:results.append(run(row,'skeleton',args.timeout))
        record=revision()|dict(results=results,timeout_s=args.timeout,scope='Fresh subprocess per case, startup included; returncode/timeout/child result all retained, no production worker API')
        Path('build/experiments/buildings/hip-execution-probe.json').write_text(json.dumps(record,indent=2)+'\n')
        print(json.dumps(results[:2],indent=2));print('Python errors',sum('"status": "error"' in r.get('output','') for r in results[2:]))
