"""Same verified immutable roof plans, direct versus persistent isolated execution."""

import json
from pathlib import Path
import time

import numpy as np
import shapely

from evidence import revision
from outshine_ref.buildings.modelling import HipPlanner,prepare_hip
from verify_hip_fixture import verify


def memory(pid):
    p=Path('/proc')/str(pid)/'smaps_rollup'
    if not p.is_file():return None
    values={}
    for line in p.read_text().splitlines():
        key,*parts=line.split()
        if key in ['Pss:','Rss:']:values[key[:-1]+'_mib']=int(parts[0])/1024
    return values


def benchmark():
    import os
    import verify_hip_fixture as checker
    data=json.loads((Path(__file__).parent/'reference/hip-plans.json').read_text())
    result=[];started=time.monotonic()
    with HipPlanner() as planner:
        startup_ms=(time.monotonic()-started)*1000
        for method,call in [('direct',prepare_hip),('isolated',planner.prepare)]:
            original=checker.prepare_hip;checker.prepare_hip=call
            try:
                for row in data['records']:verify(row,data['comparison'])
            finally:checker.prepare_hip=original
            for row in data['records']:
                polygon=shapely.Polygon(row['outer'],row['holes']);samples=[]
                for _ in range(5):
                    started=time.monotonic();plan=call(polygon);samples.append((time.monotonic()-started)*1000)
                result.append(dict(id=row['id'],execution=method,plan_method=plan.method,samples_ms=samples))
        parent=memory(os.getpid());child=memory(planner.pid)
    return revision()|dict(results=result,startup_ms=startup_ms,parent_memory=parent,child_memory=child,
        aggregate_sampled_pss_mib=None if parent is None or child is None else parent['Pss_mib']+child['Pss_mib'],
        scope='Same unchanged 48-source geometry fixture verified before five warm sequential host samples per source/mode; IPC/immutable decode included, startup separate. PSS/RSS sampled after jobs incl imports, not peak/device/frame proof.')


if __name__=='__main__':
    report=benchmark();Path('build/experiments/buildings/hip-worker-benchmark.json').write_text(json.dumps(report,indent=2)+'\n')
    for method in ['direct','isolated']:
        print(method,np.percentile([x for r in report['results'] if r['execution']==method for x in r['samples_ms']],[50,95]))
