"""Measure shared placement against equivalent per-placement geometry materialization."""

import argparse
from dataclasses import replace
import gc
import json
import os
from pathlib import Path
import platform
import time
import tracemalloc

import numpy as np

from evidence import revision
from modelling_insert import Asset
from modelling_insert_cases import block,frame
from modelling_scope import Scope
from modelling_shape import Shape
from verify_modelling_extrude import near


def parents(source,count):
    return tuple(replace(source,scope=Scope(position=(i%20*1.5,i//20*1.8,0.),size=(1.2,1.6,0.))) for i in range(count))


def products(targets,asset,shared):
    result=[]
    for target in targets:
        child=target.insert(asset)
        if not shared:
            child=Shape.from_mesh(child.emit(),child.scope,child.geometry.polygons,child.geometry.edges,child.geometry.points,
                                  child.geometry.group_names,child.geometry.material_defs)
        result.append(child)
    return tuple(result)


def benchmark():
    rows=[]
    for label,source in (('attributed-block',block()),('hollow-frame',frame())):
        asset=Asset.from_shape(source,label)
        for count in (100,1000):
            targets=parents(source,count);versions={};samples={'shared':[],'materialized':[]}
            for method in samples:versions[method]=products(targets,asset,method=='shared')
            for a,b in zip(versions['shared'],versions['materialized'],strict=True):
                one,two=a.emit(),b.emit();assert one.keys()==two.keys()
                for key in one:near(one[key],two[key])
                assert a.geometry.polygons==b.geometry.polygons
                assert a.geometry.group_names==b.geometry.group_names and a.geometry.material_defs==b.geometry.material_defs
            del versions
            for run in range(21):
                methods=('shared','materialized') if run%2==0 else ('materialized','shared')
                for method in methods:
                    start=time.perf_counter_ns();product=products(targets,asset,method=='shared')
                    samples[method].append((time.perf_counter_ns()-start)/1e6);del product
            for method,values in samples.items():
                gc.collect();tracemalloc.start();product=products(targets,asset,method=='shared');retained,peak=tracemalloc.get_traced_memory();tracemalloc.stop()
                geometries={id(child.geometry):child.geometry for child in product}
                arrays={id(array):array for geometry in geometries.values() for array in (geometry.unit_vertices,*geometry.fields.values())}
                rows.append(dict(label=label,count=count,method=method,unique_geometries=len(geometries),
                    unique_native_array_bytes=sum(array.nbytes for array in arrays.values()),new_retained_bytes=retained,new_peak_bytes=peak,
                    p50_ms=float(np.percentile(values,50)),p95_ms=float(np.percentile(values,95))))
                del product
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/insert-benchmark.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',measurements=benchmark(),warm_runs=21,host=platform.platform(),
        openblas_threads=os.environ.get('OPENBLAS_NUM_THREADS'),sample_order='Alternating shared/materialized pairs',
        limits='Prepared asset and target scopes excluded from timings and traced allocation; materialized alternative emits and copies equivalent geometry. Unique arrays counted once including shared source. No GPU instances, frames or complete process memory.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',measurements=len(record['measurements']))))


if __name__=='__main__':main()
