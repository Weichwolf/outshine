"""Measure native wire extraction and compare stable unique-edge indexing alternatives."""

import argparse
import json
import os
from pathlib import Path
import time

import numpy as np

from evidence import revision
from modelling_components import Surface
from modelling_extrude_probe import fixtures
from modelling_shape import Shape


def dictionary_edges(edges):
    seen=set();indices=[]
    for index,(a,b) in enumerate(edges):
        key=(min(a,b),max(a,b))
        if key not in seen:seen.add(key);indices.append(index)
    return np.asarray(indices,dtype=np.int64)


def numpy_edges(edges):
    _,indices=np.unique(np.sort(edges,axis=1),axis=0,return_index=True)
    return np.sort(indices)


def timed(call):
    call();times=[]
    for _ in range(21):
        began=time.perf_counter_ns();call();times.append((time.perf_counter_ns()-began)/1e6)
    return dict(p50_ms=float(np.percentile(times,50)),p95_ms=float(np.percentile(times,95)))


def benchmark():
    products=[];indexing=[]
    for label,polygon,elevation,height,source_sha in fixtures():
        source=Shape.from_polygon(polygon,elevation,tags=(label,)).extrude(height,'world.up');surface=Surface(source)
        edges=np.asarray([(a,b) for face in surface.faces for ring in (face.outer,*face.holes) for a,b in zip(ring,ring[1:]+ring[:1])],dtype=np.int64)
        if not np.array_equal(dictionary_edges(edges),numpy_edges(edges)):raise AssertionError('Unique-edge alternatives changed first-encounter order')
        for method,function in (('dictionary',dictionary_edges),('numpy-sort',numpy_edges)):
            indexing.append(dict(label=label,method=method,candidate_edges=len(edges),unique_edges=len(function(edges)),**timed(lambda:function(edges))))
        for kind in ('e','fe','v','fv'):
            children=source.comp(kind);meshes=[child.emit() for child in children]
            products.append(dict(label=label,kind=kind,components=len(children),source_sha256=source_sha,
                raw_mesh_bytes=sum(array.nbytes for mesh in meshes for array in mesh.values()),**timed(lambda:source.comp(kind))))
    return dict(products=products,indexing_alternatives=indexing)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/wire-benchmark.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',warm_runs=21,openblas_threads=os.environ.get('OPENBLAS_NUM_THREADS'),**benchmark(),
        limits='Python primitive extraction and stable edge indexing; final emitted positions excluded; no native frame/GPU time or full metadata memory')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',products=len(record['products']),indexing_measurements=len(record['indexing_alternatives']))))


if __name__=='__main__':main()
