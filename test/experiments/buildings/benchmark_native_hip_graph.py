"""Same pinned sources and quality: former facet-first auto versus courtyard graph auto."""

import argparse
import json
from pathlib import Path
import resource
import sys
import time

import numpy as np
import shapely

from evidence import revision
from outshine_ref.buildings.modelling import skeleton_plan as core
from outshine_ref.buildings.modelling.skeleton_native import partition
from verify_hip_fixture import verify
from verify_native_hip_graph import FIXTURE


def former_native_data(local,rings,boundaries,limits):
    nodes,faces=partition(local,rings,limits.nodes)
    owners=core.face_boundaries(nodes,faces,boundaries);incident=[[] for _ in nodes]
    for face,(_,gradient,intercept) in zip(faces,owners,strict=True):
        for index in face:incident[index].append(nodes[index]@gradient+intercept)
    original={tuple(point) for ring in rings for point in ring}
    distances=np.array([0. if tuple(point) in original else np.mean(heights)
                       for point,heights in zip(nodes,incident,strict=True)])
    return (nodes,distances,faces,None),owners


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--method',choices=['former-auto','graph-auto'],required=True)
    parser.add_argument('--repetitions',type=int,default=5)
    args=parser.parse_args()
    if args.repetitions<1:parser.error('Repetitions must be positive')
    data=json.loads(FIXTURE.read_text());native=core.native_data
    if args.method=='former-auto':core.native_data=former_native_data
    try:
        for row in data['records']:verify(row,data['comparison'])
        results=[]
        for row in data['records']:
            polygon=shapely.Polygon(row['outer'],row['holes']);times=[]
            for _ in range(args.repetitions):
                started=time.perf_counter();plan=core.prepare_hip(polygon);times.append((time.perf_counter()-started)*1000)
            results.append(dict(id=row['id'],method=plan.method,holes=len(row['holes']),samples_ms=times,
                p50_ms=float(np.median(times)),maximum_refinement_m=plan.maximum_refinement_m))
        peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        record=revision()|dict(method=args.method,repetitions=args.repetitions,results=results,
            peak_rss_mib=peak/(1048576 if sys.platform=='darwin' else 1024),
            memory_scope='Fresh sequential host process high-water RSS incl. imports, fixture check and planning; no per-asset delta/GPU/device claim',
            scope='Same unchanged 48-source fixture and strict library geometry contracts verified before timed samples; plans only, imports warm, no mesh emission/frame time')
        Path('build/experiments/buildings/native-hip-'+args.method+'-benchmark.json').write_text(json.dumps(record,indent=2)+'\n')
        print(args.method,'native',sum(r['method']=='native' for r in results),'of',len(results),'peak RSS MiB',record['peak_rss_mib'])
    finally:core.native_data=native
