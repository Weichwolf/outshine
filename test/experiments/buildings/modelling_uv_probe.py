"""Compare a composed UV affine edit with three equal edits on actual provider footprints."""

import argparse
import json
from pathlib import Path
import time

import numpy as np

from evidence import revision
from modelling_extrude_probe import fixtures
from modelling_shape import Shape


ANGLE=13.
RADIANS=np.deg2rad(ANGLE)
MATRIX=np.array([[np.cos(RADIANS),-np.sin(RADIANS)],[np.sin(RADIANS),np.cos(RADIANS)]])@np.diag([1.25,-.75])
OFFSET=(.2,.3)


def separate(shape):return shape.scale_uv(0,1.25,-.75).rotate_uv(0,ANGLE).translate_uv(0,*OFFSET)


def combined(shape):return shape.transform_uv(0,MATRIX,OFFSET)


def equal(a,b):
    assert a.scope==b.scope and a.geometry.group_names==b.geometry.group_names and a.geometry.material_defs==b.geometry.material_defs
    for key,value in a.emit().items():assert np.allclose(value,b.emit()[key],atol=1e-12,rtol=1e-12),key
    for x,y in zip(a.geometry.polygons,b.geometry.polygons,strict=True):
        assert x.outer==y.outer and x.holes==y.holes and x.tags==y.tags and x.material==y.material and x.group==y.group
        for u,v in zip(x.uvs,y.uvs,strict=True):assert np.allclose(u,v,atol=1e-12,rtol=1e-12)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/uv-benchmark.json'));args=parser.parse_args();rows=[]
    for label,polygon,elevation,_,_ in fixtures():
        source=Shape.from_polygon(polygon,elevation);equal(separate(source),combined(source));timings={'composed':[],'separate':[]}
        for index in range(21):
            for name,call in [('composed',combined),('separate',separate)] if index%2==0 else [('separate',separate),('composed',combined)]:
                start=time.perf_counter_ns();result=call(source);timings[name].append((time.perf_counter_ns()-start)/1e6)
                assert result.geometry.unit_vertices is source.geometry.unit_vertices
        rows.append(dict(label=label,vertices=len(source.geometry.unit_vertices),logical_faces=len(source.geometry.polygons),pairs=21,
            composed_ms=dict(zip(('p50','p95','p99'),np.percentile(timings['composed'],[50,95,99]).tolist())),
            separate_ms=dict(zip(('p50','p95','p99'),np.percentile(timings['separate'],[50,95,99]).tolist()))))
    record=revision()|dict(status='pass',cases=rows,scope='three affine UV edits, native UV stream plus logical corner values; same emitted geometry/UVs and source ownership',
        limits='small pinned provider footprints plus rectangular control, warm Python, two OpenBLAS threads; construction, final emission, GPU and frame timing excluded')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',cases=rows)))


if __name__=='__main__':main()
