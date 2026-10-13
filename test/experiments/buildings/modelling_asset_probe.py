"""Compare one complete source decode with repeated decodes for equal asset placements."""

import argparse
import json
from pathlib import Path
import time

import numpy as np

from evidence import revision
from modelling_asset_cases import fixtures
from modelling_insert import Asset
from outshine_ref.buildings.modelling.scope import Scope
from modelling_shape import Shape


def placements(path,targets,reuse):
    asset=Asset.from_file(path) if reuse else None
    return tuple(target.insert(asset if reuse else Asset.from_file(path)) for target in targets)


def equal(first,second):
    for a,b in zip(first,second,strict=True):
        assert a.scope==b.scope and a.asset.import_info==b.asset.import_info
        assert a.geometry.polygons==b.geometry.polygons and a.geometry.material_defs==b.geometry.material_defs
        assert a.asset.resources==b.asset.resources
        for key,value in a.emit().items():assert np.array_equal(value,b.emit()[key]),key


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/asset-import-benchmark.json'))
    parser.add_argument('--pairs',type=int,default=11);parser.add_argument('--placements',type=int,default=24)
    args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True)
    obj,paths,_=fixtures(args.output.parent/'asset-benchmark-controls')
    base=Shape.from_mesh(dict(vertices=np.array([[0.,0.,0.],[1.,0.,0.],[0.,1.,1.]]),faces=np.array([[0,1,2]])))
    targets=tuple(Shape(base.geometry,Scope(position=(i*3.,0.,0.),size=(2.,3.,1.))) for i in range(args.placements))
    rows=[]
    for path in (obj,paths[0],paths[1]):
        equal(placements(path,targets,True),placements(path,targets,False));times={True:[],False:[]}
        for index in range(args.pairs):
            for reuse in (True,False) if index%2==0 else (False,True):
                start=time.perf_counter_ns();result=placements(path,targets,reuse)
                times[reuse].append((time.perf_counter_ns()-start)/1e6)
                assert len(result)==args.placements
        rows.append(dict(source=path.name,placements=args.placements,pairs=args.pairs,
            decode_once_ms=dict(zip(('p50','p95','p99'),np.percentile(times[True],[50,95,99]).tolist())),
            decode_each_ms=dict(zip(('p50','p95','p99'),np.percentile(times[False],[50,95,99]).tolist()))))
    record=revision()|dict(status='pass',cases=rows,scope='warm local-file decode, prepare and insert; one decode included in reuse path; target construction and final emit excluded',
        correctness='emitted fields, scopes, source metadata, materials and encoded texture resources equal',
        limits='small owned file controls, no cold disk, external OSM asset, native cache, GPU or frame timing')
    args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(dict(status='pass',cases=rows)))


if __name__=='__main__':main()
