"""Measure value/group/hole splitting and avoid geometric work for name queries."""

import argparse
from dataclasses import replace
import json
import os
from pathlib import Path
import time
from unittest.mock import patch

import numpy as np

from evidence import revision
from modelling_component_probe import rebuilt
from modelling_extrude_probe import fixtures
from outshine_ref.buildings.modelling import group_components as modelling_group_components
from outshine_ref.buildings.modelling.metadata import Material
from outshine_ref.buildings.modelling.shape import Shape
from verify_modelling_groups import signatures
from verify_modelling_extrude import near


def authored(body):
    faces=[]
    for face in body.geometry.polygons:
        group,material=(1,9) if 'extrude.top' in face.tags else (2,28) if 'extrude.side.inner' in face.tags else (0,17)
        faces.append(replace(face,group=group,material=material))
    facade=Material('facade',{'color':(.3,.5,.6),'opacity':1.})
    return Shape.from_faces(body.emit()['vertices'],faces,body.scope).with_metadata(
        {0:'shell',1:'roof',2:'courtyard'},{9:Material('roof',{'color':(.4,.4,.4)}),17:facade,28:facade})


def timed(call):
    call();times=[]
    for _ in range(21):
        start=time.perf_counter_ns();call();times.append((time.perf_counter_ns()-start)/1e6)
    return dict(p50_ms=float(np.percentile(times,50)),p95_ms=float(np.percentile(times,95)))


def benchmark():
    rows=[];queries=[];reuse=modelling_group_components.component_shape
    for label,polygon,elevation,height,source_sha in fixtures():
        source=Shape.from_polygon(polygon,elevation,tags=(label,));body=authored(source.extrude(height,'world.up'))
        for kind in ('g','m','h'):
            target=source if kind=='h' else body;products={}
            for method,constructor in [('reuse-native-triangles',reuse),('rebuild-with-geos',rebuilt)]:
                with patch.object(modelling_group_components,'component_shape',constructor):
                    products[method]=target.comp(kind);cost=timed(lambda:target.comp(kind))
                meshes=[child.emit() for child in products[method]]
                rows.append(dict(label=label,kind=kind,method=method,components=len(meshes),source_sha256=source_sha,
                                 triangles=sum(len(mesh['faces']) for mesh in meshes),raw_mesh_bytes=sum(a.nbytes for mesh in meshes for a in mesh.values()),**cost))
            a,b=products.values()
            if len(a)!=len(b):raise AssertionError('Component alternatives changed group/hole cardinality')
            for x,y in zip(a,b,strict=True):
                if signatures(x.emit())!=signatures(y.emit()) or x.geometry.polygons!=y.geometry.polygons:
                    raise AssertionError('Component alternatives changed geometry, UVs or source group/material identities')
                if x.geometry.group_names!=y.geometry.group_names or x.geometry.material_defs!=y.geometry.material_defs or x.comp_info!=y.comp_info:
                    raise AssertionError('Component alternatives lost names, material values or predecessor attributes')
                near(x.scope.matrix,y.scope.matrix)
        for kind in ('g','m'):
            cheap=lambda:modelling_group_components.names(body,kind)
            costly=lambda:tuple(record.name for record in modelling_group_components.GroupSet(body,kind).records)
            if cheap()!=costly():raise AssertionError('Name lookup changed component index order')
            for method,call in [('metadata-only',cheap),('full-geometric-context',costly)]:
                queries.append(dict(label=label,kind=kind,method=method,names=len(call()),**timed(call)))
    return rows,queries


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/group-benchmark.json'))
    args=parser.parse_args();rows,queries=benchmark();record=revision()|dict(status='pass',measurements=rows,name_queries=queries,warm_runs=21,
        openblas_threads=os.environ.get('OPENBLAS_NUM_THREADS'),limits='Python source context, selection, frames and payload construction; final emitted positions excluded; no native frame/GPU time or full logical metadata memory')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
    print(json.dumps(dict(status='pass',component_measurements=len(rows),name_measurements=len(queries))))


if __name__=='__main__':main()
