"""Compare cumulative triangle areas with repeated GEOS cuts at equal geometric accuracy."""

import argparse
import json
import os
from pathlib import Path

import numpy as np

from evidence import revision
from modelling_extrude_probe import fixtures
from modelling_group_probe import timed
from modelling_split_plan import split_plan
from modelling_split_sizes import Floating,Part


def benchmark():
    rows=[];pattern=[Part(Floating(1.),str(i)) for i in range(4)]
    for label,polygon,_,_,source_sha in fixtures():
        for axis in ('x','y'):
            for method in ('triangles','triangles-bisect','geos'):
                call=lambda:split_plan(polygon,axis,pattern,area=True,method=method)
                product=call();residual=max(abs(part.geometry.area-polygon.area/4) for part in product)
                if residual>max(1e-7,polygon.area*1e-10):raise AssertionError('Area target accuracy failed')
                rows.append(dict(label=label,axis=axis,method=method,source_sha256=source_sha,
                    source_area_m2=polygon.area,parts=len(product),max_area_error_m2=residual,**timed(call)))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/split-plan-benchmark.json'))
    args=parser.parse_args();rows=benchmark();record=revision()|dict(status='pass',measurements=rows,warm_runs=21,
        openblas_threads=os.environ.get('OPENBLAS_NUM_THREADS'),
        limits='Python 2D partitioning including area index construction, all boundary queries and final GEOS cuts; no native mesh/UV output, frames/GPU or full logical metadata memory')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',measurements=len(rows))))


if __name__=='__main__':main()
