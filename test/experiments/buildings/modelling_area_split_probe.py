"""Compare complete native surface cuts using analytic and iterative area inversion."""

import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import platform
import time
from unittest.mock import patch

import numpy as np

from evidence import revision
from modelling_extrude_probe import fixtures
from outshine_ref.buildings.modelling.faces import Face
from outshine_ref.buildings.modelling.shape import Shape
from outshine_ref.buildings.modelling.split_area import TriangleArea,inverse_area
from outshine_ref.buildings.modelling.split_sizes import Floating,Part
from verify_modelling_area_split import area
from verify_modelling_split import source_attributes


def surfaces():
    for label,polygon,elevation,_,source_sha in fixtures():
        yield label,Shape.from_polygon(polygon,elevation,material=7,tags=(label,)),('x','z'),source_sha
    facade=Shape.from_faces([[0.,0.,0.],[6.,0.,0.],[0.,8.,0.]],[Face((0,1,2),material=7)])
    yield 'triangular-facade',facade,('x','y'),None
    bent=Shape.from_faces([[0.,0.,0.],[4.,0.,0.],[0.,0.,4.],[4.,.6,4.]],
                          [Face((0,1,2),material=7),Face((1,3,2),material=7)])
    yield 'almost-planar',bent,('x','z'),None


def alternative(method):
    return patch.object(TriangleArea,'inverse',inverse_area) if method=='bisect' else nullcontext()


def paired_times(call):
    samples={'analytic':[],'bisect':[]}
    for method in samples:
        with alternative(method):call()
    for index in range(21):
        methods=('analytic','bisect') if index%2==0 else ('bisect','analytic')
        for method in methods:
            with alternative(method):
                start=time.perf_counter_ns();call();elapsed=(time.perf_counter_ns()-start)/1e6
            samples[method].append(elapsed)
    return {method:dict(p50_ms=float(np.percentile(values,50)),p95_ms=float(np.percentile(values,95)))
            for method,values in samples.items()}


def benchmark():
    rows=[];pattern=[Part(Floating(1.),str(i)) for i in range(4)]
    for label,source,axes,source_sha in surfaces():
        for axis in axes:
            call=lambda:source.split_area(axis,*pattern)
            costs=paired_times(call)
            for method in ('analytic','bisect'):
                with alternative(method):
                    children=call();residual=max(abs(area(child)-area(source)/4) for child in children)
                    if residual>max(1e-7,area(source)*1e-10):raise AssertionError('Surface area target accuracy failed')
                    corners=sum(source_attributes(source,child) for child in children)
                    rows.append(dict(label=label,axis=axis,method=method,source_sha256=source_sha,
                        source_area_m2=area(source),parts=len(children),max_area_error_m2=residual,
                        source_corners_checked=corners,triangles=sum(len(child.emit()['faces']) for child in children),
                        logical_faces=sum(len(child.geometry.polygons) for child in children),**costs[method]))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/area-split-benchmark.json'))
    args=parser.parse_args();rows=benchmark()
    record=revision()|dict(status='pass',measurements=rows,warm_runs=21,openblas_threads=os.environ.get('OPENBLAS_NUM_THREADS'),
        host=platform.platform(),sample_order='Alternating analytic/bisect pairs; method override outside timed region',
        limits='Complete Python surface preparation, area index, boundary inversion and attributed native/logical cuts. Final emitted positions, frames/GPU and complete metadata memory excluded.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',measurements=len(rows))))


if __name__=='__main__':main()
