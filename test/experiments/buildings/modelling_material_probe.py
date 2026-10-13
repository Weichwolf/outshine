"""Compare shared shape material overrides with baking the same values into geometry."""

import argparse
import json
from pathlib import Path
import time

import numpy as np

from evidence import revision
from modelling_material import canonical
from modelling_insert import Asset
from modelling_scope_probe import cga_mesh
from modelling_shape import Shape


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/material-benchmark.json'))
    args=parser.parse_args();root=Path(__file__).resolve().parents[3]/'build/experiments/buildings/opening-inspections';rows=[]
    for label in ('rectangle','vienna-compact','vienna-courtyard','tokyo-compact'):
        with np.load(root/('opening-'+label+'-mesh.npz'),allow_pickle=False) as data:
            source=Shape.from_mesh(cga_mesh(dict(data)))
        def override():return source.color('#6f929c').set_material({'roughness':.45,'metallic':.1})
        def baked():return Asset.from_shape(override())
        expected=override().resolved_materials();asset=baked()
        actual=dict(asset.geometry.material_defs)
        assert all(canonical(actual[key])==canonical(value) for key,value in expected.items())
        timings={'override':[],'baked':[]}
        for call in (override,baked):call()
        for index in range(21):
            for name,call in [('override',override),('baked',baked)] if index%2==0 else [('baked',baked),('override',override)]:
                start=time.perf_counter_ns();result=call();timings[name].append((time.perf_counter_ns()-start)/1e6)
                if name=='override':assert result.geometry is source.geometry
        rows.append(dict(label=label,vertices=len(source.geometry.unit_vertices),triangles=len(source.geometry.fields['faces']),pairs=21,
            override_ms=dict(zip(('p50','p95','p99'),np.percentile(timings['override'],[50,95,99]).tolist())),
            baked_ms=dict(zip(('p50','p95','p99'),np.percentile(timings['baked'],[50,95,99]).tolist())),resolved_values_equal=True))
    record=revision()|dict(status='pass',cases=rows,decision='Retain shared shape overrides until asset preparation needs resolved geometry materials',
        limits='Same core resolved values on four cached meshes, warm Python/two OpenBLAS threads; source construction, final emission, shader binding and frames excluded')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',cases=rows)))


if __name__=='__main__':main()
