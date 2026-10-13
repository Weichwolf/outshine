"""Compare GEOS batching on identical edge sweep topology and inherited attributes."""

import argparse
import json
import os
from pathlib import Path
import time
from unittest.mock import patch

import numpy as np

from evidence import revision
from outshine_ref.buildings.modelling import faces as modelling_faces
from outshine_ref.buildings.modelling.components import Branch
from modelling_extrude_probe import fixtures,individual
from outshine_ref.buildings.modelling.shape import Shape
from verify_modelling_extrude import near


def benchmark():
    rows=[];batch=modelling_faces.triangulations
    for label,polygon,elevation,_,source_sha in fixtures():
        source=Shape.from_polygon(polygon,elevation,tags=(label,))
        for kind in ('e','fe'):
            wire=source.comp(kind,Branch('all','='))[0]
            for mode in ('face.normal','vertex.normal'):
                products={}
                for method,triangulate in (('batched',batch),('individual',individual)):
                    with patch.object(modelling_faces,'triangulations',triangulate):
                        products[method]=wire.extrude(.6,mode);call=lambda:wire.extrude(.6,mode).emit();call();times=[]
                        for _ in range(21):
                            began=time.perf_counter_ns();mesh=call();times.append((time.perf_counter_ns()-began)/1e6)
                    rows.append(dict(label=label,kind=kind,mode=mode,method=method,source_sha256=source_sha,edges=len(wire.geometry.edges),
                                     faces=len(products[method].geometry.polygons),triangles=len(mesh['faces']),raw_mesh_bytes=sum(a.nbytes for a in mesh.values()),
                                     p50_ms=float(np.percentile(times,50)),p95_ms=float(np.percentile(times,95))))
                a,b=(products[method] for method in ('batched','individual'))
                if a.geometry.polygons!=b.geometry.polygons or a.comp_info!=b.comp_info or a.sweep_info!=b.sweep_info:
                    raise AssertionError('Triangulation alternatives changed edge topology, UVs, materials, tags or diagnostics')
                near(a.scope.matrix,b.scope.matrix)
                for key,value in a.emit().items():near(value,b.emit()[key])
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/edge-extrusion-benchmark.json'))
    args=parser.parse_args();record=revision()|dict(status='pass',measurements=benchmark(),warm_runs=21,openblas_threads=os.environ.get('OPENBLAS_NUM_THREADS'),
        limits='Complete Python edge sweep and final mesh output; source construction/extraction excluded; no native frame/GPU time or full logical metadata memory')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',measurements=len(record['measurements']))))


if __name__=='__main__':main()
